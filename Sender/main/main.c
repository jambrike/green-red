#include <string.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"

uint8_t peer_mac[] = {0x44, 0x1D, 0x64, 0xF6, 0xD9, 0xF0};

void app_main(void) {
    // 1. Initialize NVS (Required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize WiFi and ESP-NOW
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_now_init());

    // 3. Add Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, peer_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    // 4. Send Loop
    while(1) {
        const char *msg = "ON";
        esp_now_send(peer_mac, (uint8_t *)msg, strlen(msg));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}