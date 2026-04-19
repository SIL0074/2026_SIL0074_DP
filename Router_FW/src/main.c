#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "freertos/semphr.h"
#include "mesh_common.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define TAG "ROUTER"
#define MAX_HOPS 3
#define SYNC_INTERVAL_S 10
#define WAKE_WINDOW_MS 5000 // zvětšeno z 3500 na 5000 ms pro pokrytí offsetů
#define MAX_TRACKED_SENSORS 10
#define SEND_RETRIES 3
#define MIN_DIRECT_RSSI -85  // minimální RSSI pro přímý sync od GW (vynucení multi-hop / testování)
#define MAX_SAFE_CHUNK_S 60  // maximální bezpečný spánek bez rekalibrace (u směrovače lepší, protože používá externí oscilátor)

// měření napětí baterie: odporový dělič 100k+100k na GPIO34 (ADC1_CH6) | NUTNO ZMĚNIT PŘI JINÝCH HODNOTÁCH!
// maximální napětí baterie 4.2V -> na pinu ADC 2.1V -> bezpečně v rozsahu ADC_ATTEN_DB_12
#define BATT_ADC_CHANNEL    ADC_CHANNEL_6   // GPIO34 = ADC1_CH6
#define BATT_ADC_UNIT       ADC_UNIT_1
#define BATT_DIVIDER_RATIO  2.0f            // Dělič 1:2 (100k + 100k)

static float read_battery_voltage(void) {
    // --- inicializace ADC ---
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = BATT_ADC_UNIT };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_handle, BATT_ADC_CHANNEL, &chan_cfg);

    // --- průměr z 64 vzorků --- menší hodnoty (8,16,32) dělaly měření nepřesným
    int raw_sum = 0;
    for (int i = 0; i < 64; i++) {
        int raw = 0;
        adc_oneshot_read(adc_handle, BATT_ADC_CHANNEL, &raw);
        raw_sum += raw;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    int raw_avg = raw_sum / 64;
    ESP_LOGI(TAG, "BATT ADC raw avg: %d", raw_avg);

    float v_pin = 0.0f;
    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = BATT_ADC_UNIT,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    bool calibrated = (adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle) == ESP_OK);

    if (calibrated) {
        int mv = 0;
        adc_cali_raw_to_voltage(cali_handle, raw_avg, &mv);
        v_pin = mv / 1000.0f;
        adc_cali_delete_scheme_line_fitting(cali_handle);
        ESP_LOGI(TAG, "BATT pin (calibrated): %.3f V", v_pin);
    } else {
        v_pin = (raw_avg / 4095.0f) * 3.1f;
        ESP_LOGW(TAG, "BATT pin (uncalibrated fallback): %.3f V", v_pin);
    }

    adc_oneshot_del_unit(adc_handle);
    return v_pin * BATT_DIVIDER_RATIO;
}



RTC_DATA_ATTR uint32_t rtc_sync_interval_s = SYNC_INTERVAL_S;
RTC_DATA_ATTR uint16_t rtc_safe_chunk_s = MAX_SAFE_CHUNK_S;
RTC_DATA_ATTR uint8_t  rtc_wifi_channel = ESP_NOW_CHANNEL;

typedef struct {
    uint16_t id;
    int64_t last_seen;
} tracked_sensor_t;

RTC_DATA_ATTR bool is_synced = false;
RTC_DATA_ATTR uint8_t rtc_gateway_mac[6] = {0};
RTC_DATA_ATTR bool rtc_gateway_valid = false;
RTC_DATA_ATTR int8_t rtc_gw_rssi = -127;
RTC_DATA_ATTR uint32_t rtc_heartbeat_seq = 0;
RTC_DATA_ATTR uint16_t rtc_uplink_id = 0;
RTC_DATA_ATTR uint8_t rtc_uplink_hop = 255;
RTC_DATA_ATTR tracked_sensor_t tracked_sensors[MAX_TRACKED_SENSORS] = {0};

uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint16_t my_id = 0;
QueueHandle_t forward_queue;
QueueHandle_t sync_forward_queue;
static SemaphoreHandle_t send_sem;
volatile bool last_send_ok = false;

static void send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    last_send_ok = (status == ESP_NOW_SEND_SUCCESS);
    xSemaphoreGive(send_sem);
}

static bool send_with_retry(const uint8_t *peer, const uint8_t *data, size_t len) {
    for (int attempt = 0; attempt < SEND_RETRIES; attempt++) {
        esp_now_send(peer, data, len);
        if (xSemaphoreTake(send_sem, pdMS_TO_TICKS(200)) == pdTRUE && last_send_ok) {
            return true;
        }
        if (attempt < SEND_RETRIES - 1) vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}

static void wifi_init(void) {
    esp_netif_init(); esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM); esp_wifi_set_mode(WIFI_MODE_STA); esp_wifi_start();
    esp_wifi_set_channel(rtc_wifi_channel, WIFI_SECOND_CHAN_NONE);
}

