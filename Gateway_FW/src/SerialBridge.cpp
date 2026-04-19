#include "SerialBridge.h"
#include "DataManager.h"
#include "MqttManager.h"
#include "Config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <algorithm>
#include <time.h>
#include "LedManager.h"

#define TAG "BRIDGE"

SerialBridge& SerialBridge::getInstance() {
    static SerialBridge instance;
    return instance;
}

void SerialBridge::init() {
    uart_config_t uc;
    memset(&uc, 0, sizeof(uart_config_t));
    uc.baud_rate = 115200;
    uc.data_bits = UART_DATA_8_BITS;
    uc.parity = UART_PARITY_DISABLE;
    uc.stop_bits = UART_STOP_BITS_1;
    uc.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uc.source_clk = UART_SCLK_DEFAULT;
    uart_driver_install(BRIDGE_UART_PORT, 2048, 0, 0, NULL, 0);
    uart_param_config(BRIDGE_UART_PORT, &uc);
    uart_set_pin(BRIDGE_UART_PORT, BRIDGE_TX_PIN, BRIDGE_RX_PIN, -1, -1);

    last_packet_time = esp_timer_get_time();
    add_log("SYSTEM", "Serial Bridge Initialized (Dual-Core)");
    xTaskCreatePinnedToCore(uart_task, "ub_task", 8192, this, 10, NULL, 1);
    xTaskCreatePinnedToCore(monitor_task, "mon_task", 2048, this, 5, NULL, 1);
}

void SerialBridge::add_log(const char* direction, const char* msg) {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string s = msg;
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    std::replace(s.begin(), s.end(), '\"', '\'');
    if (s.empty()) return;
    if (console_logs.size() > 15) console_logs.erase(console_logs.begin());
    char entry[256];
    snprintf(entry, sizeof(entry), "[%s] %s: %s",
        direction, direction[0] == 'S' ? "---" : (direction[0] == 'T' ? "OUT" : "IN "), s.c_str());
    console_logs.push_back(std::string(entry));
}

void SerialBridge::sendTime(int64_t t) {
    auto q = DataManager::getInstance().getOtaQueueCopy();
    
    char b[512];
    int l = snprintf(b, sizeof(b), "{\"type\":\"TIME\",\"val\":%.0f", (double)t);
    
    if (!q.empty() && DataManager::getInstance().isOtaEnabled()) {
        l += snprintf(b + l, sizeof(b) - l, ",\"ota\":[");
        for (size_t i = 0; i < q.size(); i++) {
            l += snprintf(b + l, sizeof(b) - l, "%s{\"id\":\"%04X\",\"int\":%lu,\"safe\":%d,\"ch\":%d}",
                (i > 0 ? "," : ""), q[i].target_id, (unsigned long)q[i].sleep_interval, (int)q[i].safe_chunk_s, (int)q[i].channel);
        }
        l += snprintf(b + l, sizeof(b) - l, "]");
    }
    
    l += snprintf(b + l, sizeof(b) - l, ",\"mch\":%d", DataManager::getInstance().getMeshChannel());
    
    l += snprintf(b + l, sizeof(b) - l, "}\n");
    uart_write_bytes(BRIDGE_UART_PORT, b, l);
    add_log("TX", b);
}

