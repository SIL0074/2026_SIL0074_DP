#ifndef MESH_COMMON_H
#define MESH_COMMON_H

#include <stdint.h>

#define ESP_NOW_CHANNEL 11

typedef enum {
    ROLE_GATEWAY = 0x01,
    ROLE_ROUTER  = 0x02,
    ROLE_SENSOR  = 0x03
} device_role_t;

typedef enum {
    MSG_DATA = 0,
    MSG_PAIRING_REQ = 1,
    MSG_PAIRING_RESP = 2,
    MSG_TIME_SYNC = 3
} msg_type_t;

// definice typů senzoru pro interpretaci na bráně
typedef enum {
    SENSE_GENERIC  = 0,
    SENSE_TEMP_HUM = 1,  // { float temp_c; float hum_pct; }          8 bytes
    SENSE_POWER_MON= 2,  // { float voltage; float current_ma; }      8 bytes
    SENSE_GPS      = 3,  // { float lat; float lon; float alt_m; }    12 bytes
    SENSE_SMOKE    = 4,  //NENÍ VHODNÝ PRO TUTO SÍŤ { float ppm; uint8_t alarm; }              5 bytes
    SENSE_MOTION   = 5,  //NENÍ VHODNÝ PRO TUTO SÍŤ { uint8_t detected; uint32_t count; }      5 bytes
    SENSE_AIR      = 6,  // { float co2_ppm; float tvoc_ppb; }        8 bytes
    SENSE_SOIL     = 7,  // { float moisture_pct; float temp_c; }     8 bytes
    SENSE_BME688   = 8   // { float temp_c; float hum_pct; float pressure_hpa; float gas_kohm; } 16 bytes
} sensor_type_t;

typedef struct {
    uint16_t target_id;
    uint32_t sleep_interval;
    uint16_t safe_chunk_s;
    uint8_t channel;
} __attribute__((packed)) ota_config_slot_t;


#define OTA_MAX_SLOTS 7

typedef struct {
    uint8_t slot_count;
    ota_config_slot_t slots[OTA_MAX_SLOTS];
} __attribute__((packed)) ota_sync_payload_t;


typedef struct {
    uint8_t  version;
    uint8_t  role;
    uint8_t  msg_type;
    uint8_t  sensor_type;   

    uint16_t device_id;
    uint16_t relay_id;
    uint32_t packet_seq;
    uint64_t unix_time;
    uint32_t active_time_us;

    float    batt_v;
    int8_t   rssi;
    uint8_t  hop_count;
    
    uint32_t current_sleep_interval;
    uint16_t current_safe_chunk;
    uint8_t  current_wifi_channel;

    uint8_t  payload_len;
    uint8_t  payload[64];

} __attribute__((packed)) esp_now_packet_t;

#endif