static void track_sensor(uint16_t id) {
    int64_t now = esp_timer_get_time() / 1000000;
    int empty_slot = -1;
    for (int i = 0; i < MAX_TRACKED_SENSORS; i++) {
        if (tracked_sensors[i].id == id) { tracked_sensors[i].last_seen = now; return; }
        if (tracked_sensors[i].id == 0 && empty_slot == -1) empty_slot = i;
    }
    if (empty_slot != -1) { tracked_sensors[empty_slot].id = id; tracked_sensors[empty_slot].last_seen = now; }
}

static void recv_cb(const esp_now_recv_info_t * info, const uint8_t *data, int len) {
    if (len != sizeof(esp_now_packet_t)) return;
    esp_now_packet_t *p = (esp_now_packet_t *)data;

    if (p->msg_type == MSG_TIME_SYNC) {
        // pokud je sync přímo od GW (hop 0) a signál je slabý -> ignoruje, čeká na bližší směrovač
        int rssi = info->rx_ctrl->rssi;
        if (p->hop_count == 0 && rssi < MIN_DIRECT_RSSI) {
            ESP_LOGW(TAG, "Ignoring weak direct GW sync (RSSI: %d < %d), waiting for closer relay", rssi, MIN_DIRECT_RSSI);
            return;
        }
        if (p->unix_time > 1000000) {
            struct timeval tv = { .tv_sec = (time_t)p->unix_time, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            is_synced = true;
            
            // uloží si MAC toho, kdo poslal Sync (uplink). Pokud je to Gateway (hop 0) nebo jiný směrovač
            // přijímá uplink pouze pokud je jeho hop_count menší nebo roven aktuálnímu
            // (předchází mesh smyčkám 1 -> 2 -> 1)
            if (p->hop_count <= rtc_uplink_hop || !rtc_gateway_valid) {
                rtc_uplink_hop = p->hop_count;
                memcpy(rtc_gateway_mac, info->src_addr, 6);
                rtc_gateway_valid = true;
                
                // pokud je zdrojem brána (Gateway), nastaví uplink ID na 0000 (DIRECT)
                // jinak ukládá ID konkrétního odesílatele
                if (p->role == ROLE_GATEWAY) {
                    rtc_uplink_id = 0;
                } else {
                    rtc_uplink_id = p->relay_id ? p->relay_id : p->device_id;
                }

                rtc_gw_rssi = info->rx_ctrl->rssi;
                ESP_LOGI(TAG, "Time synced from Hop %d. Relaying...", p->hop_count);

                if (p->hop_count < MAX_HOPS) {
                    esp_now_packet_t forward_sync = *p;
                    forward_sync.hop_count++;
                    forward_sync.relay_id = my_id;
                    xQueueSend(sync_forward_queue, &forward_sync, 0);
                }
                
                // OTA Payload
                if (p->payload_len > 0) {
                    ota_sync_payload_t *ota = (ota_sync_payload_t *)p->payload;
                    for (int i = 0; i < ota->slot_count; i++) {
                        if (ota->slots[i].target_id == my_id || ota->slots[i].target_id == 0xFFFF) {
                            ESP_LOGI(TAG, "OTA Config Received! Interval: %lu, Channel: %d", 
                                     (unsigned long)ota->slots[i].sleep_interval, ota->slots[i].channel);
                            if (ota->slots[i].sleep_interval > 0) {
                                rtc_sync_interval_s = ota->slots[i].sleep_interval;
                            }
                            if (ota->slots[i].channel > 0 && rtc_wifi_channel != ota->slots[i].channel) {
                                rtc_wifi_channel = ota->slots[i].channel;
                                esp_wifi_set_channel(rtc_wifi_channel, WIFI_SECOND_CHAN_NONE);
                            }
                            if (ota->slots[i].safe_chunk_s > 0) {
                                rtc_safe_chunk_s = ota->slots[i].safe_chunk_s;
                            }
                        }
                    }
                }
                
                // uzel následuje kanál hlášený bránou (kvůli přeslechům mezi kanály)
                if (p->role == ROLE_GATEWAY && p->current_wifi_channel > 0 && p->current_wifi_channel != rtc_wifi_channel) {
                    ESP_LOGW(TAG, "Gateway shifted to channel %d. Following...", p->current_wifi_channel);
                    rtc_wifi_channel = p->current_wifi_channel;
                    esp_wifi_set_channel(rtc_wifi_channel, WIFI_SECOND_CHAN_NONE);
                }
            }
        }
    }

    if (p->msg_type == MSG_PAIRING_REQ) {
        // směrovač odpovídá vždy — i když ještě nemá sync od GW
        // (sync dorazí z forwarded pulzu dřív, než senzor pošle první data | měl by)
        if (p->role != ROLE_GATEWAY) {
            esp_now_packet_t resp = { .version = 1, .role = ROLE_ROUTER, .msg_type = MSG_PAIRING_RESP,
                                      .device_id = my_id, .unix_time = (uint64_t)time(NULL),
                                      .current_wifi_channel = rtc_wifi_channel };
            esp_now_peer_info_t peer = { .channel = rtc_wifi_channel, .encrypt = false };
            memcpy(peer.peer_addr, info->src_addr, 6);
            if (!esp_now_is_peer_exist(info->src_addr)) esp_now_add_peer(&peer);
            esp_now_send(info->src_addr, (uint8_t *)&resp, sizeof(resp));
        }
        return;
    }

    if (p->msg_type == MSG_DATA) {
        if (p->hop_count >= MAX_HOPS) return;
        track_sensor(p->device_id);
        esp_now_packet_t forward = *p;

        // pokud je první hop (Senzor -> Směrovač), uloží RSSI senzoru
        if (forward.hop_count == 0) {
            forward.rssi = info->rx_ctrl->rssi;
        }

        if (xQueueSend(forward_queue, &forward, 0) != pdTRUE) {
            ESP_LOGW(TAG, "forward_queue full, dropping packet from %04X", p->device_id);
        }
    }
}

void router_main_task(void *pvParameters) {
    // --- KONTROLA NAPĚTÍ BATERIE (SW POJISTKA) ---
    float v_start = read_battery_voltage();
    if (v_start < 3.58f) {
        ESP_LOGE(TAG, "Battery level critical (%.2fV)! Hardware protection active. Entering hibernation...", v_start);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_now_deinit();
        esp_wifi_stop();
        // spánek bez nastaveného probouzení = hibernace do manuálního resetu/nabití
        esp_deep_sleep_start();
    }
    ESP_LOGI(TAG, "Battery voltage: %.2fV (Ok)", v_start);

    if (!is_synced) {
        int scan_time = 0;
        while (!is_synced) { 
            vTaskDelay(pdMS_TO_TICKS(1000)); 
            printf("Scanning for Gateway on CH %d... (%ds)\n", rtc_wifi_channel, ++scan_time);
            
            // gateway posílá sync pulse každých 10s (možno změnit). Pokud do 12s nic nepřijde, skočí na další kanál.
            if (scan_time >= 12) {
                scan_time = 0;
                rtc_wifi_channel++;
                if (rtc_wifi_channel > 11) rtc_wifi_channel = 1; // Wi-Fi kanály v ČR (1 - 11)
                ESP_LOGW(TAG, "Pulse not found. Hopping to Channel %d", rtc_wifi_channel);
                esp_wifi_set_channel(rtc_wifi_channel, WIFI_SECOND_CHAN_NONE);
            }
        }
        ESP_LOGI(TAG, "First Sync achieved on Channel %d!", rtc_wifi_channel);
    }

    int64_t window_start = esp_timer_get_time();
    while ((esp_timer_get_time() - window_start) < (WAKE_WINDOW_MS * 1000)) {
        // odeslat čekající sync pulsy (nemohou být odeslány přímo z recv_cb, blbne to)
        esp_now_packet_t sync_to_fwd;
        if (xQueueReceive(sync_forward_queue, &sync_to_fwd, 0) == pdTRUE) {
            esp_now_send(broadcast_mac, (uint8_t *)&sync_to_fwd, sizeof(sync_to_fwd));
            ESP_LOGI(TAG, "Forwarded sync pulse (hop %d)", sync_to_fwd.hop_count);
        }

        esp_now_packet_t to_forward;
        if (xQueueReceive(forward_queue, &to_forward, pdMS_TO_TICKS(100)) == pdTRUE) {
            // forward jen tomu, od koho dostal sync (cesta uplink)
            if (rtc_gateway_valid) {
                esp_now_peer_info_t gw = { .encrypt = false, .channel = 0 };
                memcpy(gw.peer_addr, rtc_gateway_mac, 6);
                if (!esp_now_is_peer_exist(rtc_gateway_mac)) esp_now_add_peer(&gw);

                // razítko prvního směrovače: pokud to přišlo přímo od senzoru (relay_id == 0)
                if (to_forward.relay_id == 0) {
                    to_forward.relay_id = my_id;
                }
                // hop count jen pro senzor data, ne pro heartbeaty směrovače (ty už mají správnou hodnotu)
                if (to_forward.role != ROLE_ROUTER) {
                    to_forward.hop_count++;
                }
                ESP_LOGI(TAG, "Forwarding data from %04X (relay %04X, hop %d) to uplink", to_forward.device_id, to_forward.relay_id, to_forward.hop_count);
                if (!send_with_retry(rtc_gateway_mac, (uint8_t *)&to_forward, sizeof(esp_now_packet_t))) {
                    ESP_LOGW(TAG, "Forward FAILED after %d retries for %04X", SEND_RETRIES, to_forward.device_id);
                }
            }
        }
    }
 
    time_t now_time; time(&now_time);
    uint32_t next_window = ((now_time / rtc_sync_interval_s) + 1) * rtc_sync_interval_s;
    uint32_t sleep_time_s = next_window - now_time;
    
    if (sleep_time_s > rtc_safe_chunk_s) {
        uint32_t next_chunk = ((now_time / rtc_safe_chunk_s) + 1) * rtc_safe_chunk_s;
        if (next_chunk < next_window) {
            sleep_time_s = next_chunk - now_time;
            if (sleep_time_s > 1) sleep_time_s -= 1;
        }
    } else {
        if (sleep_time_s <= 1) sleep_time_s = rtc_sync_interval_s;
    }

    if (rtc_gateway_valid) {
        rtc_heartbeat_seq++;
        esp_now_packet_t hb; memset(&hb, 0, sizeof(hb));
        hb.version = 1; hb.role = ROLE_ROUTER; hb.msg_type = MSG_DATA; hb.device_id = my_id;
        hb.rssi = rtc_gw_rssi; hb.packet_seq = rtc_heartbeat_seq;
        hb.relay_id = rtc_uplink_id;
        hb.hop_count = rtc_uplink_hop; // pozice v síti (GW je 0, směrovač na GW je taky 0)
        hb.current_sleep_interval = rtc_sync_interval_s;
        hb.current_safe_chunk = rtc_safe_chunk_s;
        hb.current_wifi_channel = rtc_wifi_channel;
        hb.unix_time = (uint64_t)now_time;
        hb.active_time_us = (uint32_t)esp_timer_get_time(); // uptime tohoto probuzení
        hb.batt_v = read_battery_voltage();  
        
        esp_now_peer_info_t gw = { .encrypt = false, .channel = 0 };
        memcpy(gw.peer_addr, rtc_gateway_mac, 6);
        if (!esp_now_is_peer_exist(rtc_gateway_mac)) esp_now_add_peer(&gw);
        send_with_retry(rtc_gateway_mac, (uint8_t *)&hb, sizeof(hb));
    }

    ESP_LOGI(TAG, "ID:%04X | RSSI to GW: %d | Sleeping %lus", my_id, rtc_gw_rssi, (unsigned long)sleep_time_s);
    vTaskDelay(pdMS_TO_TICKS(100)); 
    
    uint64_t wake_us = (uint64_t)sleep_time_s * 1000000ULL;
    if (wake_us > 1000000) wake_us -= 1000000;
    
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();


    esp_sleep_enable_timer_wakeup(wake_us);
    esp_deep_sleep_start();
}

void app_main(void) {
    if (rtc_wifi_channel == 0 || rtc_wifi_channel > 14) rtc_wifi_channel = ESP_NOW_CHANNEL;
    if (rtc_safe_chunk_s == 0) rtc_safe_chunk_s = MAX_SAFE_CHUNK_S;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    wifi_init(); esp_now_init();
    send_sem = xSemaphoreCreateBinary();
    forward_queue = xQueueCreate(10, sizeof(esp_now_packet_t));
    sync_forward_queue = xQueueCreate(2, sizeof(esp_now_packet_t));
    esp_now_register_recv_cb(recv_cb);
    esp_now_register_send_cb(send_cb);
    
    uint8_t mac[6]; esp_wifi_get_mac(WIFI_IF_STA, mac); 
    my_id = (mac[4] << 8) | mac[5];
    
    esp_now_peer_info_t b = { .encrypt = false, .channel = 0 }; // hodnota 0 dynamicky sleduje fyzické Wi-Fi rádio | bez tohoto zlobí
    memcpy(b.peer_addr, broadcast_mac, 6); esp_now_add_peer(&b);
 
    // core 1 task
    xTaskCreatePinnedToCore(router_main_task, "router_task", 4096, NULL, 10, NULL, 1);
}
