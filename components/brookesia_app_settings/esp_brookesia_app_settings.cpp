/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cstdio>
#include <list>
#include <string>
#include <vector>
#include "esp_brookesia.hpp"
#include "settings_service.hpp"
#include "storybook_ui_helpers.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "AppSettings"
#include "esp_lib_utils.h"

using namespace esp_brookesia::systems;
using namespace esp_brookesia::gui;

namespace {

using storybook::DeviceInfoEntry;
using storybook::SettingsService;
using storybook::WifiNetwork;
using storybook::WifiState;

constexpr const char *APP_NAME = "Settings";
constexpr uint32_t APP_ICON_COLOR = 0xFF956B;

constexpr base::App::Config make_core_config()
{
    auto config = base::App::Config::SIMPLE_CONSTRUCTOR(APP_NAME, nullptr, true);
    config.launcher_icon = StyleImage::IMAGE_RECOLOR(&esp_brookesia_image_small_app_launcher_default_98_98, APP_ICON_COLOR);
    return config;
}

phone::App::Config make_phone_config()
{
    storybook::ui::PhoneAppChromeOptions options = {};
    options.show_status_bar = true;
    options.show_navigation_bar = false;
    options.launcher_page_index = 0;
    return storybook::ui::makePhoneAppConfig(options);
}

lv_obj_t *create_card(lv_obj_t *parent, uint32_t base_color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 28, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFDF8), 0);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(base_color), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xF0DAD0), 0);
    lv_obj_set_style_shadow_width(card, 18, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xE8DDD7), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(card, 18, 0);
    lv_obj_set_style_pad_row(card, 12, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    return card;
}

lv_obj_t *create_section_title(lv_obj_t *parent, const char *title)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x584A3C), 0);
    return label;
}

void create_info_entry(lv_obj_t *parent, const DeviceInfoEntry &entry)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_pad_ver(row, 6, 0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, entry.label.c_str());
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x8A7667), 0);

    lv_obj_t *value = lv_label_create(row);
    lv_label_set_text(value, entry.value.c_str());
    lv_label_set_long_mode(value, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(value, lv_pct(100));
    lv_obj_set_style_text_font(value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(value, lv_color_hex(0x4F4339), 0);
}

void update_percent_label(lv_obj_t *label, int value)
{
    if (label != nullptr) {
        lv_label_set_text_fmt(label, "%d%%", value);
    }
}

std::string format_voltage_text(uint16_t millivolt)
{
    char buffer[24] = {0};
    std::snprintf(buffer, sizeof(buffer), "%.2f V", static_cast<double>(millivolt) / 1000.0);
    return buffer;
}

} // namespace

namespace esp_brookesia::apps {

class SettingsApp final : public systems::phone::App {
public:
    static SettingsApp *requestInstance()
    {
        static SettingsApp instance;
        return &instance;
    }

    SettingsApp(): App(make_core_config(), make_phone_config())
    {
    }

    bool run() override
    {
        SettingsService &service = SettingsService::instance();
        ESP_UTILS_CHECK_ERROR_RETURN(service.begin(), false, "Init settings service failed");
        ESP_UTILS_CHECK_ERROR_RETURN(service.ensureWifiStarted(), false, "Start Wi-Fi service failed");

        lv_obj_t *screen = lv_screen_active();
        storybook::ui::applyScreenVerticalGradient(screen, 0xFFF7F0, 0xFFE7CB);

        storybook::ui::StatusBarBackdropStyle backdrop_style = {};
        backdrop_style.color_from = 0x8B573F;
        backdrop_style.color_to = 0xC47A57;
        backdrop_style.extra_height = 10;
        storybook::ui::createStatusBarBackdrop(screen, backdrop_style);

        storybook::ui::ColumnRootStyle root_style = {};
        root_style.side_padding = 14;
        root_style.top_padding = 14;
        root_style.bottom_padding = 14;
        root_style.row_padding = 12;
        root_style.reserve_status_bar = true;
        _root = storybook::ui::createColumnRoot(screen, root_style);

        buildHeroCard();
        buildDeviceInfoCard();
        buildDisplayCard();
        buildAudioCard();
        buildTimeCard();
        buildWifiCard();
        buildPasswordDialog(screen);
        buildManualTimeDialog(screen);

        refreshUi();
        if (service.getWifiScanResults().empty()) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(service.startWifiScan());
        }
        _refresh_timer = lv_timer_create(&SettingsApp::onRefreshTimer, 1000, this);

