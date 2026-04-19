#include "MqttManager.h"
#include "Config.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <algorithm>
#include "SerialBridge.h"
#include "DataManager.h"
#include "LedManager.h"

MqttManager& MqttManager::getInstance() {
    static MqttManager instance;
    return instance;
}

void MqttManager::init() {
    initModemUart();
    loadProfiles();
    loadApn();
    loadPublishInterval();
    if (!profiles.empty()) {
        current_profile_index = 0;
        strncpy(active_uri, profiles[0].uri, 127); active_uri[127] = 0;
        strncpy(active_user, profiles[0].user, 32); active_user[32] = 0;
        strncpy(active_pass, profiles[0].pass, 32); active_pass[32] = 0;
    }
    setupClientFromCurrentInternal();
    xTaskCreate(modem_init_task, "modem_task", 8192, this, 10, NULL);
}

void MqttManager::handleWiFiConnected() {
    std::lock_guard<std::mutex> lock(mqtt_mutex);
    setupClientFromCurrentInternal();
}

void MqttManager::handleWiFiDisconnected() {
    std::lock_guard<std::mutex> lock(mqtt_mutex);
    connected = false;
    if (client) esp_mqtt_client_stop(client);
}

void MqttManager::publishSensor(const SensorData& s) {
    char topic[128], payload_str[512];
    const char* link_type = connected ? "WiFi" : "NB-IoT";
    snprintf(topic, 128, "esp-now/infra/sensors/%04X", s.id);

    char hex_data[130] = {0};
    for (int i = 0; i < s.payload_len && i < 64; i++) {
        sprintf(hex_data + (i * 2), "%02X", s.payload[i]);
    }

    snprintf(payload_str, 512,
        "{\"id\":\"%04X\",\"relay\":\"%04X\",\"stype\":%d,\"seq\":%lu,\"ts\":%llu,"
        "\"batt\":%.2f,\"rssi\":%d,\"hops\":%d,\"active_us\":%lu,"
        "\"sleep_int\":%lu,\"channel\":%d,"
        "\"plen\":%d,\"data\":\"%s\",\"link\":\"%s\","
        "\"p\":%lu,\"l\":%lu,\"r\":%lu}",
        s.id, s.last_relay, (int)s.sensor_type, (unsigned long)s.packets,
        (unsigned long long)s.sensor_ts, s.batt, s.rssi, s.hop_count,
        (unsigned long)s.active_time_us, (unsigned long)s.current_sleep_interval, s.current_wifi_channel,
        (int)s.payload_len, hex_data, link_type,
        (unsigned long)s.packets, (unsigned long)s.lost_packets, (unsigned long)s.received_packets);
    internalPublish(topic, payload_str);
}

void MqttManager::publishRouterStatus(const SensorData& r, const std::vector<uint16_t>& children) {
    char topic[128], payload[512];
    snprintf(topic, 128, "esp-now/infra/routers/%04X", r.id);
    char sensors_json[256] = "[";
    for (size_t i = 0; i < children.size(); i++) {
        char id_str[10];
        snprintf(id_str, 10, "%s\"%04X\"", (i > 0 ? "," : ""), children[i]);
        strcat(sensors_json, id_str);
    }
    strcat(sensors_json, "]");
    snprintf(payload, 512,
        "{\"id\":\"%04X\",\"type\":\"ROUTER\",\"batt\":%.2f,\"rssi_to_gw\":%d,\"hops\":%d,"
        "\"sleep_int\":%lu,\"channel\":%d,\"active_sensors\":%s,\"uptime_us\":%llu}",
        r.id, r.batt, r.rssi, r.hop_count, (unsigned long)r.current_sleep_interval, r.current_wifi_channel,
        sensors_json, (unsigned long long)r.active_time_us);
    internalPublish(topic, payload);
}

