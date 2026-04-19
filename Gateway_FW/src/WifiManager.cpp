#include "WifiManager.h"
#include "MqttManager.h"
#include "Config.h"
#include "LedManager.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <algorithm>

WifiManager& WifiManager::getInstance() {
    static WifiManager instance;
    return instance;
}

void WifiManager::init() {
    loadProfiles();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, this, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, this, NULL));

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    wifi_config_t ap_cfg = {};
    strcpy((char*)ap_cfg.ap.ssid, "ESP32_Gateway_Setup");
    ap_cfg.ap.ssid_len = strlen("ESP32_Gateway_Setup");
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    ap_cfg.ap.max_connection = 4;
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
    
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    ap_active = true;
    ESP_LOGI("WIFI", "AP Mode: 'ESP32_Gateway_Setup' started on 192.168.4.1 (ch 1)");

    if (!profiles.empty()) {
        LedManager::getInstance().setState(LedState::WIFI_CONNECTING);
        connectNext();
    } else {
        ESP_LOGW("WIFI", "No Wi-Fi profiles found in NVS memory.");
    }

    xTaskCreate(reconnect_task, "wifi_recon", 4096, this, 5, NULL);
}

void WifiManager::reconnect_task(void* pv) {
    WifiManager* mgr = (WifiManager*)pv;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(20000)); 
        wifi_ap_record_t info;
        if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) {
            if (!mgr->profiles.empty()) {
                ESP_LOGI("WIFI", "STA Disconnected. Background retry task starting...");
                mgr->connectNext();
            }
        }
    }
}

void WifiManager::loadProfiles() {
    std::lock_guard<std::mutex> lock(wifi_mutex);
    profiles.clear();
    nvs_handle_t h;
    if (nvs_open("wifi_store", NVS_READONLY, &h) == ESP_OK) {
        uint8_t count = 0;
        nvs_get_u8(h, "w_count", &count);
        for (int i = 0; i < count; i++) {
            WifiProfile p;
            char ks[16], kp[16];
            snprintf(ks, 16, "w_s_%d", i);
            snprintf(kp, 16, "w_p_%d", i);
            size_t s = 33, s2 = 65;
            if (nvs_get_str(h, ks, p.ssid, &s) == ESP_OK) {
                nvs_get_str(h, kp, p.pass, &s2);
                profiles.push_back(p);
            }
        }
        nvs_close(h);
    }
}

void WifiManager::saveProfile(const char* ssid, const char* pass) {
    std::lock_guard<std::mutex> lock(wifi_mutex);
    auto it = std::find_if(profiles.begin(), profiles.end(),
        [&](const WifiProfile& p) { return strcmp(p.ssid, ssid) == 0; });
    if (it != profiles.end()) {
        strcpy(it->pass, pass);
    } else {
        WifiProfile newP;
        strncpy(newP.ssid, ssid, 32); newP.ssid[32] = 0;
        strncpy(newP.pass, pass, 64); newP.pass[64] = 0;
        profiles.push_back(newP);
    }
    syncNvsInternal();
}

void WifiManager::removeProfile(const char* ssid) {
    std::lock_guard<std::mutex> lock(wifi_mutex);
    profiles.erase(
        std::remove_if(profiles.begin(), profiles.end(),
            [&](const WifiProfile& p) { return strcmp(p.ssid, ssid) == 0; }),
        profiles.end());
    syncNvsInternal();
}

void WifiManager::connectProfile(const char* ssid) {
    std::lock_guard<std::mutex> lock(wifi_mutex);
    for (int i = 0; i < (int)profiles.size(); i++) {
        if (strcmp(profiles[i].ssid, ssid) == 0) {
            current_profile_index = i;
            if (!ap_active) {
                esp_wifi_set_mode(WIFI_MODE_APSTA);
                ap_active = true;
            }
            wifi_config_t sta_cfg = {};
            strcpy((char*)sta_cfg.sta.ssid, profiles[i].ssid);
            strcpy((char*)sta_cfg.sta.password, profiles[i].pass);
            ESP_LOGI("WIFI", "Switching to SSID: %s", ssid);
            esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
            esp_wifi_connect();
            last_reconnect_time = esp_timer_get_time();
            return;
        }
    }
}

