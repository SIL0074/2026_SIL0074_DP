#include "DataManager.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

#define TAG "DATA_MGR"

DataManager& DataManager::getInstance() {
    static DataManager instance;
    return instance;
}

void DataManager::init() {
    std::lock_guard<std::mutex> lock(data_mutex);
    nvs_handle_t h;
    if (nvs_open("mesh_store", NVS_READONLY, &h) == ESP_OK) {
        uint8_t ch = 0;
        if (nvs_get_u8(h, "m_ch", &ch) == ESP_OK && ch > 0) {
            mesh_channel = ch;
        }
        
        uint8_t en = 1;
        if (nvs_get_u8(h, "ota_en", &en) == ESP_OK) {
            ota_enabled = (en == 1);
        }
        
        uint8_t count = 0;
        nvs_get_u8(h, "ota_count", &count);
        ota_queue.clear();
        for (int i = 0; i < count && i < OTA_MAX_SLOTS; i++) {
            ota_config_slot_t slot;
            char key[16];
            size_t size = sizeof(slot);
            snprintf(key, 16, "ota_%d", i);
            if (nvs_get_blob(h, key, &slot, &size) == ESP_OK) {
                ota_queue.push_back(slot);
            }
        }
        nvs_close(h);
        ESP_LOGI(TAG, "Mesh Config Loaded: Channel %d, OTA Queue %d", mesh_channel, (int)ota_queue.size());
    }
}

SensorData DataManager::updateSensor(esp_now_packet_t* p, int8_t current_link_rssi) {
    std::lock_guard<std::mutex> lock(data_mutex);
    SensorData* s = getSensor(p->device_id);
    if (!s) {
        SensorData newS = {};
        newS.id = p->device_id;
        newS.packets = p->packet_seq;
        sensors.push_back(newS);
        s = &sensors.back();
    } else {
        // detekce restartu senzoru: pokud je seq výrazně nižší
        // považuje to za restart a vynuluje statistiku tohoto uzlu.
        if (p->packet_seq < s->packets) {
            s->lost_packets = 0;
            s->received_packets = 0;
            s->packets = 0; 
        } else if (p->packet_seq > s->packets && s->packets > 0) {
            s->lost_packets += (p->packet_seq - s->packets - 1);
        }
    }
    s->received_packets++;

    s->last_relay = p->relay_id;
    s->role = p->role;
    s->sensor_type = p->sensor_type;
    s->batt = p->batt_v;
    s->rssi = p->rssi;
    s->link_rssi = current_link_rssi;
    s->hop_count = p->hop_count;
    s->active_time_us = p->active_time_us;
    s->sensor_ts = p->unix_time;
    s->packets = p->packet_seq;
    s->last_seen_ts = esp_timer_get_time();
    s->current_sleep_interval = p->current_sleep_interval;
    s->current_safe_chunk = p->current_safe_chunk;
    s->current_wifi_channel = p->current_wifi_channel;

    bool queueChanged = false;
    for (auto it = ota_queue.begin(); it != ota_queue.end(); ) {
        if (it->target_id == s->id) {
            bool intMatch = (it->sleep_interval == 0 || s->current_sleep_interval == it->sleep_interval);
            bool safeMatch = (it->safe_chunk_s == 0 || s->current_safe_chunk == it->safe_chunk_s);
            bool chMatch = (it->channel == 0 || s->current_wifi_channel == it->channel);
            
            if (intMatch && safeMatch && chMatch) {
                it = ota_queue.erase(it);
                queueChanged = true;
                continue; 
            }
        } else if (it->target_id == 0xFFFF) {
            bool allMatch = true;
            int validNodes = 0;
            for (auto& sn : sensors) {
                if (sn.role == 1) continue; 
                validNodes++;
                bool intMatch = (it->sleep_interval == 0 || sn.current_sleep_interval == it->sleep_interval);
                bool safeMatch = (it->safe_chunk_s == 0 || sn.current_safe_chunk == it->safe_chunk_s);
                bool chMatch = (it->channel == 0 || sn.current_wifi_channel == it->channel);
                if (!intMatch || !safeMatch || !chMatch) {
                    allMatch = false;
                    break;
                }
            }
            if (allMatch && validNodes > 0) {
                ESP_LOGI(TAG, "Global OTA migration (FFFF) completed by all %d nodes. Removing from queue.", validNodes);
                it = ota_queue.erase(it);
                queueChanged = true;
                continue;
            }
        }
        ++it;
    }
    if (queueChanged) saveConfigInternal();

    s->payload_len = p->payload_len;
    memcpy(s->payload, p->payload, p->payload_len);

    return *s;
}

