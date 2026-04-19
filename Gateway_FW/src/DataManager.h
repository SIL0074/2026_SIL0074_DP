#pragma once
#include <vector>
#include <mutex>
#include <cstdint>
#include "mesh_common.h"

struct SensorData {
    uint16_t id;
    uint16_t last_relay;
    uint8_t role;
    uint8_t sensor_type;
    float batt;
    int8_t rssi;             // signál k prvnímu skoku (Senzor->Směrovač)
    int8_t link_rssi;        // signál k bráně (Směrovač->GW)
    uint8_t hop_count;
    uint32_t packets;
    uint32_t lost_packets;
    uint32_t received_packets;
    uint32_t active_time_us;
    uint64_t sensor_ts;
    int64_t last_seen_ts;
    uint32_t current_sleep_interval;
    uint16_t current_safe_chunk;
    uint8_t  current_wifi_channel;
    uint8_t payload_len;
    uint8_t payload[64];
};

struct OptimizationHint {
    uint16_t router_id;
    uint32_t current_interval;
    uint32_t recommended_interval;
    uint16_t child_count;
    bool needs_optimization;
};

class DataManager {
public:
    static DataManager& getInstance();
    void init(); 
    SensorData updateSensor(esp_now_packet_t* p, int8_t current_link_rssi);
    std::vector<SensorData> getSensorsCopy();
    
    void queueOtaConfig(uint16_t id, uint32_t interval, uint16_t safe_chunk, uint8_t channel);
    void removeOtaConfig(uint16_t id);
    std::vector<ota_config_slot_t> getOtaQueueCopy();
    
    bool isOtaEnabled();
    void setOtaEnabled(bool en);

    void setMeshChannel(uint8_t ch);
    uint8_t getMeshChannel() { return mesh_channel; }
    
    void resetAllStats(); 
    
    uint32_t calculateGCD(uint32_t a, uint32_t b);
    OptimizationHint getOptimizationHint(uint16_t routerId);

private:
    DataManager() : mesh_channel(ESP_NOW_CHANNEL) {}
    std::vector<SensorData> sensors;
    std::vector<ota_config_slot_t> ota_queue;
    uint8_t mesh_channel;
    bool ota_enabled = true;
    std::mutex data_mutex;
    SensorData* getSensor(uint16_t id);
    void saveConfigInternal();
};
