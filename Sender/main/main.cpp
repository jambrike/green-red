extern "C"
{
#include <stdio.h>
#include <string.h>
#include "esp_camera.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h" // Added for PSRAM allocation
}

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "person_detect_model_data.h"

// ESP-NOW receiver MAC address
uint8_t peer_mac[] = {0x44, 0x1D, 0x64, 0xF6, 0xD9, 0xF0};

// Camera pins for ESP32-CAM (AI-Thinker)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    5

#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM    13

// TensorFlow Lite settings
// Increased to 128KB to prevent "Failed to resize buffer" 
constexpr int kTensorArenaSize = 128 * 1024; 
static uint8_t *tensor_arena = nullptr; // Changed to pointer for PSRAM allocation
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input = nullptr;

// Movement detection thresholds
#define MOVEMENT_THRESHOLD 0.2
#define PERSON_CONFIDENCE_MIN 0.5 
#define DETECTION_COOLDOWN_MS 500 
#define SCORE_HISTORY_SIZE 5      

// Game state
bool game_active = false;
float score_history[SCORE_HISTORY_SIZE] = {0};
int score_index = 0;
int64_t last_detection_time = 0;
bool red_light = false; 

// Function to initialize camera
esp_err_t init_camera()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size = FRAMESIZE_96X96; 
    config.jpeg_quality = 12;
    config.fb_count = 1;

    // The driver automatically uses PSRAM if available [cite: 12]
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        printf("Camera init failed with error 0x%x\n", err);
        return err;
    }
    return ESP_OK;
}

// Function to initialize TensorFlow Lite
bool init_tflite()
{
    // 1. Allocate Tensor Arena in PSRAM 
    tensor_arena = (uint8_t *)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (tensor_arena == nullptr)
    {
        printf("Failed to allocate TFLite arena in PSRAM!\n");
        return false;
    }

    const tflite::Model *model = tflite::GetModel(g_person_detect_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        printf("Model version mismatch!\n");
        return false;
    }

    static tflite::MicroMutableOpResolver<5> micro_op_resolver;
    micro_op_resolver.AddAveragePool2D();
    micro_op_resolver.AddConv2D();
    micro_op_resolver.AddDepthwiseConv2D();
    micro_op_resolver.AddReshape();
    micro_op_resolver.AddSoftmax();

    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        printf("AllocateTensors() failed after moving to PSRAM\n");
        return false;
    }

    input = interpreter->input(0);
    printf("TFLite initialized successfully in PSRAM\n");
    return true;
}

// Send ESP-NOW command
void send_command(const char *cmd)
{
    esp_err_t result = esp_now_send(peer_mac, (uint8_t *)cmd, strlen(cmd));
    if (result == ESP_OK)
    {
        printf("Sent: %s\n", cmd);
    }
    else
    {
        printf("Error sending: %s\n", cmd);
    }
}

// Process camera frame and detect movement
void process_frame()
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        printf("Camera capture failed\n");
        return;
    }

    for (int i = 0; i < input->bytes; i++)
    {
        input->data.int8[i] = fb->buf[i] - 128; 
    }

    esp_camera_fb_return(fb);

    if (interpreter->Invoke() != kTfLiteOk)
    {
        printf("Invoke failed\n");
        return;
    }

    TfLiteTensor *output = interpreter->output(0);
    int8_t person_score_int8 = output->data.int8[1]; 
    float person_score = (person_score_int8 + 128) / 255.0f;

    score_history[score_index] = person_score;
    score_index = (score_index + 1) % SCORE_HISTORY_SIZE;

    printf("Person score: %.2f | Red Light: %s\n", person_score, red_light ? "YES" : "NO");

    if (red_light && person_score > PERSON_CONFIDENCE_MIN)
    {
        float avg_change = 0;
        int valid_comparisons = 0;

        for (int i = 1; i < SCORE_HISTORY_SIZE; i++)
        {
            int prev_idx = (score_index - i - 1 + SCORE_HISTORY_SIZE) % SCORE_HISTORY_SIZE;
            int curr_idx = (score_index - i + SCORE_HISTORY_SIZE) % SCORE_HISTORY_SIZE;

            if (score_history[prev_idx] > 0.1 && score_history[curr_idx] > 0.1)
            {
                avg_change += fabsf(score_history[curr_idx] - score_history[prev_idx]);
                valid_comparisons++;
            }
        }

        if (valid_comparisons > 0)
        {
            avg_change /= valid_comparisons;
        }

        int64_t current_time = esp_timer_get_time() / 1000; 
        bool cooldown_passed = (current_time - last_detection_time) > DETECTION_COOLDOWN_MS;

        if (avg_change > MOVEMENT_THRESHOLD && cooldown_passed)
        {
            printf("🚨 MOVEMENT DETECTED! Avg change: %.3f\n", avg_change);
            send_command("ON"); 
            last_detection_time = current_time;

            vTaskDelay(pdMS_TO_TICKS(1500));

            if (red_light)
            {
                send_command("OFF"); 
            }
        }
    }
}

void toggle_light()
{
    red_light = !red_light;
    printf("Light changed to: %s\n", red_light ? "RED (STOP)" : "GREEN (GO)");

    if (red_light)
    {
        send_command("OFF"); 
    }
    else
    {
        send_command("ON"); 
        for (int i = 0; i < SCORE_HISTORY_SIZE; i++)
        {
            score_history[i] = 0;
        }
    }
}

void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    // Handle send status if needed
}

extern "C" void app_main(void)
{
    printf("Starting Person Detection + ESP-NOW\n");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_send_cb(on_data_sent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peer_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
    printf("ESP-NOW initialized\n");

    if (init_camera() != ESP_OK)
    {
        printf("Camera init failed!\n");
        return;
    }
    printf("Camera initialized\n");

    if (!init_tflite())
    {
        printf("TFLite init failed!\n");
        return;
    }

    printf("Starting Green Light, Red Light!\n");
    game_active = true;
    red_light = true;
    send_command("OFF"); 

    int cycle_count = 0;

    while (1)
    {
        process_frame();

        cycle_count++;
        if (cycle_count >= 50)
        { 
            toggle_light();
            cycle_count = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}