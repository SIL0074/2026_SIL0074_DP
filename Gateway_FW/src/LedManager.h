#pragma once
#include <mutex>
#include <cstdint>

enum class LedState {
    BOOTING,
    WIFI_CONNECTING,
    MODEM_LOADING,
    ALL_OK,
    BRIDGE_OFFLINE,
    NB_IOT_MODE
};

class LedManager {
public:
    static LedManager& getInstance();
    void init();
    void setState(LedState state);
    void notifyDataRx();

private:
    LedManager() : current_state(LedState::BOOTING), data_flash_active(false), last_flash_time(0) {}
    LedState current_state;
    std::mutex state_mutex;
    
    bool data_flash_active;
    int64_t last_flash_time;

    static void led_animation_task(void* pv);
    void update_hw_led(uint8_t r, uint8_t g, uint8_t b);
};