        return true;
    }

    bool back() override
    {
        hidePasswordDialog();
        hideManualTimeDialog();
        return notifyCoreClosed();
    }

    bool close() override
    {
        _root = nullptr;
        _device_info_card = nullptr;
        _device_info_content = nullptr;
        _device_info_summary_label = nullptr;
        _device_info_toggle_label = nullptr;
        _brightness_slider = nullptr;
        _brightness_value = nullptr;
        _volume_slider = nullptr;
        _volume_value = nullptr;
        _wifi_status_label = nullptr;
        _wifi_list = nullptr;
        _scan_button_label = nullptr;
        _time_status_label = nullptr;
        _current_time_label = nullptr;
        _auto_time_switch = nullptr;
        _password_overlay = nullptr;
        _password_title = nullptr;
        _password_textarea = nullptr;
        _password_keyboard = nullptr;
        _manual_time_overlay = nullptr;
        _manual_date_textarea = nullptr;
        _manual_time_textarea = nullptr;
        _manual_time_keyboard = nullptr;
        _refresh_timer = nullptr;
        _selected_ssid.clear();
        _device_info_signature.clear();
        _device_info_expanded = false;
        _wifi_signature.clear();
        _wifi_buttons.clear();
        return true;
    }

private:
    struct WifiButtonInfo {
        std::string ssid;
        bool is_open = false;
        lv_obj_t *button = nullptr;
    };