void WifiManager::connectNext() {
    if (profiles.empty()) return;
    current_profile_index = (current_profile_index + 1) % profiles.size();

    ESP_LOGI("WIFI", "Connecting to SSID: '%s' (Profile %d/%d)",
        profiles[current_profile_index].ssid,
        current_profile_index + 1, (int)profiles.size());

    if (!ap_active) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        ap_active = true;
    }

    wifi_config_t sta_cfg = {};
    strcpy((char*)sta_cfg.sta.ssid, profiles[current_profile_index].ssid);
    strcpy((char*)sta_cfg.sta.password, profiles[current_profile_index].pass);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_connect();
    last_reconnect_time = esp_timer_get_time();
}

void WifiManager::startScan() {
    if (!is_scanning) {
        is_scanning = true;
        scan_data_ready = false;
        wifi_scan_config_t sc = {};
        esp_wifi_scan_start(&sc, false);
    }
}

char* WifiManager::generateScanJson() {
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count > 20) count = 20;
    wifi_ap_record_t recs[20];
    esp_wifi_scan_get_ap_records(&count, recs);
    char* json = (char*)malloc(3072);
    if (!json) return NULL;
    char* p = json;
    size_t rem = 3072;
    int w = snprintf(p, rem, "{\"ready\":true, \"networks\":[");
    p += w; rem -= w;
    for (int i = 0; i < count; i++) {
        if (strlen((char*)recs[i].ssid) == 0) continue;
        w = snprintf(p, rem, "%s{\"ssid\":\"%s\",\"rssi\":%d}",
            (i > 0 ? "," : ""), (char*)recs[i].ssid, recs[i].rssi);
        p += w; rem -= w;
    }
    strcat(p, "]}");
    return json;
}

void WifiManager::clearAll() {
    std::lock_guard<std::mutex> lock(wifi_mutex);
    profiles.clear();
    syncNvsInternal();
}

void WifiManager::reorderProfile(int f, int t) {
    std::lock_guard<std::mutex> lock(wifi_mutex);
    if (f < 0 || f >= (int)profiles.size() || t < 0 || t >= (int)profiles.size()) return;
    WifiProfile p = profiles[f];
    profiles.erase(profiles.begin() + f);
    profiles.insert(profiles.begin() + t, p);
    syncNvsInternal();
}

void WifiManager::syncNvsInternal() {
    nvs_handle_t h;
    if (nvs_open("wifi_store", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_set_u8(h, "w_count", (uint8_t)profiles.size());
        for (int i = 0; i < (int)profiles.size(); i++) {
            char ks[16], kp[16];
            snprintf(ks, 16, "w_s_%d", i);
            snprintf(kp, 16, "w_p_%d", i);
            nvs_set_str(h, ks, profiles[i].ssid);
            nvs_set_str(h, kp, profiles[i].pass);
        }
        nvs_commit(h);
        nvs_close(h);
    }
}

void WifiManager::event_handler(void* arg, esp_event_base_t base, int32_t id, void* data) {
    WifiManager* mgr = (WifiManager*)arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) data;
        ESP_LOGI("WIFI", "SUCCESS! Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (mgr->ap_active) {
            esp_wifi_set_mode(WIFI_MODE_STA);
            mgr->ap_active = false;
            ESP_LOGI("WIFI", "AP Mode disabled, operating as STA only");
        }
        MqttManager::getInstance().handleWiFiConnected();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) data;
        ESP_LOGW("WIFI", "DISCONNECTED (Reason: %d). Auto-retrying...", event->reason);
        if (!mgr->ap_active) {
            esp_wifi_set_mode(WIFI_MODE_APSTA);
            mgr->ap_active = true;
            ESP_LOGI("WIFI", "AP Mode re-enabled as fallback");
        }
        MqttManager::getInstance().handleWiFiDisconnected();
        int64_t now = esp_timer_get_time();
        if ((now - mgr->last_reconnect_time) > 2000000 && !mgr->profiles.empty()) {
            mgr->connectNext();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        mgr->is_scanning = false;
        mgr->scan_data_ready = true;
    }
}
