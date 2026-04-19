#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "mesh_common.h"

#define TAG "BUTTON_TEST"
#define BUTTON_GPIO GPIO_NUM_9 

static uint32_t seq = 0;
static uint16_t my_id = 0xBBBB;
static uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void send_button_packet() {
    esp_now_packet_t p;
    memset(&p, 0, sizeof(p));
    
    p.version = 1;
    p.role = ROLE_SENSOR;
    p.msg_type = MSG_DATA;
    p.sensor_type = SENSE_GENERIC;
    p.device_id = my_id;
    p.relay_id = 0x0000; 
    p.packet_seq = seq++;
    p.batt_v = 3.3f;
    p.current_wifi_channel = ESP_NOW_CHANNEL;
    p.payload_len = 1;
    p.payload[0] = 0x01; 
    
    esp_err_t result = esp_now_send(broadcast_mac, (uint8_t *)&p, sizeof(p));
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Sent Button Packet (Seq: %lu)", p.packet_seq);
    } else {
        ESP_LOGE(TAG, "Send Failed: %d", result);
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    
    esp_now_peer_info_t broadcast_peer = {
        .channel = 0, 
        .encrypt = false
    };
    memcpy(broadcast_peer.peer_addr, broadcast_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&broadcast_peer));

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Button Test Ready. Press BOOT button (GPIO9) to send packet.");

    int last_state = 1;
    while (1) {
        int current_state = gpio_get_level(BUTTON_GPIO);
        if (last_state == 1 && current_state == 0) {
            send_button_packet();
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