void MqttManager::publishGatewayStatus() {
    char topic[128], payload[512];
    snprintf(topic, 128, "esp-now/infra/gateway_info");
    esp_netif_ip_info_t ipi;
    char ips[16] = "0.0.0.0";
    esp_netif_t* nif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (nif && esp_netif_get_ip_info(nif, &ipi) == ESP_OK) {
        esp_ip4addr_ntoa(&ipi.ip, ips, 16);
    }
    const char* link_type = connected ? "WiFi" : (modem_mqtt_connected ? "NB-IoT" : "Disconnected");
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    
    int active_sensors = 0;
    int active_routers = 0;
    auto sens = DataManager::getInstance().getSensorsCopy();
    for (auto& s : sens) {
        if ((esp_timer_get_time() - s.last_seen_ts) < 600000000) {
            if (s.role == ROLE_SENSOR) active_sensors++;
            else if (s.role == ROLE_ROUTER) active_routers++;
        }
    }
    int total_nodes = active_sensors + active_routers;

    snprintf(payload, 512,
        "{\"id\":\"GATEWAY\",\"link\":\"%s\",\"ip\":\"%s\",\"apn\":\"%s\","
        "\"free_heap\":%lu,\"uptime_s\":%lu,\"bridge_ok\":%s,"
        "\"pub_interval_s\":%lu,\"total_nodes\":%d,\"active_sensors\":%d,\"active_routers\":%d}",
        link_type, ips, apn, (unsigned long)free_heap, (unsigned long)uptime_s,
        SerialBridge::getInstance().is_link_up ? "true" : "false",
        (unsigned long)publish_interval_s, total_nodes, active_sensors, active_routers);
    internalPublish(topic, payload);
}

void MqttManager::loadPublishInterval() {
    nvs_handle_t h;
    if (nvs_open("mqtt_store", NVS_READONLY, &h) == ESP_OK) {
        uint32_t val = 0;
        if (nvs_get_u32(h, "pub_int", &val) == ESP_OK && val >= 5) {
            publish_interval_s = val;
        }
        nvs_close(h);
    }
}

void MqttManager::setPublishInterval(uint32_t interval) {
    if (interval < 5) interval = 5; 
    publish_interval_s = interval;
    nvs_handle_t h;
    if (nvs_open("mqtt_store", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, "pub_int", publish_interval_s);
        nvs_commit(h);
        nvs_close(h);
    }
}



void MqttManager::setApn(const char* new_apn) {
    strncpy(apn, new_apn, 63);
    apn[63] = 0;
    nvs_handle_t h;
    if (nvs_open("modem_store", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "apn", apn);
        nvs_commit(h);
        nvs_close(h);
    }
}

bool MqttManager::isConnected() { return connected || modem_mqtt_connected; }
bool MqttManager::isModemReady() { return modem_mqtt_connected; }

void MqttManager::addProfile(const char* u, const char* n, const char* p) {
    std::lock_guard<std::mutex> lock(mqtt_mutex);
    MqttProfile pr;
    strncpy(pr.uri, u, 127); pr.uri[127] = 0;
    strncpy(pr.user, n, 32); pr.user[32] = 0;
    strncpy(pr.pass, p, 32); pr.pass[32] = 0;
    profiles.push_back(pr);
    syncNvsInternal();
}

void MqttManager::removeProfile(int i) {
    std::lock_guard<std::mutex> lock(mqtt_mutex);
    if (i >= 0 && i < (int)profiles.size()) {
        profiles.erase(profiles.begin() + i);
        syncNvsInternal();
    }
}

void MqttManager::clearAll() {
    std::lock_guard<std::mutex> lock(mqtt_mutex);
    profiles.clear();
    current_profile_index = -1;
    syncNvsInternal();
}

void MqttManager::reorderProfile(int f, int t) {
    std::lock_guard<std::mutex> lock(mqtt_mutex);
    if (f < 0 || f >= (int)profiles.size() || t < 0 || t >= (int)profiles.size()) return;
    MqttProfile p = profiles[f];
    profiles.erase(profiles.begin() + f);
    profiles.insert(profiles.begin() + t, p);
    syncNvsInternal();
}

