#include <esp_now.h>
#include <WiFi.h>

// For standard ESP32 DevKit, D4 is GPIO 4
#define LED_PIN 4 

// Callback for when data is received
void data_receive_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    char buf[16];
    int copy_len = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
    memcpy(buf, data, copy_len);
    buf[copy_len] = '\0';

    Serial.print("Received message: ");
    Serial.print(buf);
    Serial.print(" from MAC: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X%s", recv_info->src_addr[i], (i == 5) ? "" : ":");
    }
    Serial.println();

    if (strcmp(buf, "ON") == 0) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("GPIO 4 HIGH");
    } else if (strcmp(buf, "OFF") == 0) {
        digitalWrite(LED_PIN, LOW);
        Serial.println("GPIO 4 LOW");
    }
}

void setup() {
    Serial.begin(115200);

    // Setup GPIO
    pinMode(LED_PIN, OUTPUT);

    // Initialize WiFi in Station mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // We don't want to connect to an AP

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Register receive callback
    esp_now_register_recv_cb(data_receive_cb);

    Serial.println("ESP32 Receiver Started. Listening on GPIO 4...");
}

void loop() {
    // ESP-NOW is interrupt-based, so loop can stay empty
    delay(1000); 
}