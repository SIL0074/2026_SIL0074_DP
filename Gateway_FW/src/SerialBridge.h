#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

class SerialBridge {
public:
    static SerialBridge& getInstance();

    bool is_link_up = false;
    uint32_t packets_received = 0;
    int64_t last_packet_time = 0;
    std::vector<std::string> console_logs;
    std::mutex log_mutex;

    void init();
    void add_log(const char* direction, const char* msg);
    void sendTime(int64_t t);

private:
    SerialBridge() {}
    static void monitor_task(void *pvParameters);
    static void uart_task(void *p);
    void parse_json(const char* js);
};
