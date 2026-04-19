#pragma once
#include <vector>
#include <mutex>
#include <cstdint>
#include "esp_event.h"

struct WifiProfile {
    char ssid[33];
    char pass[65];
};

class WifiManager {
public:
    static WifiManager& getInstance();

    std::vector<WifiProfile> profiles;
    int current_profile_index = -1;
    int64_t last_reconnect_time = 0;
    bool is_scanning = false;
    bool scan_data_ready = false;
    bool ap_active = false;

    void init();
    void loadProfiles();
    void saveProfile(const char* ssid, const char* pass);
    void removeProfile(const char* ssid);
    void connectProfile(const char* ssid);
    void connectNext();
    void startScan();
    char* generateScanJson();
    void clearAll();
    void reorderProfile(int f, int t);

private:
    WifiManager() {}
    std::mutex wifi_mutex;
    void syncNvsInternal();
    static void event_handler(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void reconnect_task(void* pv);
};