void MqttManager::connectProfile(int i) {
    std::lock_guard<std::mutex> lock(mqtt_mutex);
    if (i >= 0 && i < (int)profiles.size()) {
        current_profile_index = i;
        strncpy(active_uri, profiles[i].uri, 127);
        strncpy(active_user, profiles[i].user, 32);
        strncpy(active_pass, profiles[i].pass, 32);
        setupClientFromCurrentInternal();
    }
}


void MqttManager::internalPublish(const char* topic, const char* payload) {
    if (connected) {
        esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            while (!msg_queue.empty()) {
                QueuedMessage qm = msg_queue.front(); msg_queue.pop();
                esp_mqtt_client_publish(client, qm.topic, qm.payload, 0, 1, 0);
            }
        }
    } else {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (msg_queue.size() < 100) {
            QueuedMessage msg;
            strncpy(msg.topic, topic, 127); msg.topic[127] = 0;
            strncpy(msg.payload, payload, 511); msg.payload[511] = 0;
            msg_queue.push(msg);
        }
    }
}

void MqttManager::initModemUart() {
    uart_config_t uc;
    memset(&uc, 0, sizeof(uc));
    uc.baud_rate = 115200;
    uc.data_bits = UART_DATA_8_BITS;
    uc.parity = UART_PARITY_DISABLE;
    uc.stop_bits = UART_STOP_BITS_1;
    uc.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uc.source_clk = UART_SCLK_DEFAULT;
    uart_driver_install(MODEM_UART_PORT, 1024, 0, 0, NULL, 0);
    uart_param_config(MODEM_UART_PORT, &uc);
    uart_set_pin(MODEM_UART_PORT, MODEM_TX_PIN, MODEM_RX_PIN, -1, -1);
    gpio_set_direction((gpio_num_t)MODEM_PWR_PIN, GPIO_MODE_OUTPUT);
}

