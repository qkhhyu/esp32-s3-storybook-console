/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <cstring>
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "esp_brookesia.hpp"
#include "boost/thread.hpp"
#include "suite_settings_service.hpp"
#include "suite_ui_font_service.hpp"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "SuiteMain"
#include "esp_lib_utils.h"
#include "./dark/stylesheet.hpp"

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;

namespace {

constexpr uint32_t PMU_I2C_TIMEOUT_MS = 1000;
constexpr uint32_t PMU_I2C_FREQ = 100000;
constexpr uint8_t PMU_I2C_ADDRESS = 0x34;
constexpr uint32_t DISPLAY_BUFFER_LINES = 8;
constexpr uint32_t DISPLAY_BUFFER_PIXELS = BSP_LCD_H_RES * DISPLAY_BUFFER_LINES;
constexpr uint32_t DISPLAY_TRANSFER_BYTES = DISPLAY_BUFFER_PIXELS * sizeof(uint16_t);
static i2c_master_dev_handle_t s_boot_pmu_dev = nullptr;

int pmu_register_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    (void)dev_addr;
    esp_err_t ret = i2c_master_transmit_receive(s_boot_pmu_dev, &reg_addr, 1, data, len, PMU_I2C_TIMEOUT_MS);
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
    esp_err_t ret = i2c_master_transmit(s_boot_pmu_dev, buffer, len + 1, PMU_I2C_TIMEOUT_MS);
    return (ret == ESP_OK) ? 0 : -1;
}

bool recover_board_power()
{
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
    ESP_UTILS_CHECK_NULL_RETURN(i2c_bus, false, "Get board I2C bus failed");

    if (s_boot_pmu_dev == nullptr) {
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
            i2c_master_bus_add_device(i2c_bus, &dev_config, &s_boot_pmu_dev), false, "Create PMU I2C device failed"
        );
    }

    XPowersPMU pmu;
    ESP_UTILS_CHECK_FALSE_RETURN(
        pmu.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte), false, "Init PMU failed"
    );

    pmu.setDC1Voltage(3300);
    pmu.enableDC1();
    pmu.setALDO1Voltage(3300);
    pmu.enableALDO1();
    pmu.enableVbusVoltageMeasure();
    pmu.enableBattVoltageMeasure();
    pmu.enableSystemVoltageMeasure();
    pmu.enableTemperatureMeasure();
    pmu.disableTSPinMeasure();

    return true;
}

} // namespace

#define LVGL_PORT_INIT_CONFIG() \
    {                           \
        .task_priority = 4,     \
        .task_stack = 9 * 1024, \
        .task_affinity = -1,    \
        .task_max_sleep_ms = 500,\
        .task_stack_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, \
        .timer_period_ms = 5,   \
    }

