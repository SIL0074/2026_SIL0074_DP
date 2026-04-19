#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "cJSON.h"
#include "mesh_common.h"

#define TAG "RECEIVER"
#define BRIDGE_UART_PORT UART_NUM_2
#define BRIDGE_TX_PIN 17
#define BRIDGE_RX_PIN 16
#define UART_BAUD 115200

uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint16_t my_id = 0;
uint8_t current_mesh_channel = ESP_NOW_CHANNEL;
uint8_t target_mesh_channel = ESP_NOW_CHANNEL;
static uint8_t previous_mesh_channel = ESP_NOW_CHANNEL; // kanál před migrací
static bool migration_active = false;

static void send_sync_packet(ota_sync_payload_t *ota_p, uint8_t channel) {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    time_t now; time(&now);
    esp_now_packet_t p; memset(&p, 0, sizeof(p));
    p.version = 1; p.role = ROLE_GATEWAY; p.msg_type = MSG_TIME_SYNC; 
    p.unix_time = (uint64_t)now;
    p.hop_count = 0; 
    p.device_id = my_id;
    p.current_wifi_channel = target_mesh_channel; // vždy hlásíme cílový kanál
    
    if (ota_p && ota_p->slot_count > 0) {
        size_t ota_len = sizeof(uint8_t) + (ota_p->slot_count * sizeof(ota_config_slot_t));
        if (ota_len <= sizeof(p.payload)) {
            memcpy(p.payload, ota_p, ota_len);
            p.payload_len = ota_len;
        }
    }
    esp_now_send(broadcast_mac, (uint8_t *) &p, sizeof(p));
}

