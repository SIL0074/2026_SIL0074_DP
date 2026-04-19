#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "DataManager.h"
#include "MqttManager.h"
#include "WebServer.h"
#include "WifiManager.h"
#include "SerialBridge.h"
#include "LedManager.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define TAG "MAIN"

void time_sync_task(void *pvParameters) {
    // odložení startu
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        if (timeinfo.tm_year > 120) {
            // výpočet času do nejbližší 10s hranice
            time_t next_pulse = ((now / 10) + 1) * 10;
            uint32_t wait_s = next_pulse - now;
            if (wait_s < 1) wait_s = 10;
            
            vTaskDelay(pdMS_TO_TICKS(wait_s * 1000));
            
            // fetching času (aby se odeslal přesný okamžik pro sync)
            time(&now);
            SerialBridge::getInstance().sendTime((int64_t)now);
            ESP_LOGI("TIME", "Synced time pulse emitted");
        } else {
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

void gateway_status_task(void *pvParameters) {
    while (1) {
        uint32_t interval = MqttManager::getInstance().publish_interval_s;
        if (interval < 5) interval = 5;
        vTaskDelay(pdMS_TO_TICKS(interval * 1000));
        MqttManager::getInstance().publishGatewayStatus();
    }
}

extern "C" void app_main(void) {
    printf("\n\n--- GATEWAY STARTING (Master Mode) ---\n\n");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    DataManager::getInstance().init();
    LedManager::getInstance().init();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    SerialBridge::getInstance().init();
    WifiManager::getInstance().init();   // WiFi musí být aktivní před MQTT | podstatné, opačně nestabilní
    MqttManager::getInstance().init();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    xTaskCreatePinnedToCore(time_sync_task, "time_sync", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(gateway_status_task, "gw_stat", 4096, NULL, 5, NULL, 1);

    vTaskDelay(pdMS_TO_TICKS(500));
    WebServer::init();

    ESP_LOGI(TAG, "Gateway Master ready.");
    
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);
    char line[256]; int pos = 0;
    while (true) {
        char c;
        if (read(0, &c, 1) > 0) {
            if (c == '\n' || c == '\r') {
                if (pos > 0) {
                    line[pos] = 0;
                    if (strncmp(line, "reboot", 6) == 0) esp_restart();
                    pos = 0;
                }
            } else if (pos < 255) line[pos++] = c;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
