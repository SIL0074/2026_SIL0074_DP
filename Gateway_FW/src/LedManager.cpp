#include "LedManager.h"
#include "Config.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

LedManager& LedManager::getInstance() {
    static LedManager instance;
    return instance;
}

void LedManager::init() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    int pins[] = { LED_R_PIN, LED_G_PIN, LED_B_PIN };
    int channels[] = { LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2 };

    for (int i = 0; i < 3; i++) {
        // použije jen piny, které jsou v platném rozsahu ESP32 GPIO (0-39) | testování volných pinů LED pro LilyGO
        if (pins[i] >= 0 && pins[i] <= 39) {
            ledc_channel_config_t ledc_channel = {
                .gpio_num       = pins[i],
                .speed_mode     = LEDC_LOW_SPEED_MODE,
                .channel        = (ledc_channel_t)channels[i],
                .intr_type      = LEDC_INTR_DISABLE,
                .timer_sel      = LEDC_TIMER_0,
                .duty           = 0,
                .hpoint         = 0
            };
            ledc_channel_config(&ledc_channel);
        }
    }

    xTaskCreatePinnedToCore(led_animation_task, "led_task", 2048, this, 5, NULL, 0);
}

void LedManager::setState(LedState state) {
    std::lock_guard<std::mutex> lock(state_mutex);
    current_state = state;
}

void LedManager::notifyDataRx() {
    last_flash_time = esp_timer_get_time();
    data_flash_active = true;
}

void LedManager::update_hw_led(uint8_t r, uint8_t g, uint8_t b) {
    int pins[] = { LED_R_PIN, LED_G_PIN, LED_B_PIN };
    int channels[] = { 0, 1, 2 };
    uint8_t colors[] = { r, g, b };

    for (int i = 0; i < 3; i++) {
        if (pins[i] >= 0 && pins[i] <= 39) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channels[i], colors[i]);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channels[i]);
        }
    }
}

void LedManager::led_animation_task(void* pv) {
    LedManager* mgr = (LedManager*)pv;
    uint32_t step = 0;

    while (1) {
        uint8_t r = 0, g = 0, b = 0;
        
        if (mgr->data_flash_active) {
            if (esp_timer_get_time() - mgr->last_flash_time < 150000) {
                mgr->update_hw_led(255, 255, 255);
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            } else {
                mgr->data_flash_active = false;
            }
        }

        LedState st;
        {
            std::lock_guard<std::mutex> lock(mgr->state_mutex);
            st = mgr->current_state;
        }

        float pulse = (sinf(step * 0.05f) + 1.2f) / 2.2f;

        switch (st) {
            case LedState::BOOTING:
                if ((step / 10) % 2) { r = 200; g = 100; b = 0; }
                break;
            case LedState::WIFI_CONNECTING:
                r = 0; g = 0; b = (uint8_t)(255 * pulse);
                break;
            case LedState::MODEM_LOADING:
                r = 0; g = (uint8_t)(150 * pulse); b = (uint8_t)(255 * pulse);
                break;
            case LedState::ALL_OK:
                r = 0; g = 20; b = 0;
                break;
            case LedState::BRIDGE_OFFLINE:
                r = (uint8_t)(255 * pulse); g = 0; b = 0;
                break;
            case LedState::NB_IOT_MODE:
                r = (uint8_t)(150 * pulse); g = 0; b = (uint8_t)(255 * pulse);
                break;
        }

        mgr->update_hw_led(r, g, b);
        step++;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
