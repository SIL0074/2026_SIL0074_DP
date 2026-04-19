#include "WebServer.h"
#include "DataManager.h"
#include "MqttManager.h"
#include "WifiManager.h"
#include "SerialBridge.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <mutex>
#include "cJSON.h"

static void url_decode(char *dst, size_t dst_len, const char *src) {
    char a, b; size_t written = 0;
    while (*src && written < dst_len - 1) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10); else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10); else b -= '0';
            *dst++ = 16 * a + b; src += 3;
        } else if (*src == '+') { *dst++ = ' '; src++; }
        else { *dst++ = *src++; }
        written++;
    }
    *dst++ = '\0';
}

#include "web_content.inc"


void WebServer::init() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 25;
    config.stack_size = 24576;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uris[] = {
            {"/", HTTP_GET, root_handler, NULL},
            {"/data", HTTP_GET, data_handler, NULL},
            {"/logs", HTTP_GET, logs_handler, NULL},
            {"/save_wifi", HTTP_POST, save_handler_wifi, NULL},
            {"/save_mqtt", HTTP_POST, save_handler_mqtt, NULL},
            {"/save_apn", HTTP_POST, save_handler_apn, NULL},
            {"/delete_wifi", HTTP_POST, delete_handler_wifi, NULL},
            {"/delete_mqtt", HTTP_POST, delete_handler_mqtt, NULL},
            {"/scan", HTTP_GET, scan_handler, NULL},
            {"/scan_res", HTTP_GET, scan_res_handler, NULL},
            {"/reorder_wifi", HTTP_POST, reorder_handler_wifi, NULL},
            {"/reorder_mqtt", HTTP_POST, reorder_handler_mqtt, NULL},
            {"/api/set_ota", HTTP_POST, set_ota_handler, NULL},
            {"/api/remove_ota", HTTP_POST, remove_ota_handler, NULL},
            {"/api/ota_queue", HTTP_GET, get_ota_queue_handler, NULL},
            {"/api/set_pub_int", HTTP_POST, set_pub_int_handler, NULL},
            {"/api/ota_control", HTTP_POST, ota_control_handler, NULL},
            {"/api/reset_stats", HTTP_POST, reset_stats_handler, NULL}
        };
        for (int i = 0; i < (int)(sizeof(uris)/sizeof(httpd_uri_t)); i++) {
            httpd_register_uri_handler(server, &uris[i]);
        }
    }
}

esp_err_t WebServer::root_handler(httpd_req_t *req) {
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebServer::logs_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_chunk(req, "{\"logs\":[", 9);
    
    {
        std::lock_guard<std::mutex> lock(SerialBridge::getInstance().log_mutex);
        auto& logs = SerialBridge::getInstance().console_logs;
        char buf[512];
        for (size_t i = 0; i < logs.size(); i++) {
            int w = snprintf(buf, sizeof(buf), "%s\"%s\"", (i > 0 ? "," : ""), logs[i].c_str());
            if (w > 0) httpd_resp_send_chunk(req, buf, w);
        }
    }
    
    httpd_resp_send_chunk(req, "]}", 2);
    httpd_resp_send_chunk(req, NULL, 0); 
    return ESP_OK;
}