void MqttManager::modem_init_task(void* pv) {
    MqttManager* mgr = (MqttManager*)pv;
    LedManager::getInstance().setState(LedState::MODEM_LOADING);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level((gpio_num_t)MODEM_PWR_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)MODEM_PWR_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level((gpio_num_t)MODEM_PWR_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(4000));

    while (1) {
        if (!mgr->modem_mqtt_connected) {
            std::lock_guard<std::mutex> lock(mgr->uart_mutex);
            mgr->send_at_internal("ATE0\r\n");
            mgr->send_at_internal("AT+CNMP=38\r\n");
            mgr->send_at_internal("AT+CMNB=2\r\n");
            bool attached = false;
            for (int i = 0; i < 30; i++) {
                if (mgr->send_at_expect_internal("AT+CGATT?\r\n", "+CGATT: 1", 1000)) {
                    attached = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            if (attached) {
                // získání času ze sítě operátora (NITZ)
                mgr->send_at_internal("AT+CLTS=1\r\n"); 
                uart_flush(MODEM_UART_PORT);
                uart_write_bytes(MODEM_UART_PORT, "AT+CCLK?\r\n", 10);
                uint8_t rbuf[256] = {0};
                int rlen = uart_read_bytes(MODEM_UART_PORT, rbuf, 255, pdMS_TO_TICKS(1000));
                if (rlen > 0) {
                    char* start = strstr((char*)rbuf, "+CCLK: \"");
                    if (start) {
                        start += 8;
                        int yy, mm, dd, hr, mn, sc;
                        if (sscanf(start, "%d/%d/%d,%d:%d:%d", &yy, &mm, &dd, &hr, &mn, &sc) >= 6) {
                            struct tm tm_info;
                            memset(&tm_info, 0, sizeof(struct tm));
                            tm_info.tm_year = yy + 100; 
                            tm_info.tm_mon  = mm - 1;
                            tm_info.tm_mday = dd;
                            tm_info.tm_hour = hr;
                            tm_info.tm_min  = mn;
                            tm_info.tm_sec  = sc;
                            time_t epoch = mktime(&tm_info);
                            
                            time_t now; time(&now);
                            if (epoch > 1000000000 && now < 1000000000) {
                                struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
                                settimeofday(&tv, NULL);
                                ESP_LOGI("MODEM", "NITZ Time Synced: %02d/%02d/20%02d %02d:%02d:%02d", dd, mm, yy, hr, mn, sc);
                            }
                        }
                    }
                }

                char cmd[256];
                snprintf(cmd, 256, "AT+CGDCONT=1,\"IP\",\"%s\"\r\n", mgr->apn);
                mgr->send_at_internal(cmd);
                snprintf(cmd, 256, "AT+CNACT=1,\"%s\"\r\n", mgr->apn);
                mgr->send_at_internal(cmd);
                mgr->send_at_internal("AT+CGACT=1,1\r\n");
                vTaskDelay(pdMS_TO_TICKS(3000));
                mgr->send_at_internal("AT+SMDISC\r\n");
                mgr->send_at_internal("AT+SMCONF=\"CLIENTID\",\"sil0074\"\r\n");
                const char* s = strstr(mgr->active_uri, "://");
                char host[128];
                strncpy(host, s ? s + 3 : mgr->active_uri, 127); host[127] = 0;
                char* sep = strchr(host, ':');
                if (sep) *sep = 0;
                snprintf(cmd, 256, "AT+SMCONF=\"URL\",\"%s\",\"1883\"\r\n", host);
                mgr->send_at_internal(cmd);
                snprintf(cmd, 256, "AT+SMCONF=\"USERNAME\",\"%s\"\r\n", mgr->active_user);
                mgr->send_at_internal(cmd);
                snprintf(cmd, 256, "AT+SMCONF=\"PASSWORD\",\"%s\"\r\n", mgr->active_pass);
                mgr->send_at_internal(cmd);
                if (mgr->send_at_expect_internal("AT+SMCONN\r\n", "OK", 20000)) {
                    mgr->modem_mqtt_connected = true;
                    if (!mgr->connected) {
                        LedManager::getInstance().setState(LedState::NB_IOT_MODE);
                    }
                }
            }
        } else {
            {
                std::lock_guard<std::mutex> lock(mgr->uart_mutex);
                if (!mgr->send_at_expect_internal("AT+SMSTATE?\r\n", "+SMSTATE: 1", 2000)) {
                    mgr->modem_mqtt_connected = false;
                }
            }
            if (mgr->modem_mqtt_connected && !mgr->connected) {
                while (true) {
                    QueuedMessage qm;
                    {
                        std::lock_guard<std::mutex> lock(mgr->queue_mutex);
                        if (mgr->msg_queue.empty()) break;
                        qm = mgr->msg_queue.front();
                        mgr->msg_queue.pop();
                    }
                    mgr->publishViaNBIoT(qm.topic, qm.payload);
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void MqttManager::publishViaNBIoT(const char* t, const char* p) {
    std::lock_guard<std::mutex> lock(uart_mutex);
    char cmd[128];
    snprintf(cmd, 128, "AT+SMPUB=\"%s\",%d,1,0\r\n", t, (int)strlen(p));
    uart_flush(MODEM_UART_PORT);
    uart_write_bytes(MODEM_UART_PORT, cmd, strlen(cmd));
    uint8_t buf[32];
    int len = uart_read_bytes(MODEM_UART_PORT, buf, 31, pdMS_TO_TICKS(2000));
    if (len > 0 && strchr((char*)buf, '>')) {
        uart_write_bytes(MODEM_UART_PORT, p, strlen(p));
    }
}

bool MqttManager::send_at_internal(const char* cmd) {
    uart_flush(MODEM_UART_PORT);
    uart_write_bytes(MODEM_UART_PORT, cmd, strlen(cmd));
    ESP_LOGI("MODEM", "OUT: %s", cmd);

    uint8_t r[256];
    int len = uart_read_bytes(MODEM_UART_PORT, r, 255, pdMS_TO_TICKS(1000));
    if (len > 0) {
        char buf[257];
        memcpy(buf, r, len);
        buf[len] = 0;
        ESP_LOGI("MODEM", "IN: %s", buf);
    }
    return len > 0;
}

bool MqttManager::send_at_expect_internal(const char* cmd, const char* expected, uint32_t ms) {
    uart_flush(MODEM_UART_PORT);
    uart_write_bytes(MODEM_UART_PORT, cmd, strlen(cmd));
    ESP_LOGI("MODEM", "OUT: %s", cmd);

    uint8_t r[256];
    int len = uart_read_bytes(MODEM_UART_PORT, r, 255, pdMS_TO_TICKS(ms));
    if (len > 0) {
        char buf[257];
        memcpy(buf, r, len);
        buf[len] = 0;
        ESP_LOGI("MODEM", "IN: %s", buf);
    }
    return (len > 0 && strstr((char*)r, expected));
}

void MqttManager::loadApn() {
    nvs_handle_t h;
    if (nvs_open("modem_store", NVS_READONLY, &h) == ESP_OK) {
        size_t s = 64;
        nvs_get_str(h, "apn", apn, &s);
        nvs_close(h);
    }
}

void MqttManager::loadProfiles() {
    profiles.clear();
    nvs_handle_t h;
    if (nvs_open("mqtt_store", NVS_READONLY, &h) == ESP_OK) {
        uint8_t c = 0;
        nvs_get_u8(h, "m_count", &c);
        for (int i = 0; i < c; i++) {
            MqttProfile p;
            char ku[16], kn[16], kp[16];
            snprintf(ku, 16, "m_u_%d", i);
            snprintf(kn, 16, "m_n_%d", i);
            snprintf(kp, 16, "m_p_%d", i);
            size_t s1 = 128, s2 = 33, s3 = 33;
            if (nvs_get_str(h, ku, p.uri, &s1) == ESP_OK) {
                nvs_get_str(h, kn, p.user, &s2);
                nvs_get_str(h, kp, p.pass, &s3);
                profiles.push_back(p);
            }
        }
        nvs_close(h);
    }
}

void MqttManager::syncNvsInternal() {
    nvs_handle_t h;
    if (nvs_open("mqtt_store", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_set_u8(h, "m_count", (uint8_t)profiles.size());
        for (int i = 0; i < (int)profiles.size(); i++) {
            char ku[16], kn[16], kp[16];
            snprintf(ku, 16, "m_u_%d", i);
            snprintf(kn, 16, "m_n_%d", i);
            snprintf(kp, 16, "m_p_%d", i);
            nvs_set_str(h, ku, profiles[i].uri);
            nvs_set_str(h, kn, profiles[i].user);
            nvs_set_str(h, kp, profiles[i].pass);
        }
        nvs_commit(h);
        nvs_close(h);
    }
}

void MqttManager::setupClientFromCurrentInternal() {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = nullptr;
    }
    if (active_uri[0] == '\0') {
        ESP_LOGW("MQTT", "No valid MQTT profile found, skipping connection");
        return;
    }
    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = active_uri;
    cfg.credentials.username = active_user;
    cfg.credentials.authentication.password = active_pass;
    client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, this);
    esp_mqtt_client_start(client);
}

void MqttManager::mqtt_event_handler(void *args, esp_event_base_t b, int32_t id, void *d) {
    MqttManager* mgr = (MqttManager*)args;
    if (id == MQTT_EVENT_CONNECTED) {
        mgr->connected = true;
        LedManager::getInstance().setState(LedState::ALL_OK);
    }
    else if (id == MQTT_EVENT_DISCONNECTED) mgr->connected = false;
}
