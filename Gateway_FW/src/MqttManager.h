#pragma once
#include "mqtt_client.h"
#include "esp_event.h"
#include "DataManager.h"
#include <vector>
#include <mutex>
#include <queue>
#include <cstdint>

struct MqttProfile {
    char uri[128];
    char user[33];
    char pass[33];
};

struct QueuedMessage {
    char topic[128];
    char payload[512];
};

class MqttManager {
public:
    static MqttManager& getInstance();

    std::vector<MqttProfile> profiles;
    int current_profile_index = -1;
    char active_uri[128] = "";
    char active_user[33] = "";
    char active_pass[33] = "";
    char apn[64] = "lpwa.vodafone.com";
    bool modem_mqtt_connected = false;
    uint32_t publish_interval_s = 10;

    void init();
    void handleWiFiConnected();
    void handleWiFiDisconnected();
    void publishSensor(const SensorData& s);
    void publishRouterStatus(const SensorData& r, const std::vector<uint16_t>& children);
    void publishGatewayStatus();
    void setApn(const char* new_apn);
    void setPublishInterval(uint32_t interval);
    bool isConnected();
    bool isModemReady();
    void addProfile(const char* u, const char* n, const char* p);
    void removeProfile(int i);
    void clearAll();
    void reorderProfile(int f, int t);
    void connectProfile(int i);

private:
    MqttManager() {}
    esp_mqtt_client_handle_t client = NULL;
    bool connected = false;
    std::mutex mqtt_mutex;
    std::mutex uart_mutex;
    std::mutex queue_mutex;
    std::queue<QueuedMessage> msg_queue;

    void internalPublish(const char* topic, const char* payload);
    void initModemUart();
    static void modem_init_task(void* pv);
    void publishViaNBIoT(const char* t, const char* p);
    bool send_at_internal(const char* cmd);
    bool send_at_expect_internal(const char* cmd, const char* expected, uint32_t ms);
    void loadApn();
    void loadPublishInterval();
    void loadProfiles();
    void syncNvsInternal();
    void setupClientFromCurrentInternal();
    static void mqtt_event_handler(void *args, esp_event_base_t b, int32_t id, void *d);
};
