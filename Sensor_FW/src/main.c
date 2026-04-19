#include "mesh_common.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

#ifdef USE_BME688
#include "bme688_driver.h" 
#endif


#define SYNC_INTERVAL_S 10 // počáteční interval synchronizace
#define RSSI_THRESHOLD -85 // testovací thresshold, může být změněno
#define MAX_FAILURES 3 // maximální počet chyb odeslání před nouzí senzoru
#define SENSOR_SEND_RETRIES 3  // max pokusů o doručení před sleep
#define MAX_SAFE_CHUNK_S 60 // resycn kvůli RC oscilátoru
 

RTC_DATA_ATTR uint32_t rtc_sync_interval_s = SYNC_INTERVAL_S;
RTC_DATA_ATTR uint16_t rtc_safe_chunk_s = MAX_SAFE_CHUNK_S;
RTC_DATA_ATTR uint8_t  rtc_wifi_channel = ESP_NOW_CHANNEL;
RTC_DATA_ATTR uint32_t rtc_next_send_time = 0; 

RTC_DATA_ATTR bool is_synced = false;
RTC_DATA_ATTR uint8_t rtc_target_mac[6] = {0};
RTC_DATA_ATTR bool rtc_target_valid = false;
RTC_DATA_ATTR uint32_t boot_count = 0;
RTC_DATA_ATTR uint32_t rtc_packet_seq = 0;
RTC_DATA_ATTR int8_t rtc_last_rssi = -127;
RTC_DATA_ATTR int fail_count = 0;
RTC_DATA_ATTR float simulated_batt = 4.2f;
RTC_DATA_ATTR uint32_t motion_count = 0; 
RTC_DATA_ATTR int32_t rtc_drift_correction_us = 0; 

uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint16_t my_id = 0;
uint8_t best_mac[6] = {0};
int best_score = -127;
bool best_found = false;

static SemaphoreHandle_t ack_sem;
volatile bool last_send_success = false;
volatile bool sync_received_this_boot = false; 

float get_random(float min, float max) {
  return min + (float)esp_random() / ((float)UINT32_MAX / (max - min));
}

static void wifi_init(void) {
  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_wifi_set_channel(rtc_wifi_channel, WIFI_SECOND_CHAN_NONE);
}

static void send_cb(const uint8_t *mac_addr,
                    esp_now_send_status_t status) {
  last_send_success = (status == ESP_NOW_SEND_SUCCESS);
  xSemaphoreGive(ack_sem);
}