std::vector<SensorData> DataManager::getSensorsCopy() {
    std::lock_guard<std::mutex> lock(data_mutex);
    return sensors;
}

void DataManager::queueOtaConfig(uint16_t id, uint32_t interval, uint16_t safe_chunk, uint8_t channel) {
    std::lock_guard<std::mutex> lock(data_mutex);
    bool found = false;
    for (auto& q : ota_queue) {
        if (q.target_id == id) {
            q.sleep_interval = interval;
            q.safe_chunk_s = safe_chunk;
            q.channel = channel;
            found = true;
            break;
        }
    }
    if (!found && ota_queue.size() < OTA_MAX_SLOTS) {
        ota_config_slot_t slot;
        slot.target_id = id;
        slot.sleep_interval = interval;
        slot.safe_chunk_s = safe_chunk;
        slot.channel = channel;
        ota_queue.push_back(slot);
    }
    saveConfigInternal();
}

void DataManager::removeOtaConfig(uint16_t id) {
    std::lock_guard<std::mutex> lock(data_mutex);
    bool changed = false;
    for (auto it = ota_queue.begin(); it != ota_queue.end(); ) {
        if (it->target_id == id) {
            it = ota_queue.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) saveConfigInternal();
}

std::vector<ota_config_slot_t> DataManager::getOtaQueueCopy() {
    std::lock_guard<std::mutex> lock(data_mutex);
    return ota_queue;
}

void DataManager::setMeshChannel(uint8_t ch) {
    std::lock_guard<std::mutex> lock(data_mutex);
    mesh_channel = ch;
    saveConfigInternal();
}

bool DataManager::isOtaEnabled() {
    std::lock_guard<std::mutex> lock(data_mutex);
    return ota_enabled;
}

void DataManager::setOtaEnabled(bool en) {
    std::lock_guard<std::mutex> lock(data_mutex);
    ota_enabled = en;
    saveConfigInternal();
}

void DataManager::resetAllStats() {
    std::lock_guard<std::mutex> lock(data_mutex);
    for (auto& s : sensors) {
        s.packets = 0;
        s.lost_packets = 0;
        s.received_packets = 0;
    }
    ESP_LOGI(TAG, "All sensor statistics have been reset.");
}


uint32_t DataManager::calculateGCD(uint32_t a, uint32_t b) {
    while (b) {
        a %= b;
        std::swap(a, b);
    }
    return a;
}

OptimizationHint DataManager::getOptimizationHint(uint16_t routerId) {
    std::lock_guard<std::mutex> lock(data_mutex);
    OptimizationHint hint = {routerId, 0, 0, 0, false};
    
    std::vector<uint32_t> intervals;
    for (const auto& s : sensors) {
        if (s.id == routerId) {
            hint.current_interval = s.current_sleep_interval;
        }
        if (s.last_relay == routerId && s.id != routerId) {
            if (s.current_sleep_interval > 0) {
                intervals.push_back(s.current_sleep_interval);
                hint.child_count++;
            }
        }
    }
    
    if (intervals.empty()) return hint;
    
    uint32_t res_gcd = intervals[0];
    for (size_t i = 1; i < intervals.size(); i++) {
        res_gcd = calculateGCD(res_gcd, intervals[i]);
    }
    
    hint.recommended_interval = res_gcd;
    if (hint.current_interval > 0) {
        if (res_gcd % hint.current_interval != 0) {
            hint.needs_optimization = true;
        }
    } else {
        hint.needs_optimization = true;
    }
    
    return hint;
}

SensorData* DataManager::getSensor(uint16_t id) {
    for (auto& s : sensors) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

void DataManager::saveConfigInternal() {
    nvs_handle_t h;
    if (nvs_open("mesh_store", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "m_ch", mesh_channel);
        nvs_set_u8(h, "ota_en", ota_enabled ? 1 : 0);
        nvs_set_u8(h, "ota_count", (uint8_t)ota_queue.size());
        for (int i = 0; i < (int)ota_queue.size(); i++) {
            char key[16];
            snprintf(key, 16, "ota_%d", i);
            nvs_set_blob(h, key, &ota_queue[i], sizeof(ota_config_slot_t));
        }
        nvs_commit(h);
        nvs_close(h);
    }
}