void broadcast_time(ota_sync_payload_t *ota_p) {
    ESP_LOGI(TAG, "Broadcasting pulse burst (Items:%d)", ota_p ? ota_p->slot_count : 0);
    
    // vysílání burstu impulzů rozprostřený do 2,5 / 3 sekund na pokrytí probouzecích ID offsetů 
    // místo jednoho úzkého pulzu, který senzory v offsetu minou, chytí z burstu zaručeně alespoň jeden | S TÍMTO TO FUNGUJE
    for (int i = 0; i < 6; i++) {
        if (migration_active && target_mesh_channel != previous_mesh_channel) {
            if (i == 0) {
                ESP_LOGW(TAG, "MIGRATION BURST: Sending to Ch %d (old) and Ch %d (new)", previous_mesh_channel, target_mesh_channel);
                char burst_msg[128];
                snprintf(burst_msg, sizeof(burst_msg), "{\"type\":\"LOG\",\"msg\":\"MIGRATION BURST: Ch %d -> %d\"}\n", previous_mesh_channel, target_mesh_channel);
                uart_write_bytes(BRIDGE_UART_PORT, burst_msg, strlen(burst_msg));
            }
            send_sync_packet(ota_p, previous_mesh_channel);
            vTaskDelay(pdMS_TO_TICKS(50));
            send_sync_packet(ota_p, target_mesh_channel);
        } else {
            migration_active = false; 
            current_mesh_channel = target_mesh_channel;
            send_sync_packet(ota_p, current_mesh_channel);
        }
        
        // vkládá 500ms prodlevu
        if (i < 5) vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // aktuální kanál nasadí na cíl natrvalo, aby poslech mezi okny byl už na novém kanálu
    current_mesh_channel = target_mesh_channel; 
}

static void esp_now_recv_cb(const esp_now_recv_info_t * info, const uint8_t *data, int len) {
    if (len != sizeof(esp_now_packet_t)) return;
    esp_now_packet_t *p = (esp_now_packet_t *)data;

    // RSSI info
    int8_t src_rssi = info->rx_ctrl->rssi;
    int8_t link_rssi = src_rssi;

    if (p->msg_type == MSG_PAIRING_REQ) {
        esp_now_packet_t resp = { .version = 1, .role = ROLE_GATEWAY, .msg_type = MSG_PAIRING_RESP, .device_id = my_id, .unix_time = (uint64_t)time(NULL) };
        resp.current_wifi_channel = current_mesh_channel;
        esp_now_peer_info_t peer = { .channel = 0, .encrypt = false }; 
        memcpy(peer.peer_addr, info->src_addr, 6);
        if (!esp_now_is_peer_exist(info->src_addr)) esp_now_add_peer(&peer);
        esp_now_send(info->src_addr, (uint8_t *)&resp, sizeof(resp));
        return;
    }

    if (p->msg_type == MSG_DATA) {
        static char hex_payload[130];
        for (int i = 0; i < p->payload_len && i < 64; i++) {
            sprintf(hex_payload + (i * 2), "%02X", p->payload[i]);
        }
        hex_payload[p->payload_len * 2] = 0;

        // pokud jde o přeposlaný paket, 'rssi' je signál k uplinku (p->rssi)
        // pokud je to přímý paket, 'rssi' je signál přímo k bráně (src_rssi)
        int final_rssi = (p->hop_count > 0) ? p->rssi : src_rssi;

        char buffer[512];
        snprintf(buffer, sizeof(buffer), 
            "{\"type\":\"DATA\",\"id\":\"%04X\",\"relay\":\"%04X\",\"role\":%d,\"stype\":%d,\"seq\":%lu,\"ts\":%llu,\"batt\":%.2f,\"rssi\":%d,\"lrssi\":%d,\"hops\":%d,\"active\":%lu,\"sleep_int\":%lu,\"safe\":%d,\"channel\":%d,\"plen\":%d,\"payload\":\"%s\"}\n",
            p->device_id, p->relay_id, (int)p->role, (int)p->sensor_type, (unsigned long)p->packet_seq, (unsigned long long)p->unix_time,
            p->batt_v, final_rssi, (int)src_rssi, (int)p->hop_count, (unsigned long)p->active_time_us, 
            (unsigned long)p->current_sleep_interval, (int)p->current_safe_chunk, (int)p->current_wifi_channel,
            (int)p->payload_len, hex_payload);
        uart_write_bytes(BRIDGE_UART_PORT, buffer, strlen(buffer));
    }
}

static void heartbeat_task(void *pvParameters) {
    while (1) {
        char hb[64];
        snprintf(hb, sizeof(hb), "{\"type\":\"HEARTBEAT\",\"ch\":%d,\"mig\":%d}\n", 
                 current_mesh_channel, migration_active ? 1 : 0);
        uart_write_bytes(BRIDGE_UART_PORT, hb, strlen(hb));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void process_json_line(const char* line) {
    cJSON *root = cJSON_Parse(line);
    if (!root) {
        ESP_LOGE(TAG, "JSON Parse Error: %s", line);
        return;
    }
    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type && strcmp(type->valuestring, "TIME") == 0) {
        cJSON *v = cJSON_GetObjectItem(root, "val");
        if(v) { 
            time_t t = (time_t)v->valuedouble;
            struct timeval tv = { .tv_sec = t, .tv_usec = 0 }; 
            settimeofday(&tv, NULL); 
            
            ota_sync_payload_t ota = {0};
            cJSON *ota_arr = cJSON_GetObjectItem(root, "ota");
            migration_active = false;
            if (ota_arr && cJSON_IsArray(ota_arr)) {
                int size = cJSON_GetArraySize(ota_arr);
                if (size > OTA_MAX_SLOTS) size = OTA_MAX_SLOTS; // Limit dle mesh_common.h
                ota.slot_count = size;
                for (int i = 0; i < size; i++) {
                    cJSON *item = cJSON_GetArrayItem(ota_arr, i);
                    cJSON *id = cJSON_GetObjectItem(item, "id");
                    cJSON *interval = cJSON_GetObjectItem(item, "int");
                    cJSON *safe = cJSON_GetObjectItem(item, "safe");
                    cJSON *ch = cJSON_GetObjectItem(item, "ch");
                    if (id && interval && ch) {
                        uint16_t tid = (uint16_t)strtoul(id->valuestring, NULL, 16);
                        ota.slots[i].target_id = tid;
                        ota.slots[i].sleep_interval = (uint32_t)interval->valueint;
                        ota.slots[i].channel = (uint8_t)ch->valueint;
                        if (safe) ota.slots[i].safe_chunk_s = (uint16_t)safe->valueint;
                        
                        if (tid == 0xFFFF && ota.slots[i].channel > 0) {
                            if (target_mesh_channel != ota.slots[i].channel) {
                                previous_mesh_channel = current_mesh_channel;
                            }
                            target_mesh_channel = ota.slots[i].channel;
                            migration_active = (target_mesh_channel != previous_mesh_channel);
                        }
                    }
                }
            }
            
            cJSON *mch = cJSON_GetObjectItem(root, "mch");
            if (mch && cJSON_IsNumber(mch)) {
                uint8_t master_ch = (uint8_t)mch->valueint;
                if (!migration_active) {
                    target_mesh_channel = master_ch;
                }
            }
            
            broadcast_time(&ota); 
        }
    }
    cJSON_Delete(root);
}

static void master_link_task(void *pvParameters) {
    uint8_t* data = (uint8_t*) malloc(1024);
    char line_buffer[1024];
    int line_pos = 0;
    while (1) {
        int len = uart_read_bytes(BRIDGE_UART_PORT, data, 1023, 20 / portTICK_PERIOD_MS);
        for (int i = 0; i < len; i++) {
            if (data[i] == '\n' || data[i] == '\r') {
                if (line_pos > 0) {
                    line_buffer[line_pos] = 0;
                    process_json_line(line_buffer);
                    line_pos = 0;
                }
            } else if (line_pos < 1023) {
                line_buffer[line_pos++] = data[i];
            }
        }
    }
    free(data);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init(); esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM); esp_wifi_set_mode(WIFI_MODE_STA); esp_wifi_start();
    esp_wifi_set_channel(ESP_NOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_now_init(); esp_now_register_recv_cb(esp_now_recv_cb);
    // nastavením channel = 0 zajistí, že se broadcast odešle vždy na aktuálně aktivním Wi-Fi kanálu!
    // původní hodnota (ESP_NOW_CHANNEL = 11) zlobila ESP-NOW driver a nutila ho během odesílání přeladit PHY zpět na 11 i při migraci | TAKTO MI TO FUNGUJE
    esp_now_peer_info_t bcast = { .channel = 0, .encrypt = false }; 
    memcpy(bcast.peer_addr, broadcast_mac, 6); 
    esp_now_add_peer(&bcast);

    uint8_t mac[6]; esp_wifi_get_mac(WIFI_IF_STA, mac);
    my_id = (mac[4] << 8) | mac[5];
    ESP_LOGI(TAG, "Receiver ID: %04X", my_id);

    uart_config_t uc; memset(&uc, 0, sizeof(uc));
    uc.baud_rate=115200; uc.data_bits=UART_DATA_8_BITS; uc.parity=UART_PARITY_DISABLE; uc.stop_bits=UART_STOP_BITS_1; uc.flow_ctrl=UART_HW_FLOWCTRL_DISABLE; uc.source_clk=UART_SCLK_DEFAULT;
    
    // 1. nastav parametry
    uart_param_config(BRIDGE_UART_PORT, &uc);
    // 2. nastav piny
    uart_set_pin(BRIDGE_UART_PORT, BRIDGE_TX_PIN, BRIDGE_RX_PIN, -1, -1);
    // 3. nainstaluj driver 
    // zvětšeno na 4096 bytů pro vyšší stabilitu při burstu paketů, menší hodnoty zlobily
    uart_driver_install(BRIDGE_UART_PORT, 4096, 4096, 0, NULL, 0);

    xTaskCreatePinnedToCore(master_link_task, "master_link", 8192, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(heartbeat_task, "hb_task", 2048, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "Receiver Ready (Dual-Core Bridge Mode)");
}
