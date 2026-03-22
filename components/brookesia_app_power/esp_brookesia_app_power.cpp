/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdio>
#include <cstring>
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "esp_brookesia.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "AppPower"
#include "esp_lib_utils.h"

using namespace esp_brookesia::systems;
using namespace esp_brookesia::gui;

namespace {

constexpr const char *APP_NAME = "Device Care";
constexpr uint32_t APP_ICON_COLOR = 0xC9A7FF;
constexpr uint32_t PMU_I2C_FREQ = 100000;
constexpr uint32_t PMU_I2C_TIMEOUT_MS = 1000;
constexpr uint8_t PMU_I2C_ADDRESS = 0x34;

static i2c_master_bus_handle_t s_i2c_bus = nullptr;
static i2c_master_dev_handle_t s_pmu_dev = nullptr;
static XPowersPMU s_pmu;
static bool s_pmu_ready = false;
static lv_obj_t *s_info_label = nullptr;
static lv_timer_t *s_power_timer = nullptr;

constexpr base::App::Config makeCoreConfig(const char *name, uint32_t icon_color)
{
    auto cfg = base::App::Config::SIMPLE_CONSTRUCTOR(name, nullptr, true);
    cfg.launcher_icon = StyleImage::IMAGE_RECOLOR(&esp_brookesia_image_small_app_launcher_default_98_98, icon_color);
    return cfg;
}

constexpr phone::App::Config makePhoneConfig(uint8_t page_index)
{
    auto cfg = phone::App::Config::SIMPLE_CONSTRUCTOR(nullptr, true, false);
    cfg.app_launcher_page_index = page_index;
    return cfg;
}

int pmu_register_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    (void)dev_addr;
    esp_err_t ret = i2c_master_transmit_receive(s_pmu_dev, &reg_addr, 1, data, len, PMU_I2C_TIMEOUT_MS);
    return (ret == ESP_OK) ? 0 : -1;
}

int pmu_register_write_byte(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    (void)dev_addr;
    uint8_t buffer[16] = {0};
    if ((len == 0) || (len > (sizeof(buffer) - 1))) {
        return -1;
    }

    buffer[0] = reg_addr;
    memcpy(&buffer[1], data, len);
    esp_err_t ret = i2c_master_transmit(s_pmu_dev, buffer, len + 1, PMU_I2C_TIMEOUT_MS);
    return (ret == ESP_OK) ? 0 : -1;
}

bool ensure_pmu_ready()
{
    if (s_pmu_ready) {
        return true;
    }

    if (s_i2c_bus == nullptr) {
        s_i2c_bus = bsp_i2c_get_handle();
        ESP_UTILS_CHECK_NULL_RETURN(s_i2c_bus, false, "Get board I2C bus failed");
    }

    if (s_pmu_dev == nullptr) {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = PMU_I2C_ADDRESS,
            .scl_speed_hz = PMU_I2C_FREQ,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        ESP_UTILS_CHECK_ERROR_RETURN(
            i2c_master_bus_add_device(s_i2c_bus, &dev_config, &s_pmu_dev), false, "Create PMU I2C device failed"
        );
    }

    ESP_UTILS_CHECK_FALSE_RETURN(
        s_pmu.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte), false, "Init PMU failed"
    );

    s_pmu.enableVbusVoltageMeasure();
    s_pmu.enableBattVoltageMeasure();
    s_pmu.enableSystemVoltageMeasure();
    s_pmu.enableTemperatureMeasure();
    s_pmu.disableTSPinMeasure();

    s_pmu_ready = true;
    return true;
}

void refresh_power_label()
{
    if (s_info_label == nullptr) {
        return;
    }

    if (!ensure_pmu_ready()) {
        lv_label_set_text(s_info_label, "PMU init failed");
        return;
    }

    char buffer[256];
    char percent_text[16];
    int battery_percent = s_pmu.isBatteryConnect() ? s_pmu.getBatteryPercent() : -1;
    snprintf(percent_text, sizeof(percent_text), "%s", s_pmu.isBatteryConnect() ? "" : "--");
    if (s_pmu.isBatteryConnect()) {
        snprintf(percent_text, sizeof(percent_text), "%d", battery_percent);
    }
    snprintf(
        buffer, sizeof(buffer),
        "Battery: %s\n"
        "Percent: %s\n"
        "VBUS: %u mV\n"
        "System: %u mV\n"
        "Temp: %.2f C\n"
        "Charging: %s",
        s_pmu.isBatteryConnect() ? "Connected" : "Not connected",
        percent_text,
        s_pmu.getVbusVoltage(),
        s_pmu.getSystemVoltage(),
        s_pmu.getTemperature(),
        s_pmu.isCharging() ? "YES" : "NO"
    );
    lv_label_set_text(s_info_label, buffer);
}

void power_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_power_label();
}

} // namespace

namespace esp_brookesia::apps {

class PowerApp: public systems::phone::App {
public:
    static PowerApp *requestInstance()
    {
        static PowerApp app;
        return &app;
    }

    PowerApp(): App(makeCoreConfig(APP_NAME, APP_ICON_COLOR), makePhoneConfig(1))
    {
    }

    bool run() override
    {
        lv_obj_t *title = lv_label_create(lv_screen_active());
        lv_label_set_text(title, "Device Care");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

        s_info_label = lv_label_create(lv_screen_active());
        lv_label_set_long_mode(s_info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(s_info_label, 360);
        lv_obj_align(s_info_label, LV_ALIGN_CENTER, 0, 16);
        lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_font(s_info_label, &lv_font_montserrat_20, 0);

        refresh_power_label();
        s_power_timer = lv_timer_create(power_timer_cb, 1000, nullptr);
        return true;
    }

    bool back() override
    {
        return notifyCoreClosed();
    }

    bool close() override
    {
        if (s_power_timer != nullptr) {
            lv_timer_delete(s_power_timer);
            s_power_timer = nullptr;
        }
        s_info_label = nullptr;
        return true;
    }
};

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, PowerApp, APP_NAME, []() {
    return std::shared_ptr<PowerApp>(PowerApp::requestInstance(), [](PowerApp *p) {
        (void)p;
    });
})

} // namespace esp_brookesia::apps
