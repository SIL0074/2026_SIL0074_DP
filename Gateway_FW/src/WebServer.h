#pragma once
#include "esp_http_server.h"
#include "esp_err.h"

class WebServer {
public:
    static void init();
private:
    static esp_err_t root_handler(httpd_req_t *req);
    static esp_err_t data_handler(httpd_req_t *req);
    static esp_err_t logs_handler(httpd_req_t *req);
    static esp_err_t save_handler_wifi(httpd_req_t *req);
    static esp_err_t delete_handler_wifi(httpd_req_t *req);
    static esp_err_t save_handler_mqtt(httpd_req_t *req);
    static esp_err_t delete_handler_mqtt(httpd_req_t *req);
    static esp_err_t save_handler_apn(httpd_req_t *req);
    static esp_err_t scan_handler(httpd_req_t *req);
    static esp_err_t scan_res_handler(httpd_req_t *req);
    static esp_err_t reorder_handler_wifi(httpd_req_t *req);
    static esp_err_t reorder_handler_mqtt(httpd_req_t *req);
    static esp_err_t set_ota_handler(httpd_req_t *req);
    static esp_err_t get_ota_queue_handler(httpd_req_t *req);
    static esp_err_t remove_ota_handler(httpd_req_t *req);
    static esp_err_t set_pub_int_handler(httpd_req_t *req);
    static esp_err_t ota_control_handler(httpd_req_t *req);
    static esp_err_t reset_stats_handler(httpd_req_t *req);
};