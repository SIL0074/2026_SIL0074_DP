/**
 * bme688_driver.h — BME688 I2C driver
 *
 * Použití: zahrnout do main.c pomocí #ifdef USE_BME688
 * Kompilace: pio run --environment bme688-pcb
 *
 * Konfigurace (z platformio.ini build_flags):
 *   -DUSE_BME688
 *   -DBME688_SDA_PIN=1
 *   -DBME688_SCL_PIN=3
 *   -DBME688_BATT_ADC_CH=0     (GPIO00 = ADC1_CH0)
 *   -DBME688_BATT_DIV_RATIO=2.0f
 */

#pragma once

#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "bme68x.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ── Výchozí hodnoty (lze přepsat build_flags) ─────────────────────────────
#ifndef BME688_SDA_PIN
#  define BME688_SDA_PIN       1
#endif
#ifndef BME688_SCL_PIN
#  define BME688_SCL_PIN       3
#endif
#ifndef BME688_BATT_ADC_CH
#  define BME688_BATT_ADC_CH   ADC_CHANNEL_0  // GPIO00
#endif
#ifndef BME688_BATT_DIV_RATIO
#  define BME688_BATT_DIV_RATIO 2.0f           // 100kΩ + 100kΩ dělič (na mé desce takto nastaven)
#endif
#define BME688_I2C_NUM         I2C_NUM_0
#define BME688_I2C_HZ          400000
#define BME688_ADDR            0x76            // SDO=GND → 0x76; SDO=VCC → 0x77 (na mé desce takto nastaven)

// ── Payload struct (SENSE_BME688 = 8 -> 8. typ senzoru z nabídky senzorů, viz main.c) ──────────────────────────────────────
typedef struct {
    float temp_c;          // Teplota [°C]
    float hum_pct;         // Relativní vlhkost [%]
    float pressure_hpa;    // Atmosferický tlak [hPa]
    float gas_kohm;        // Odpor plynu [kΩ] — indikátor VOC; 0 = neplatné (nevhodné pro spánkové aplikace při krátkých probuzeních)
} __attribute__((packed)) bme688_payload_t;  // 16 bytů — vejde se do 64B payloadu


static inline int8_t _bme68x_i2c_read(uint8_t reg, uint8_t *data, uint32_t len, void *ptr) {
    uint8_t addr = *(uint8_t *)ptr;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t r = i2c_master_cmd_begin(BME688_I2C_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return (r == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static inline int8_t _bme68x_i2c_write(uint8_t reg, const uint8_t *data, uint32_t len, void *ptr) {
    uint8_t addr = *(uint8_t *)ptr;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t r = i2c_master_cmd_begin(BME688_I2C_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return (r == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static inline void _bme68x_delay_us(uint32_t us, void *ptr) {
    (void)ptr;
    vTaskDelay(pdMS_TO_TICKS((us + 999) / 1000));
}

/**
 * Inicializuje I2C sběrnici pro BME688.
 * Volat jednou na začátku app_main, před wifi_init().
 */
static inline void bme688_i2c_init(void) {
    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = BME688_SDA_PIN,
        .scl_io_num       = BME688_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BME688_I2C_HZ,
    };
    i2c_param_config(BME688_I2C_NUM, &cfg);
    i2c_driver_install(BME688_I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    ESP_LOGI("BME688", "I2C init: SDA=GPIO%d SCL=GPIO%d @ %dHz",
             BME688_SDA_PIN, BME688_SCL_PIN, BME688_I2C_HZ);
}

/**
 * Přečte napětí baterie z ADC přes odporový dělič.
 * GPIO00 (ADC1_CH0), 100kΩ + 100kΩ dělič → ratio = 2.0
 */
static inline float bme688_read_battery(void) {
    adc_oneshot_unit_handle_t h;
    adc_oneshot_unit_init_cfg_t ic = { .unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE };
    adc_oneshot_new_unit(&ic, &h);
    adc_oneshot_chan_cfg_t cc = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    adc_oneshot_config_channel(h, BME688_BATT_ADC_CH, &cc);

    adc_cali_handle_t cali_handle = NULL;
    bool do_calibration = false;
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = BME688_BATT_ADC_CH,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK) {
        do_calibration = true;
    }

    int sum_raw = 0;
    int sum_mv = 0;
    for (int i = 0; i < 8; i++) {
        int raw; adc_oneshot_read(h, BME688_BATT_ADC_CH, &raw);
        sum_raw += raw;
        if (do_calibration) {
            int mv;
            adc_cali_raw_to_voltage(cali_handle, raw, &mv);
            sum_mv += mv;
        }
        vTaskDelay(1);
    }
    
    if (do_calibration) {
        adc_cali_delete_scheme_curve_fitting(cali_handle);
    }
    adc_oneshot_del_unit(h);

    float v;
    if (do_calibration) {
        v = ((float)sum_mv / 8.0f / 1000.0f) * BME688_BATT_DIV_RATIO;
    } else {
        v = ((float)sum_raw / 8.0f / 4095.0f) * 2.5f * BME688_BATT_DIV_RATIO;
    }
    
    ESP_LOGI("BME688", "Batt: %.2f V", v);
    return v;
}

/**
 * Provede jedno měření BME688 (forced mode).
 * @param out  výstupní struct s naměřenými hodnotami
 * @return true = data platná, false = chyba komunikace
 */
static inline bool bme688_measure(bme688_payload_t *out) {
    static uint8_t addr = BME688_ADDR;
    struct bme68x_dev dev = {
        .read     = _bme68x_i2c_read,
        .write    = _bme68x_i2c_write,
        .delay_us = _bme68x_delay_us,
        .intf     = BME68X_I2C_INTF,
        .intf_ptr = &addr,
        .amb_temp = 25,
    };
    if (bme68x_init(&dev) != BME68X_OK) {
        ESP_LOGE("BME688", "Init FAILED (addr=0x%02X)!", BME688_ADDR);
        return false;
    }
    struct bme68x_conf conf;
    bme68x_get_conf(&conf, &dev);
    conf.filter = BME68X_FILTER_OFF;
    conf.odr    = BME68X_ODR_NONE;
    conf.os_hum = BME68X_OS_2X;
    conf.os_pres= BME68X_OS_4X;
    conf.os_temp= BME68X_OS_8X;
    bme68x_set_conf(&conf, &dev);

    struct bme68x_heatr_conf hc = { .enable=BME68X_ENABLE, .heatr_temp=300, .heatr_dur=100 };
    bme68x_set_heatr_conf(BME68X_FORCED_MODE, &hc, &dev);
    bme68x_set_op_mode(BME68X_FORCED_MODE, &dev);

    uint32_t delay_us = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &dev) + hc.heatr_dur * 1000;
    dev.delay_us(delay_us, dev.intf_ptr);

    struct bme68x_data data; uint8_t nf = 0;
    bme68x_get_data(BME68X_FORCED_MODE, &data, &nf, &dev);
    if (nf == 0) { ESP_LOGE("BME688", "No data fields!"); return false; }

    out->temp_c       = data.temperature;
    out->hum_pct      = data.humidity;
    out->pressure_hpa = data.pressure / 100.0f;
    out->gas_kohm     = (data.status & BME68X_GASM_VALID_MSK) ? data.gas_resistance / 1000.0f : 0.0f;

    ESP_LOGI("BME688", "%.1f°C | %.1f%%RH | %.1fhPa | %.1fkΩ",
             out->temp_c, out->hum_pct, out->pressure_hpa, out->gas_kohm);
    return true;
}
