// A lot of this is based on Kyane (pookie)'s code for the EPS in Arduino IDE
// Power electronics scares me, so I just copied the code and made it work in ESP-IDF. I don't know what I'm doing, but it works! :3
#include "hal.h"
#include "pins.h"
#include "can_bus.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "eps_hal";

// https://www.ti.com/lit/ds/symlink/ina226.pdf
#define INA_REG_CONFIG 0x00
#define INA_REG_SHUNT  0x01   /* signed, 2.5 uV per bit */
#define INA_REG_MFR_ID 0xFE
#define INA_MFR_ID_TI  0x5449 /* "TI" */

#define INA_CONFIG_AVG16 0x4527

#define I2C_TIMEOUT_MS 20
#define I2C_SPEED_HZ   100000 

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_ina;
static bool                    s_ina_ok; /* false: skip reads instead of logging an I2C error every 100 ms */

static bool ina_read(uint8_t reg, uint16_t *out) {

    uint8_t buf[2];
    if (i2c_master_transmit_receive(s_ina, &reg, 1, buf, 2, I2C_TIMEOUT_MS) != ESP_OK) return false;
    *out = (uint16_t)((buf[0] << 8) | buf[1]); /* INA226 registers are big-endian ESP32s ain't */

    return true;
}

static bool ina_write(uint8_t reg, uint16_t v) {
    uint8_t buf[3] = { reg, (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
    return i2c_master_transmit(s_ina, buf, sizeof buf, I2C_TIMEOUT_MS) == ESP_OK;
}

// 
static void i2c_scan(void)
{
    int found = 0;
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(s_bus, addr, I2C_TIMEOUT_MS) == ESP_OK) {
            ESP_LOGW(TAG, ">  I2C device answered at 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, ">  nothing answered: check SDA=%d SCL=%d, don't trust sensor values",
                 PIN_I2C_SDA, PIN_I2C_SCL);
    }
}

static bool ina_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA226_ADDR,
        .scl_speed_hz = I2C_SPEED_HZ,
    };
    uint16_t id = 0;

    if (i2c_new_master_bus(&bus_cfg, &s_bus) != ESP_OK) return false;
    if (i2c_master_bus_add_device(s_bus, &dev_cfg, &s_ina) != ESP_OK) return false;

    if (!ina_read(INA_REG_MFR_ID, &id) || id != INA_MFR_ID_TI) {
        ESP_LOGE(TAG, "INA226 not found at 0x%02X (SDA=%d SCL=%d), scanning I2C bus:",
                 INA226_ADDR, PIN_I2C_SDA, PIN_I2C_SCL);
        i2c_scan();
        return false;
    }
    return ina_write(INA_REG_CONFIG, INA_CONFIG_AVG16);
}

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t         s_cali;
static adc_channel_t             s_chan;

static bool adc_init(void)
{
    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(PIN_THERMISTOR, &unit, &s_chan) != ESP_OK) return false;

    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = unit };
    adc_oneshot_chan_cfg_t chan_cfg = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = unit, .chan = s_chan, .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    return adc_oneshot_new_unit(&unit_cfg, &s_adc) == ESP_OK &&
           adc_oneshot_config_channel(s_adc, s_chan, &chan_cfg) == ESP_OK &&
           adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) == ESP_OK;
}
bool hal_init(void)
{
    bool ok = true;

    if (!can_bus_init(PIN_CAN_TX, PIN_CAN_RX)) { ESP_LOGE(TAG, "CAN init failed"); ok = false; }
    s_ina_ok = ina_init();
    if (!s_ina_ok) ok = false;
    if (!adc_init())  { ESP_LOGE(TAG, "thermistor ADC init failed"); ok = false; }
    return ok;
}

bool hal_get_shunt_mv(float *out)
{
    uint16_t raw;
    if (!s_ina_ok) return false; /* aa */
    if (!ina_read(INA_REG_SHUNT, &raw)) return false;
    *out = (float)(int16_t)raw * 0.0025f; /* 2.5 uV per bit */
    return true;
}

bool hal_get_thermistor_mv(float *out)
{
    int raw, mv;
    if (adc_oneshot_read(s_adc, s_chan, &raw) != ESP_OK) return false;
    if (adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) return false;
    *out = (float)mv;
    return true;
}

void hal_restart(void)
{
    esp_restart();
}