esp_err_t WebServer::data_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");

    bool mqtt = MqttManager::getInstance().isConnected();
    bool modem_ready = MqttManager::getInstance().isModemReady();
    bool ota_en = DataManager::getInstance().isOtaEnabled();
    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
    
    esp_netif_ip_info_t ipi;
    char ips[16] = "0.0.0.0";
    esp_netif_t* nif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (nif && esp_netif_get_ip_info(nif, &ipi) == ESP_OK) {
        esp_ip4addr_ntoa(&ipi.ip, ips, 16);
    }

    char buf[1024];
    int w = snprintf(buf, sizeof(buf),
        "{\"mqtt\":%s, \"modem_ready\":%s, \"ota_en\":%s, \"uptime_s\":%lu, \"apn\":\"%s\", \"pub_int\":%lu, \"ip\":\"%s\", \"bridge_ok\":%s, \"mch\":%d, \"nvs\":{\"wifi\":[",
        mqtt ? "true" : "false", modem_ready ? "true" : "false", ota_en ? "true" : "false",
        (unsigned long)uptime_s, MqttManager::getInstance().apn,
        (unsigned long)MqttManager::getInstance().publish_interval_s, ips,
        SerialBridge::getInstance().is_link_up ? "true" : "false",
        DataManager::getInstance().getMeshChannel());
    httpd_resp_send_chunk(req, buf, w);

    auto& wps = WifiManager::getInstance().profiles;
    for (size_t i = 0; i < wps.size(); i++) {
        w = snprintf(buf, sizeof(buf), "%s{\"ssid\":\"%s\"}", (i > 0 ? "," : ""), wps[i].ssid);
        httpd_resp_send_chunk(req, buf, w);
    }

    httpd_resp_send_chunk(req, "], \"mqtt\":[", 11);
    
    auto& mps = MqttManager::getInstance().profiles;
    for (size_t i = 0; i < mps.size(); i++) {
        w = snprintf(buf, sizeof(buf), "%s{\"uri\":\"%s\"}", (i > 0 ? "," : ""), mps[i].uri);
        httpd_resp_send_chunk(req, buf, w);
    }

    httpd_resp_send_chunk(req, "]}, \"sensors\":[", 15);

    auto sens = DataManager::getInstance().getSensorsCopy();
    for (size_t i = 0; i < sens.size(); i++) {
        uint32_t ago = (uint32_t)((esp_timer_get_time() - sens[i].last_seen_ts) / 1000000);
        char hex_p[130] = {0};
        for (int k = 0; k < sens[i].payload_len && k < 64; k++) {
            sprintf(hex_p + (k * 2), "%02X", sens[i].payload[k]);
        }
        
        OptimizationHint hint = DataManager::getInstance().getOptimizationHint(sens[i].id);

        int w = snprintf(buf, sizeof(buf),
            "%s{\"id\":\"%04X\",\"role\":%d,\"stype\":%d,"
            "\"p\":%lu,\"l\":%lu,\"r\":%lu,"
            "\"batt\":%.2f,\"rssi\":%d,\"lrssi\":%d,\"hops\":%d,\"ago\":%lu,"
            "\"sleep_int\":%lu,\"safe\":%d,\"channel\":%d,"
            "\"relay\":\"%04X\",\"plen\":%d,\"pay\":\"%s\","
            "\"opt_needs\":%s,\"opt_rec\":%lu}",
            (i > 0 ? "," : ""), sens[i].id, (int)sens[i].role, (int)sens[i].sensor_type,
            (unsigned long)sens[i].packets, (unsigned long)sens[i].lost_packets, (unsigned long)sens[i].received_packets,
            sens[i].batt, sens[i].rssi, sens[i].link_rssi, (int)sens[i].hop_count,
            (unsigned long)ago, (unsigned long)sens[i].current_sleep_interval, 
            sens[i].current_safe_chunk, sens[i].current_wifi_channel,
            sens[i].last_relay, (int)sens[i].payload_len, hex_p,
            hint.needs_optimization ? "true" : "false", (unsigned long)hint.recommended_interval);
        httpd_resp_send_chunk(req, buf, w);
    }

    httpd_resp_send_chunk(req, "]}", 2);
    httpd_resp_send_chunk(req, NULL, 0); 
    
    return ESP_OK;
}