void SerialBridge::monitor_task(void *pvParameters) {
    SerialBridge* mgr = (SerialBridge*)pvParameters;
    while (1) {
        if (esp_timer_get_time() - mgr->last_packet_time > 20000000) {
            if (mgr->is_link_up) {
                mgr->add_log("SYSTEM", "Link Status: OFFLINE (Timeout)");
                LedManager::getInstance().setState(LedState::BRIDGE_OFFLINE);
            }
            mgr->is_link_up = false;
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void SerialBridge::uart_task(void *p) {
    SerialBridge* mgr = (SerialBridge*)p;
    uint8_t* data = (uint8_t*) malloc(1024);
    char line_buffer[1024];
    int line_pos = 0;

    while (1) {
        int len = uart_read_bytes(BRIDGE_UART_PORT, data, 1023, 50 / portTICK_PERIOD_MS);
        if (len > 0) {
            mgr->last_packet_time = esp_timer_get_time();
            if (!mgr->is_link_up) {
                mgr->add_log("SYSTEM", "Link Status: ONLINE");
                LedManager::getInstance().setState(LedState::ALL_OK);
            }
            mgr->is_link_up = true;

            for (int i = 0; i < len; i++) {
                if (data[i] == '\n' || data[i] == '\r') {
                    if (line_pos > 0) {
                        line_buffer[line_pos] = 0;
                        mgr->add_log("RX", line_buffer);
                        mgr->parse_json(line_buffer);
                        line_pos = 0;
                    }
                } else if (line_pos < 1023) {
                    line_buffer[line_pos++] = data[i];
                }
            }
        }
    }
    free(data);
}

void SerialBridge::parse_json(const char* js) {
    cJSON *root = cJSON_Parse(js);
    if (!root) return;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type && strcmp(type->valuestring, "DATA") == 0) {
        esp_now_packet_t p = {};

        cJSON *role_obj = cJSON_GetObjectItem(root, "role");
        p.role = role_obj ? (uint8_t)role_obj->valueint : 0;

        cJSON *id_obj = cJSON_GetObjectItem(root, "id");
        cJSON *relay_obj = cJSON_GetObjectItem(root, "relay");
        if (!id_obj || !relay_obj) { cJSON_Delete(root); return; }

        p.device_id = (uint16_t)strtol(id_obj->valuestring, NULL, 16);
        p.relay_id = (uint16_t)strtol(relay_obj->valuestring, NULL, 16);
        
        LedManager::getInstance().notifyDataRx(); // probliknutí diody při datech

        if (p.device_id == 0) {
            cJSON_Delete(root);
            return;
        }

        cJSON *seq_obj = cJSON_GetObjectItem(root, "seq");
        p.packet_seq = seq_obj ? (uint32_t)seq_obj->valueint : 0;

        cJSON *ts_obj = cJSON_GetObjectItem(root, "ts");
        p.unix_time = ts_obj ? (uint64_t)ts_obj->valuedouble : 0;

        cJSON *stype_obj = cJSON_GetObjectItem(root, "stype");
        p.sensor_type = stype_obj ? (uint8_t)stype_obj->valueint : 0;

        cJSON *pay_obj = cJSON_GetObjectItem(root, "payload");
        if (pay_obj) {
            const char* hex = pay_obj->valuestring;
            p.payload_len = strlen(hex) / 2;
            for (int i = 0; i < (int)p.payload_len && i < 64; i++) {
                char byte[3] = { hex[i*2], hex[i*2+1], 0 };
                p.payload[i] = (uint8_t)strtol(byte, NULL, 16);
            }
        }

        cJSON *batt_obj = cJSON_GetObjectItem(root, "batt");
        cJSON *rssi_obj = cJSON_GetObjectItem(root, "rssi");
        cJSON *lrssi_obj = cJSON_GetObjectItem(root, "lrssi");
        cJSON *hops_obj = cJSON_GetObjectItem(root, "hops");
        cJSON *active_obj = cJSON_GetObjectItem(root, "active");
        cJSON *sleep_obj = cJSON_GetObjectItem(root, "sleep_int");
        cJSON *safe_obj = cJSON_GetObjectItem(root, "safe");
        cJSON *ch_obj = cJSON_GetObjectItem(root, "channel");

        if (!batt_obj || !rssi_obj || !lrssi_obj || !hops_obj || !active_obj) {
            cJSON_Delete(root);
            return;
        }

        p.batt_v = (float)batt_obj->valuedouble;
        p.rssi = (int8_t)rssi_obj->valueint;
        int8_t lrssi = (int8_t)lrssi_obj->valueint;
        p.hop_count = (uint8_t)hops_obj->valueint;
        p.active_time_us = (uint32_t)active_obj->valueint;
        p.current_sleep_interval = sleep_obj ? (uint32_t)sleep_obj->valueint : 0;
        p.current_safe_chunk = safe_obj ? (uint16_t)safe_obj->valueint : 0;
        p.current_wifi_channel = ch_obj ? (uint8_t)ch_obj->valueint : 0;

        SensorData s = DataManager::getInstance().updateSensor(&p, lrssi);

        if (p.role == ROLE_SENSOR) {
            MqttManager::getInstance().publishSensor(s);
        } else if (p.role == ROLE_ROUTER) {
            std::vector<uint16_t> children;
            auto all_sens = DataManager::getInstance().getSensorsCopy();
            for (auto& ds : all_sens) {
                if (ds.role == ROLE_SENSOR && ds.last_relay == p.device_id) {
                    if ((esp_timer_get_time() - ds.last_seen_ts) < 600000000) {
                        children.push_back(ds.id);
                    }
                }
            }
            MqttManager::getInstance().publishRouterStatus(s, children);
        }

        packets_received++;
    }
    cJSON_Delete(root);
}