static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data,
                    int len) {
  if (len != sizeof(esp_now_packet_t))
    return;
  esp_now_packet_t *p = (esp_now_packet_t *)data;

  if ((p->msg_type == MSG_TIME_SYNC || p->msg_type == MSG_PAIRING_RESP) &&
      p->unix_time > 1000000) {
    struct timeval tv = {.tv_sec = (time_t)p->unix_time, .tv_usec = 0};
    settimeofday(&tv, NULL);
    is_synced = true;
    sync_received_this_boot = true;
    
    // OTA payload processing, pokud je dostupný a je to synchronizační broadcast
    if (p->msg_type == MSG_TIME_SYNC && p->payload_len > 0) {
        ota_sync_payload_t *ota = (ota_sync_payload_t *)p->payload;
        for (int i = 0; i < ota->slot_count; i++) {
            if (ota->slots[i].target_id == my_id || ota->slots[i].target_id == 0xFFFF) {
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
    // uzel následuje kanál hlášený bránou (platí vždy, i bez OTA payloadu vynucení)
    if (p->role == ROLE_GATEWAY && p->current_wifi_channel > 0 && p->current_wifi_channel != rtc_wifi_channel) {
        rtc_wifi_channel = p->current_wifi_channel;
        esp_wifi_set_channel(rtc_wifi_channel, WIFI_SECOND_CHAN_NONE);
    }
  }

  if (p->msg_type == MSG_PAIRING_RESP) {
    int8_t rssi = info->rx_ctrl->rssi;
    int score = (p->role == ROLE_GATEWAY) ? rssi : rssi - 6;
    if (!best_found || score > best_score) {
      memcpy(best_mac, info->src_addr, 6);
      best_score = score;
      best_found = true;
      rtc_last_rssi = rssi;
    }
  }
}

void perform_pairing() {
  rtc_target_valid = false;
  int attempts = 3;
  while (attempts--) {
    best_found = false;
    esp_now_packet_t req = {.version = 1,
                            .role = ROLE_SENSOR,
                            .msg_type = MSG_PAIRING_REQ,
                            .device_id = my_id};
    esp_now_send(broadcast_mac, (uint8_t *)&req, sizeof(req));
    vTaskDelay(pdMS_TO_TICKS(800)); // delší okno pro odpovědi, kratší zlobilo, tohle je vhodná hodnota
    if (best_found) {
      memcpy(rtc_target_mac, best_mac, 6);
      rtc_target_valid = true;
      fail_count = 0;
      return;
    }
  }
}

void app_main(void) {
  int64_t start_us = esp_timer_get_time();
  ack_sem = xSemaphoreCreateBinary();
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      nvs_flash_init();
  }

#ifdef USE_BME688
  bme688_i2c_init();  // I2C na GPIO01(SDA) / GPIO03(SCL) — musí být před wifi_init, jinak zlobí!
#endif

  wifi_init();
  esp_now_init();
  esp_now_register_send_cb(send_cb);
  esp_now_register_recv_cb(recv_cb);

  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  my_id = (mac[4] << 8) | mac[5];
  boot_count++;

  esp_now_peer_info_t b = {.encrypt = false, .channel = 0};
  memcpy(b.peer_addr, broadcast_mac, 6);
  esp_now_add_peer(&b);

  time_t now_boot; time(&now_boot);
  bool is_sync_only = false;
  // resync interval
  if (is_synced && rtc_sync_interval_s > rtc_safe_chunk_s) {
      if ((uint32_t)now_boot < rtc_next_send_time - 2) { // vzbudil se alespoň 2 vteřiny před skutečným termínem
          is_sync_only = true;
      }
  }

  // vynutíme nové párování při selhání nebo slabém signálu
  if (!rtc_target_valid || fail_count >= MAX_FAILURES ||
      rtc_last_rssi < RSSI_THRESHOLD) {
    if (fail_count >= MAX_FAILURES) {
      is_synced = false;
    }
    perform_pairing();
  }

  if (rtc_target_valid) {
    esp_now_peer_info_t target = {.encrypt = false, .channel = 0};
    memcpy(target.peer_addr, rtc_target_mac, 6);
    if (!esp_now_is_peer_exist(rtc_target_mac))
      esp_now_add_peer(&target);

    if (!is_sync_only) {
    float current_v = 0.0f;
    esp_now_packet_t p;
    memset(&p, 0, sizeof(p));
    p.version     = 1;
    p.role        = ROLE_SENSOR;
    p.msg_type    = MSG_DATA;
    p.device_id   = my_id;
    rtc_packet_seq++;
    p.packet_seq  = rtc_packet_seq;
    p.unix_time   = (uint64_t)time(NULL);
    p.current_sleep_interval = rtc_sync_interval_s;
    p.current_safe_chunk = rtc_safe_chunk_s;
    p.current_wifi_channel = rtc_wifi_channel;

#ifdef USE_BME688
    // ── reálná data z BME688 ─────────────────────────────────────────────────
    current_v = bme688_read_battery();
    bme688_payload_t bme_data = {0};
    if (bme688_measure(&bme_data)) {
        p.sensor_type = SENSE_BME688;
        p.payload_len = sizeof(bme_data);
        memcpy(p.payload, &bme_data, sizeof(bme_data));
    } else {
        p.sensor_type = SENSE_BME688;
        memset(p.payload, 0, sizeof(bme_data));
    }
#else
    // ── simulační data pro testing, když nemáme BME688 senzor, lze samozřejmě dodělat podmínky ───────────────────────────────────────────────────────
    if (simulated_batt > 3.0f) simulated_batt -= 0.001f;
    current_v = simulated_batt;

    uint8_t demo_type = my_id % 4;
    p.sensor_type = (demo_type == 0) ? SENSE_TEMP_HUM : 
                    (demo_type == 1) ? SENSE_SMOKE : 
                    (demo_type == 2) ? SENSE_MOTION : SENSE_AIR;
    
    // simulační payload plnění
    p.payload_len = 8; 
#endif

    p.batt_v = current_v;
    p.active_time_us = (uint32_t)(esp_timer_get_time() - start_us);

    // ochrana baterie (softwarové vypnutí): pokud jsme pod 3.6V, pošleme poslední packet a spíme navždy
    bool battery_critical = (current_v < 3.58f && current_v > 1.0f);

    // retry smyčka: snižuje ztrátovost z ~10% na <3%
    bool delivered = false;
    for (int attempt = 0; attempt < SENSOR_SEND_RETRIES && !delivered; attempt++) {
      if (attempt > 0) vTaskDelay(pdMS_TO_TICKS(50));
      esp_now_send(rtc_target_mac, (uint8_t *)&p, sizeof(p));
      if (xSemaphoreTake(ack_sem, pdMS_TO_TICKS(200)) == pdTRUE && last_send_success) {
        delivered = true;
      }
    }

    if (battery_critical) {
        esp_deep_sleep_start(); // žádný timer -> probudí se až na USB/Reset
    }

        if (!delivered) {
          fail_count++;
        }
    }
   
    // zachycení nového času a výpočet driftu
    sync_received_this_boot = false; 
    int64_t listen_start_us = esp_timer_get_time();
    int listen_ticks = is_sync_only ? 50 : 30; // zvětšíme okno pro pořešení driftu
    while (listen_ticks-- > 0 && !sync_received_this_boot) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (sync_received_this_boot) {
      int64_t pulse_arrival_us = esp_timer_get_time() - listen_start_us;
      // cílové čekání je 100ms. Pokud čeká 1000ms, korekce bude +900ms.
      int32_t current_drift_error = (int32_t)(pulse_arrival_us - 100000); 
      
      // mírně tlumíme, aby to nerozhodil jeden náhodný šum
      rtc_drift_correction_us = (rtc_drift_correction_us * 2 + current_drift_error) / 3;
    } else if (is_sync_only) {
    }
  }

  // ROZHODNUTÍ O SPÁNKU
  if (!is_synced || !rtc_target_valid) {
    // pokud nemáme čas nebo cíl, musíme chytit puls z brány.
    // brána teď vysílá striktně jednou za 10 vteřin (lze editovat). Garantovaně se tak chytí,
    // stane se tak POUZE PŘI PRVNÍM STARTU (po výměně baterie nebo flashování), pak už RTC ví přesný čas.
    int timeout = 105; // 10.5 sekundy (garance zachycení 10s cyklu)
    while (!is_synced && timeout--) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // pokud se nedočkal pulzu, znamená to, že brána tu není nebo je na jiném kanálu.
    // přepne rtc_wifi_channel, takže příští probuzení ze spánku zkusí skenovat kanál vedle.
    if (!is_synced) {
       rtc_wifi_channel++;
       if (rtc_wifi_channel > 11) rtc_wifi_channel = 1; // 1 - 11
    }
  }

  time_t now;
  time(&now);
  
  if (!is_sync_only || !is_synced || rtc_next_send_time == 0) {
      uint32_t next_window = ((now / rtc_sync_interval_s) + 1) * rtc_sync_interval_s;
      rtc_next_send_time = next_window;
  }
  
  uint32_t sleep_time_s = (rtc_next_send_time > now) ? (rtc_next_send_time - now) : rtc_sync_interval_s;
  
  // ROZKOUSKOVÁNÍ SPÁNKU (CHUNKING) pro potlačení driftu z důvodu nepřesného interního krystalu
  if (sleep_time_s > rtc_safe_chunk_s) {
      uint32_t next_chunk = ((now / rtc_safe_chunk_s) + 1) * rtc_safe_chunk_s;
      if (next_chunk < rtc_next_send_time) {
          sleep_time_s = next_chunk - now;
          if (sleep_time_s > 1) {
              sleep_time_s -= 1; // rezerva
          }
      }
  }

  uint32_t offset_us = 0;
  if (!is_sync_only) {
    offset_us = 200000 + (esp_random() % 1800000);
  }

  // posune timer tak, aby minimalizoval čekání
  int64_t final_sleep_us = ((int64_t)sleep_time_s * 1000000ULL) + offset_us + rtc_drift_correction_us;
  if (final_sleep_us < 500000) final_sleep_us = 500000; // min spánek 0.5s
  
  esp_sleep_enable_timer_wakeup(final_sleep_us);
  esp_deep_sleep_start();
}