esp_err_t WebServer::save_handler_wifi(httpd_req_t *req) {
    char b[256];
    int r = httpd_req_recv(req, b, 255);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    char rs[64] = {0}, rp[64] = {0};
    sscanf(b, "ssid=%63[^&]&pass=%63s", rs, rp);
    char s[33], p[65];
    url_decode(s, 33, rs);
    url_decode(p, 65, rp);
    WifiManager::getInstance().saveProfile(s, p);
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::delete_handler_wifi(httpd_req_t *req) {
    char b[256];
    int r = httpd_req_recv(req, b, 255);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    char rs[64] = {0};
    sscanf(b, "ssid=%63s", rs);
    char s[33];
    url_decode(s, 33, rs);
    WifiManager::getInstance().removeProfile(s);
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::save_handler_mqtt(httpd_req_t *req) {
    char b[256];
    int r = httpd_req_recv(req, b, 255);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    char ru[128] = {0}, rn[64] = {0}, rp[64] = {0};
    sscanf(b, "uri=%127[^&]&user=%63[^&]&pass=%63s", ru, rn, rp);
    char u[128], n[33], p[33];
    url_decode(u, 128, ru);
    url_decode(n, 33, rn);
    url_decode(p, 33, rp);
    MqttManager::getInstance().addProfile(u, n, p);
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::delete_handler_mqtt(httpd_req_t *req) {
    char b[256];
    int r = httpd_req_recv(req, b, 255);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    int i = -1;
    sscanf(b, "index=%d", &i);
    MqttManager::getInstance().removeProfile(i);
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::save_handler_apn(httpd_req_t *req) {
    char b[128];
    int r = httpd_req_recv(req, b, 127);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    char ra[64] = {0};
    sscanf(b, "apn=%63s", ra);
    char a[64];
    url_decode(a, 64, ra);
    MqttManager::getInstance().setApn(a);
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::scan_handler(httpd_req_t *req) {
    WifiManager::getInstance().startScan();
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::scan_res_handler(httpd_req_t *req) {
    if (!WifiManager::getInstance().scan_data_ready) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"ready\":false}", 15);
    }
    char* j = WifiManager::getInstance().generateScanJson();
    if (!j) return ESP_FAIL;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, j, strlen(j));
    free(j);
    return ESP_OK;
}

esp_err_t WebServer::reorder_handler_wifi(httpd_req_t *req) {
    char b[256];
    int r = httpd_req_recv(req, b, 255);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    int f = -1, t = -1;
    sscanf(b, "f=%d&t=%d", &f, &t);
    WifiManager::getInstance().reorderProfile(f, t);
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::reorder_handler_mqtt(httpd_req_t *req) {
    char b[256];
    int r = httpd_req_recv(req, b, 255);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    int f = -1, t = -1;
    sscanf(b, "f=%d&t=%d", &f, &t);
    MqttManager::getInstance().reorderProfile(f, t);
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::set_ota_handler(httpd_req_t *req) {
    char b[256];
    int r = httpd_req_recv(req, b, 255);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    
    char id_str[10] = {0};
    int interval = 0;
    int safe_chunk = 0;
    int channel = 0;
    
    // přiklad: id=B04A&interval=60&safe=30&channel=11
    sscanf(b, "id=%9[^&]&interval=%d&safe=%d&channel=%d", id_str, &interval, &safe_chunk, &channel);
    
    if (strlen(id_str) == 0 || (interval == 0 && channel == 0 && safe_chunk == 0)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing params (id and at least one config field required)");
    }
    
    uint16_t id = (uint16_t)strtoul(id_str, NULL, 16);
    DataManager::getInstance().queueOtaConfig(id, (uint32_t)interval, (uint16_t)safe_chunk, (uint8_t)channel);

    if (id == 0xFFFF && channel > 0) {
        DataManager::getInstance().setMeshChannel(channel);
    }
    
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::get_ota_queue_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    auto q = DataManager::getInstance().getOtaQueueCopy();
    
    char buf[128];
    httpd_resp_send_chunk(req, "{\"queue\":[", 10);
    for (size_t i = 0; i < q.size(); i++) {
        int w = snprintf(buf, sizeof(buf), "%s{\"id\":\"%04X\",\"sleep\":%lu,\"safe\":%d,\"ch\":%d}", 
                         (i > 0 ? "," : ""), q[i].target_id, (unsigned long)q[i].sleep_interval, q[i].safe_chunk_s, q[i].channel);
        if (w > 0) httpd_resp_send_chunk(req, buf, w);
    }
    httpd_resp_send_chunk(req, "]}", 2);
    httpd_resp_send_chunk(req, NULL, 0);
    
    return ESP_OK;
}

esp_err_t WebServer::remove_ota_handler(httpd_req_t *req) {
    char b[64];
    int r = httpd_req_recv(req, b, 63);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    
    char id_str[10] = {0};
    if (sscanf(b, "id=%9s", id_str) == 1) {
        uint16_t id = (uint16_t)strtoul(id_str, NULL, 16);
        DataManager::getInstance().removeOtaConfig(id);
    }
    
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::set_pub_int_handler(httpd_req_t *req) {
    char b[128];
    int r = httpd_req_recv(req, b, 127);
    if (r <= 0) return ESP_FAIL;
    b[r] = 0;
    
    int interval = 0;
    if (sscanf(b, "interval=%d", &interval) == 1) {
        MqttManager::getInstance().setPublishInterval((uint32_t)interval);
    }
    
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t WebServer::ota_control_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = 0;

    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *en = cJSON_GetObjectItem(root, "enabled");
        if (en) {
            DataManager::getInstance().setOtaEnabled(cJSON_IsTrue(en));
        }
        cJSON_Delete(root);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", 15);
    return ESP_OK;
}

esp_err_t WebServer::reset_stats_handler(httpd_req_t *req) {
    DataManager::getInstance().resetAllStats();
    return httpd_resp_send(req, "OK", 2);
}