    void buildHeroCard()
    {
        lv_obj_t *hero = create_card(_root, 0xFFE6D5);
        lv_obj_t *title = lv_label_create(hero);
        lv_label_set_text(title, "Settings");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x5B4635), 0);

        lv_obj_t *subtitle = lv_label_create(hero);
        lv_label_set_text(subtitle, "Wi-Fi, device info, brightness, volume, and device time");
        lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0x7B6654), 0);
    }

    std::string buildDeviceInfoSignature() const
    {
        std::string signature;
        for (const auto &entry : SettingsService::instance().getDeviceInfoEntries()) {
            signature += entry.label;
            signature += "=";
            signature += entry.value;
            signature += "|";
        }
        return signature;
    }

    std::string buildDeviceInfoSummary() const
    {
        storybook::PowerStatus power = SettingsService::instance().getPowerStatus();
        if (!power.available) {
            return "Tap to view device details";
        }
        if (!power.battery_present) {
            return power.external_power_present ? "USB power connected" : "Tap to view device details";
        }

        std::string summary = "Battery ";
        summary += std::to_string(power.battery_percent);
        summary += "% | ";
        summary += format_voltage_text(power.battery_voltage_mv);
        if (power.charging) {
            summary += " | Charging";
        }
        return summary;
    }

    void syncDeviceInfoVisibility()
    {
        if (_device_info_content != nullptr) {
            if (_device_info_expanded) {
                lv_obj_clear_flag(_device_info_content, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(_device_info_content, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (_device_info_toggle_label != nullptr) {
            lv_label_set_text(_device_info_toggle_label, _device_info_expanded ? "Hide" : "Show");
        }
        if (_device_info_summary_label != nullptr) {
            lv_label_set_text(_device_info_summary_label, buildDeviceInfoSummary().c_str());
        }
    }

    void rebuildDeviceInfoCard()
    {
        if (_device_info_card == nullptr) {
            return;
        }

        lv_obj_clean(_device_info_card);

        lv_obj_t *header = lv_button_create(_device_info_card);
        lv_obj_set_width(header, lv_pct(100));
        lv_obj_set_style_radius(header, 22, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0xFFF7F1), 0);
        lv_obj_set_style_bg_grad_color(header, lv_color_hex(0xFCEBDF), 0);
        lv_obj_set_style_bg_grad_dir(header, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_border_width(header, 1, 0);
        lv_obj_set_style_border_color(header, lv_color_hex(0xE8D3C5), 0);
        lv_obj_set_style_pad_all(header, 14, 0);
        lv_obj_set_style_pad_row(header, 6, 0);
        lv_obj_set_layout(header, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_add_event_cb(header, &SettingsApp::onToggleDeviceInfo, LV_EVENT_CLICKED, this);

        lv_obj_t *title = lv_label_create(header);
        lv_label_set_text(title, "Device Info");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x584A3C), 0);
        lv_obj_set_flex_grow(title, 1);

        _device_info_toggle_label = lv_label_create(header);
        lv_obj_set_style_text_font(_device_info_toggle_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_device_info_toggle_label, lv_color_hex(0x8A7667), 0);

        _device_info_summary_label = lv_label_create(header);
        lv_obj_set_width(_device_info_summary_label, lv_pct(100));
        lv_label_set_long_mode(_device_info_summary_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(_device_info_summary_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_device_info_summary_label, lv_color_hex(0x8A7667), 0);

        _device_info_content = lv_obj_create(_device_info_card);
        lv_obj_remove_style_all(_device_info_content);
        lv_obj_set_width(_device_info_content, lv_pct(100));
        lv_obj_set_height(_device_info_content, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_top(_device_info_content, 8, 0);
        lv_obj_set_style_pad_row(_device_info_content, 2, 0);
        lv_obj_set_layout(_device_info_content, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(_device_info_content, LV_FLEX_FLOW_COLUMN);

        for (const auto &entry : SettingsService::instance().getDeviceInfoEntries()) {
            create_info_entry(_device_info_content, entry);
        }

        syncDeviceInfoVisibility();
    }

    void buildDeviceInfoCard()
    {
        _device_info_card = create_card(_root, 0xF7E7D8);
        _device_info_signature.clear();
        rebuildDeviceInfoCard();
    }

    void buildDisplayCard()
    {
        lv_obj_t *card = create_card(_root, 0xFFF3CE);
        create_section_title(card, "Display");

        _brightness_slider = lv_slider_create(card);
        lv_slider_set_range(_brightness_slider, 0, 100);
        lv_slider_set_value(_brightness_slider, SettingsService::instance().getBrightness(), LV_ANIM_OFF);
        lv_obj_add_event_cb(_brightness_slider, &SettingsApp::onBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

        _brightness_value = lv_label_create(card);
        update_percent_label(_brightness_value, SettingsService::instance().getBrightness());
    }

    void buildAudioCard()
    {
        lv_obj_t *card = create_card(_root, 0xDDF3E7);
        create_section_title(card, "Audio");

        _volume_slider = lv_slider_create(card);
        lv_slider_set_range(_volume_slider, 0, 100);
        lv_slider_set_value(_volume_slider, SettingsService::instance().getVolume(), LV_ANIM_OFF);
        lv_obj_add_event_cb(_volume_slider, &SettingsApp::onVolumeChanged, LV_EVENT_VALUE_CHANGED, this);

        _volume_value = lv_label_create(card);
        update_percent_label(_volume_value, SettingsService::instance().getVolume());
    }

    void buildTimeCard()
    {
        lv_obj_t *card = create_card(_root, 0xE2E9FF);
        create_section_title(card, "Time");

        _time_status_label = lv_label_create(card);
        lv_obj_set_style_text_font(_time_status_label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(_time_status_label, lv_color_hex(0x675A79), 0);

        _current_time_label = lv_label_create(card);
        lv_obj_set_style_text_font(_current_time_label, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(_current_time_label, lv_color_hex(0x4A4766), 0);

        _auto_time_switch = lv_switch_create(card);
        if (SettingsService::instance().getAutoTimeSync()) {
            lv_obj_add_state(_auto_time_switch, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(_auto_time_switch, &SettingsApp::onAutoTimeChanged, LV_EVENT_VALUE_CHANGED, this);

        lv_obj_t *manual_button = lv_button_create(card);
        lv_obj_set_style_bg_color(manual_button, lv_color_hex(0x98B5FF), 0);
        lv_obj_add_event_cb(manual_button, &SettingsApp::onShowManualTimeDialog, LV_EVENT_CLICKED, this);
        lv_obj_t *manual_label = lv_label_create(manual_button);
        lv_label_set_text(manual_label, "Set Manual Time");
        lv_obj_center(manual_label);
    }

    void buildWifiCard()
    {
        lv_obj_t *card = create_card(_root, 0xFFDDE6);
        create_section_title(card, "Wi-Fi");

        _wifi_status_label = lv_label_create(card);
        lv_obj_set_style_text_font(_wifi_status_label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(_wifi_status_label, lv_color_hex(0x675A79), 0);

        lv_obj_t *scan_button = lv_button_create(card);
        lv_obj_set_style_bg_color(scan_button, lv_color_hex(0xFF9D86), 0);
        lv_obj_add_event_cb(scan_button, &SettingsApp::onStartWifiScan, LV_EVENT_CLICKED, this);
        _scan_button_label = lv_label_create(scan_button);
        lv_label_set_text(_scan_button_label, "Scan Wi-Fi");
        lv_obj_center(_scan_button_label);

        _wifi_list = lv_obj_create(card);
        lv_obj_set_width(_wifi_list, lv_pct(100));
        lv_obj_set_height(_wifi_list, 220);
        lv_obj_set_style_radius(_wifi_list, 22, 0);
        lv_obj_set_style_bg_color(_wifi_list, lv_color_hex(0xFFFDFC), 0);
        lv_obj_set_style_pad_all(_wifi_list, 10, 0);
        lv_obj_set_style_pad_row(_wifi_list, 8, 0);
        lv_obj_set_layout(_wifi_list, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(_wifi_list, LV_FLEX_FLOW_COLUMN);
    }

    void buildPasswordDialog(lv_obj_t *screen)
    {
        _password_overlay = lv_obj_create(screen);
        lv_obj_remove_style_all(_password_overlay);
        lv_obj_set_size(_password_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(_password_overlay, lv_color_hex(0x5B4C45), 0);
        lv_obj_set_style_bg_opa(_password_overlay, LV_OPA_60, 0);
        lv_obj_clear_flag(_password_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_password_overlay, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *panel = lv_obj_create(_password_overlay);
        lv_obj_set_size(panel, lv_pct(92), lv_pct(88));
        lv_obj_center(panel);
        lv_obj_set_style_radius(panel, 26, 0);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFF9F3), 0);
        lv_obj_set_style_pad_all(panel, 14, 0);
        lv_obj_set_style_pad_row(panel, 10, 0);
        lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

        _password_title = lv_label_create(panel);
        lv_label_set_text(_password_title, "Wi-Fi Password");
        lv_obj_set_style_text_font(_password_title, &lv_font_montserrat_22, 0);

        _password_textarea = lv_textarea_create(panel);
        lv_obj_set_size(_password_textarea, lv_pct(100), 50);
        lv_textarea_set_placeholder_text(_password_textarea, "Type password");
        lv_textarea_set_password_mode(_password_textarea, true);
        lv_textarea_set_one_line(_password_textarea, true);
        lv_textarea_set_password_show_time(_password_textarea, 0);
        lv_obj_add_event_cb(_password_textarea, &SettingsApp::onPasswordFieldFocused, LV_EVENT_FOCUSED, this);

        lv_obj_t *button_row = lv_obj_create(panel);
        lv_obj_remove_style_all(button_row);
        lv_obj_set_width(button_row, lv_pct(100));
        lv_obj_set_layout(button_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(button_row, 10, 0);

        lv_obj_t *cancel_button = lv_button_create(button_row);
        lv_obj_set_flex_grow(cancel_button, 1);
        lv_obj_add_event_cb(cancel_button, &SettingsApp::onCancelPasswordDialog, LV_EVENT_CLICKED, this);
        lv_obj_t *cancel_label = lv_label_create(cancel_button);
        lv_label_set_text(cancel_label, "Cancel");
        lv_obj_center(cancel_label);

        lv_obj_t *connect_button = lv_button_create(button_row);
        lv_obj_set_flex_grow(connect_button, 1);
        lv_obj_set_style_bg_color(connect_button, lv_color_hex(0xFFAA91), 0);
        lv_obj_add_event_cb(connect_button, &SettingsApp::onConfirmPasswordDialog, LV_EVENT_CLICKED, this);
        lv_obj_t *connect_label = lv_label_create(connect_button);
        lv_label_set_text(connect_label, "Connect");
        lv_obj_center(connect_label);

        _password_keyboard = lv_keyboard_create(panel);
        lv_obj_set_size(_password_keyboard, lv_pct(100), 160);
        lv_keyboard_set_textarea(_password_keyboard, _password_textarea);
        lv_obj_add_event_cb(_password_keyboard, &SettingsApp::onPasswordKeyboardEvent, LV_EVENT_ALL, this);
    }

    void buildManualTimeDialog(lv_obj_t *screen)
    {
        _manual_time_overlay = lv_obj_create(screen);
        lv_obj_remove_style_all(_manual_time_overlay);
        lv_obj_set_size(_manual_time_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(_manual_time_overlay, lv_color_hex(0x46535E), 0);
        lv_obj_set_style_bg_opa(_manual_time_overlay, LV_OPA_60, 0);
        lv_obj_clear_flag(_manual_time_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_manual_time_overlay, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *panel = lv_obj_create(_manual_time_overlay);
        lv_obj_set_size(panel, lv_pct(92), lv_pct(94));
        lv_obj_center(panel);
        lv_obj_set_style_radius(panel, 26, 0);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0xF7FAFF), 0);
        lv_obj_set_style_pad_all(panel, 14, 0);
        lv_obj_set_style_pad_row(panel, 10, 0);
        lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(panel);
        lv_label_set_text(title, "Manual Time");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);

        lv_obj_t *hint = lv_label_create(panel);
        lv_label_set_text(hint, "Date: YYYY-MM-DD\nTime: HH:MM");
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x6A7280), 0);

        _manual_date_textarea = lv_textarea_create(panel);
        lv_obj_set_size(_manual_date_textarea, lv_pct(100), 48);
        lv_textarea_set_placeholder_text(_manual_date_textarea, "2026-03-29");
        lv_textarea_set_one_line(_manual_date_textarea, true);
        lv_obj_add_event_cb(_manual_date_textarea, &SettingsApp::onManualTimeFieldFocused, LV_EVENT_FOCUSED, this);

        _manual_time_textarea = lv_textarea_create(panel);
        lv_obj_set_size(_manual_time_textarea, lv_pct(100), 48);
        lv_textarea_set_placeholder_text(_manual_time_textarea, "14:30");
        lv_textarea_set_one_line(_manual_time_textarea, true);
        lv_obj_add_event_cb(_manual_time_textarea, &SettingsApp::onManualTimeFieldFocused, LV_EVENT_FOCUSED, this);

        lv_obj_t *button_row = lv_obj_create(panel);
        lv_obj_remove_style_all(button_row);
        lv_obj_set_width(button_row, lv_pct(100));
        lv_obj_set_layout(button_row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(button_row, 10, 0);

        lv_obj_t *cancel_button = lv_button_create(button_row);
        lv_obj_set_flex_grow(cancel_button, 1);
        lv_obj_add_event_cb(cancel_button, &SettingsApp::onCancelManualTimeDialog, LV_EVENT_CLICKED, this);
        lv_obj_t *cancel_label = lv_label_create(cancel_button);
        lv_label_set_text(cancel_label, "Cancel");
        lv_obj_center(cancel_label);

        lv_obj_t *apply_button = lv_button_create(button_row);
        lv_obj_set_flex_grow(apply_button, 1);
        lv_obj_set_style_bg_color(apply_button, lv_color_hex(0x98B5FF), 0);
        lv_obj_add_event_cb(apply_button, &SettingsApp::onConfirmManualTimeDialog, LV_EVENT_CLICKED, this);
        lv_obj_t *apply_label = lv_label_create(apply_button);
        lv_label_set_text(apply_label, "Apply");
        lv_obj_center(apply_label);

        _manual_time_keyboard = lv_keyboard_create(panel);
        lv_obj_set_size(_manual_time_keyboard, lv_pct(100), 160);
        lv_keyboard_set_textarea(_manual_time_keyboard, _manual_date_textarea);
        lv_obj_add_event_cb(_manual_time_keyboard, &SettingsApp::onManualTimeKeyboardEvent, LV_EVENT_ALL, this);
    }

    void refreshDeviceInfoCard()
    {
        std::string signature = buildDeviceInfoSignature();
        if (signature != _device_info_signature) {
            _device_info_signature = signature;
            rebuildDeviceInfoCard();
        } else {
            syncDeviceInfoVisibility();
        }
    }

    void refreshUi()
    {
        refreshDeviceInfoCard();
        refreshTimeCard();
        refreshWifiCard();
    }

    void refreshTimeCard()
    {
        SettingsService &service = SettingsService::instance();
        if (_current_time_label != nullptr) {
            lv_label_set_text_fmt(
                _current_time_label, "%s %s", service.getCurrentDateText().c_str(), service.getCurrentTimeText().c_str()
            );
        }

        if (_time_status_label != nullptr) {
            if (service.getAutoTimeSync()) {
                lv_label_set_text(
                    _time_status_label,
                    service.isTimeSynced() ? "Internet time is ready" : "Waiting for Wi-Fi time sync"
                );
            } else {
                lv_label_set_text(_time_status_label, "Manual time mode");
            }
        }
    }

    std::string buildWifiSignature() const
    {
        SettingsService &service = SettingsService::instance();
        std::string signature = std::to_string(static_cast<int>(service.getWifiState()));
        signature += "|";
        signature += service.getConnectedSsid();

        for (const auto &network : service.getWifiScanResults()) {
            signature += "|";
            signature += network.ssid;
            signature += ":";
            signature += std::to_string(network.rssi);
        }

        return signature;
    }

    void rebuildWifiList()
    {
        if (_wifi_list == nullptr) {
            return;
        }

        SettingsService &service = SettingsService::instance();
        lv_obj_clean(_wifi_list);
        _wifi_buttons.clear();

        if (service.isWifiScanning()) {
            lv_obj_t *label = lv_label_create(_wifi_list);
            lv_label_set_text(label, "Looking for nearby networks...");
            return;
        }

        std::vector<WifiNetwork> networks = service.getWifiScanResults();
        if (networks.empty()) {
            lv_obj_t *label = lv_label_create(_wifi_list);
            lv_label_set_text(label, "No networks yet.\nTap Scan Wi-Fi.");
            return;
        }

        for (const auto &network : networks) {
            _wifi_buttons.push_back({network.ssid, network.isOpen(), nullptr});
            WifiButtonInfo &button_info = _wifi_buttons.back();

            button_info.button = lv_button_create(_wifi_list);
            lv_obj_set_width(button_info.button, lv_pct(100));
            lv_obj_set_style_radius(button_info.button, 18, 0);
            lv_obj_set_style_bg_color(button_info.button, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_grad_color(button_info.button, lv_color_hex(0xFFF0E8), 0);
            lv_obj_set_style_bg_grad_dir(button_info.button, LV_GRAD_DIR_HOR, 0);
            lv_obj_set_style_border_width(button_info.button, 1, 0);
            lv_obj_set_style_border_color(button_info.button, lv_color_hex(0xEFD8CE), 0);
            lv_obj_set_style_pad_all(button_info.button, 12, 0);
            lv_obj_add_event_cb(button_info.button, &SettingsApp::onWifiNetworkSelected, LV_EVENT_CLICKED, this);
            lv_obj_set_user_data(button_info.button, &button_info);

            lv_obj_t *label = lv_label_create(button_info.button);
            lv_label_set_text_fmt(
                label, "%s  %s  (%ddBm)", network.ssid.c_str(), network.isOpen() ? "Open" : "Secure", network.rssi
            );
            lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(0x5B4A44), 0);
        }
    }

    void refreshWifiCard()
    {
        SettingsService &service = SettingsService::instance();
        if (_wifi_status_label != nullptr) {
            lv_label_set_text(_wifi_status_label, service.getWifiStatusText().c_str());
        }
        if (_scan_button_label != nullptr) {
            lv_label_set_text(_scan_button_label, service.isWifiScanning() ? "Scanning..." : "Scan Wi-Fi");
        }

        std::string signature = buildWifiSignature();
        if (signature != _wifi_signature) {
            _wifi_signature = signature;
            rebuildWifiList();
        }
    }

    void showPasswordDialog(const std::string &ssid)
    {
        _selected_ssid = ssid;
        lv_label_set_text_fmt(_password_title, "Password for %s", ssid.c_str());
        lv_textarea_set_text(_password_textarea, "");
        lv_keyboard_set_textarea(_password_keyboard, _password_textarea);
        lv_obj_move_foreground(_password_overlay);
        lv_obj_clear_flag(_password_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(_password_textarea, LV_STATE_FOCUSED);
        lv_obj_scroll_to_view(_password_textarea, LV_ANIM_OFF);
    }

    void hidePasswordDialog()
    {
        if (_password_overlay != nullptr) {
            lv_obj_add_flag(_password_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        _selected_ssid.clear();
    }

    void showManualTimeDialog()
    {
        SettingsService &service = SettingsService::instance();
        lv_textarea_set_text(_manual_date_textarea, service.getCurrentDateText().c_str());
        lv_textarea_set_text(_manual_time_textarea, service.getCurrentTimeText().c_str());
        lv_keyboard_set_textarea(_manual_time_keyboard, _manual_date_textarea);
        lv_obj_move_foreground(_manual_time_overlay);
        lv_obj_clear_flag(_manual_time_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(_manual_date_textarea, LV_STATE_FOCUSED);
        lv_obj_scroll_to_view(_manual_date_textarea, LV_ANIM_OFF);
    }

    void hideManualTimeDialog()
    {
        if (_manual_time_overlay != nullptr) {
            lv_obj_add_flag(_manual_time_overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }

    static SettingsApp *from_event(lv_event_t *event)
    {
        return static_cast<SettingsApp *>(lv_event_get_user_data(event));
    }

    static void onBrightnessChanged(lv_event_t *event)
    {
        auto *self = from_event(event);
        int value = lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(event)));
        ESP_ERROR_CHECK_WITHOUT_ABORT(SettingsService::instance().setBrightness(value));
        update_percent_label(self->_brightness_value, value);
    }

    static void onVolumeChanged(lv_event_t *event)
    {
        auto *self = from_event(event);
        int value = lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(event)));
        ESP_ERROR_CHECK_WITHOUT_ABORT(SettingsService::instance().setVolume(value));
        update_percent_label(self->_volume_value, value);
    }

    static void onAutoTimeChanged(lv_event_t *event)
    {
        auto *self = from_event(event);
        bool enabled = lv_obj_has_state(static_cast<lv_obj_t *>(lv_event_get_target(event)), LV_STATE_CHECKED);
        ESP_ERROR_CHECK_WITHOUT_ABORT(SettingsService::instance().setAutoTimeSync(enabled));
        self->refreshTimeCard();
    }

    static void onShowManualTimeDialog(lv_event_t *event)
    {
        auto *self = from_event(event);
        if (SettingsService::instance().getAutoTimeSync()) {
            lv_label_set_text(self->_time_status_label, "Disable Auto Sync before manual set");
            return;
        }
        self->showManualTimeDialog();
    }

    static void onManualTimeFieldFocused(lv_event_t *event)
    {
        auto *self = from_event(event);
        lv_keyboard_set_textarea(
            self->_manual_time_keyboard, static_cast<lv_obj_t *>(lv_event_get_target(event))
        );
    }

    static void onManualTimeKeyboardEvent(lv_event_t *event)
    {
        auto *self = from_event(event);
        uint32_t code = lv_event_get_code(event);
        if (code == LV_EVENT_CANCEL) {
            self->hideManualTimeDialog();
        } else if (code == LV_EVENT_READY) {
            onConfirmManualTimeDialog(event);
        }
    }

    static void onCancelManualTimeDialog(lv_event_t *event)
    {
        auto *self = from_event(event);
        self->hideManualTimeDialog();
    }

    static void onConfirmManualTimeDialog(lv_event_t *event)
    {
        auto *self = from_event(event);
        esp_err_t ret = SettingsService::instance().setManualDateTime(
            lv_textarea_get_text(self->_manual_date_textarea), lv_textarea_get_text(self->_manual_time_textarea)
        );
        if (ret == ESP_OK) {
            self->hideManualTimeDialog();
            self->refreshTimeCard();
        } else {
            lv_label_set_text(self->_time_status_label, "Manual time format invalid");
        }
    }

    static void onStartWifiScan(lv_event_t *event)
    {
        auto *self = from_event(event);
        ESP_ERROR_CHECK_WITHOUT_ABORT(SettingsService::instance().startWifiScan());
        self->refreshWifiCard();
    }

    static void onToggleDeviceInfo(lv_event_t *event)
    {
        auto *self = from_event(event);
        self->_device_info_expanded = !self->_device_info_expanded;
        self->syncDeviceInfoVisibility();
    }

    static void onWifiNetworkSelected(lv_event_t *event)
    {
        auto *self = from_event(event);
        auto *button = static_cast<lv_obj_t *>(lv_event_get_target(event));
        auto *info = static_cast<WifiButtonInfo *>(lv_obj_get_user_data(button));
        if (info == nullptr) {
            return;
        }

        if (info->is_open) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(SettingsService::instance().connectWifi(info->ssid, ""));
            self->refreshWifiCard();
            return;
        }

        self->showPasswordDialog(info->ssid);
    }

    static void onPasswordFieldFocused(lv_event_t *event)
    {
        auto *self = from_event(event);
        lv_keyboard_set_textarea(
            self->_password_keyboard, static_cast<lv_obj_t *>(lv_event_get_target(event))
        );
    }

    static void onPasswordKeyboardEvent(lv_event_t *event)
    {
        auto *self = from_event(event);
        uint32_t code = lv_event_get_code(event);
        if (code == LV_EVENT_CANCEL) {
            self->hidePasswordDialog();
        } else if (code == LV_EVENT_READY) {
            onConfirmPasswordDialog(event);
        }
    }

    static void onCancelPasswordDialog(lv_event_t *event)
    {
        auto *self = from_event(event);
        self->hidePasswordDialog();
    }

    static void onConfirmPasswordDialog(lv_event_t *event)
    {
        auto *self = from_event(event);
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            SettingsService::instance().connectWifi(self->_selected_ssid, lv_textarea_get_text(self->_password_textarea))
        );
        self->hidePasswordDialog();
        self->refreshWifiCard();
    }

    static void onRefreshTimer(lv_timer_t *timer)
    {
        auto *self = static_cast<SettingsApp *>(timer->user_data);
        if (self != nullptr) {
            self->refreshUi();
        }
    }

    lv_obj_t *_root = nullptr;
    lv_obj_t *_device_info_card = nullptr;
    lv_obj_t *_device_info_content = nullptr;
    lv_obj_t *_device_info_summary_label = nullptr;
    lv_obj_t *_device_info_toggle_label = nullptr;
    lv_obj_t *_brightness_slider = nullptr;
    lv_obj_t *_brightness_value = nullptr;
    lv_obj_t *_volume_slider = nullptr;
    lv_obj_t *_volume_value = nullptr;
    lv_obj_t *_wifi_status_label = nullptr;
    lv_obj_t *_wifi_list = nullptr;
    lv_obj_t *_scan_button_label = nullptr;
    lv_obj_t *_time_status_label = nullptr;
    lv_obj_t *_current_time_label = nullptr;
    lv_obj_t *_auto_time_switch = nullptr;
    lv_obj_t *_password_overlay = nullptr;
    lv_obj_t *_password_title = nullptr;
    lv_obj_t *_password_textarea = nullptr;
    lv_obj_t *_password_keyboard = nullptr;
    lv_obj_t *_manual_time_overlay = nullptr;
    lv_obj_t *_manual_date_textarea = nullptr;
    lv_obj_t *_manual_time_textarea = nullptr;
    lv_obj_t *_manual_time_keyboard = nullptr;
    lv_timer_t *_refresh_timer = nullptr;
    std::string _selected_ssid;
    std::string _device_info_signature;
    bool _device_info_expanded = false;
    std::string _wifi_signature;
    std::list<WifiButtonInfo> _wifi_buttons;
};

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, SettingsApp, APP_NAME, []() {
    return std::shared_ptr<SettingsApp>(SettingsApp::requestInstance(), [](SettingsApp *app) {
        (void)app;
    });
})

} // namespace esp_brookesia::apps