extern "C" void app_main(void)
{
    ESP_UTILS_LOGI("Display ESP32-S3 Storybook Console demo");
    ESP_UTILS_CHECK_FALSE_EXIT(recover_board_power(), "Recover board power failed");

    // Keep the display on the project's proven small DMA buffer path. It costs
    // some internal SRAM, but the BSP default PSRAM canvas stalled flush
    // completion on this hardware/app combination.
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = LVGL_PORT_INIT_CONFIG(),
        .buffer_size = DISPLAY_BUFFER_PIXELS,
        .trans_size = DISPLAY_TRANSFER_BYTES,
        .double_buffer = false,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        },
    };
    ESP_UTILS_CHECK_NULL_EXIT(bsp_display_start_with_config(&cfg), "Start display failed");
    ESP_UTILS_CHECK_ERROR_EXIT(bsp_display_backlight_on(), "Turn on display backlight failed");
    ESP_UTILS_LOGI(
        "Heap after display: free=%u internal=%u largest_internal=%u psram=%u",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))
    );
    esp_err_t settings_ret = suite::SettingsService::instance().begin();
    if (settings_ret != ESP_OK) {
        ESP_UTILS_LOGW("Init settings service degraded [%s]", esp_err_to_name(settings_ret));
    }

    esp_err_t font_ret = suite::UiFontService::instance().begin();
    if (font_ret != ESP_OK) {
        ESP_UTILS_LOGW("Preload Chinese font degraded [%s]", esp_err_to_name(font_ret));
    }

    LvLock::registerCallbacks([](int timeout_ms) {
        if (timeout_ms < 0) {
            timeout_ms = 0;
        } else if (timeout_ms == 0) {
            timeout_ms = 1;
        }
        ESP_UTILS_CHECK_FALSE_RETURN(bsp_display_lock(timeout_ms), false, "Lock failed");
        return true;
    }, []() {
        bsp_display_unlock();
        return true;
    });

    Phone *phone = new (std::nothrow) Phone();
    ESP_UTILS_CHECK_NULL_EXIT(phone, "Create phone failed");

    if ((BSP_LCD_H_RES == 410) && (BSP_LCD_V_RES == 502)) {
        Stylesheet *stylesheet = new (std::nothrow) Stylesheet(STYLESHEET_410_502_DARK);
        ESP_UTILS_CHECK_NULL_EXIT(stylesheet, "Create stylesheet failed");

        ESP_UTILS_LOGI("Using stylesheet (%s)", stylesheet->core.name);
        ESP_UTILS_CHECK_FALSE_EXIT(phone->addStylesheet(stylesheet), "Add stylesheet failed");
        ESP_UTILS_CHECK_FALSE_EXIT(phone->activateStylesheet(stylesheet), "Activate stylesheet failed");
        delete stylesheet;
    }

    {
        LvLockGuard gui_guard;

        ESP_UTILS_CHECK_FALSE_EXIT(phone->begin(), "Begin failed");

        std::vector<systems::base::Manager::RegistryAppInfo> inited_apps;
        ESP_UTILS_CHECK_FALSE_EXIT(phone->initAppFromRegistry(inited_apps), "Init app registry failed");
        ESP_UTILS_LOGI("Registry initialized apps: %u", static_cast<unsigned>(inited_apps.size()));
        ESP_UTILS_CHECK_FALSE_EXIT(phone->installAppFromRegistry(inited_apps), "Install app registry failed");
        ESP_UTILS_LOGI("Registry installed apps complete");

        lv_timer_create([](lv_timer_t *t) {
            time_t now;
            struct tm timeinfo;
            Phone *phone = static_cast<Phone *>(t->user_data);
            suite::SettingsService &settings = suite::SettingsService::instance();
            StatusBar *status_bar = nullptr;

            ESP_UTILS_CHECK_NULL_EXIT(phone, "Invalid phone");
            status_bar = phone->getDisplay().getStatusBar();
            ESP_UTILS_CHECK_NULL_EXIT(status_bar, "Invalid status bar");

            time(&now);
            localtime_r(&now, &timeinfo);

            if (settings.isTimeSynced()) {
                ESP_UTILS_CHECK_FALSE_EXIT(status_bar->setClock(timeinfo.tm_hour, timeinfo.tm_min), "Refresh clock failed");
            }

            StatusBar::WifiState wifi_state = StatusBar::WifiState::DISCONNECTED;
            switch (settings.getWifiSignalLevel()) {
            case 3:
                wifi_state = StatusBar::WifiState::SIGNAL_3;
                break;
            case 2:
                wifi_state = StatusBar::WifiState::SIGNAL_2;
                break;
            case 1:
                wifi_state = StatusBar::WifiState::SIGNAL_1;
                break;
            default:
                wifi_state = StatusBar::WifiState::DISCONNECTED;
                break;
            }
            ESP_UTILS_CHECK_FALSE_EXIT(status_bar->setWifiIconState(wifi_state), "Refresh wifi failed");
        }, 1000, phone);
    }

}
