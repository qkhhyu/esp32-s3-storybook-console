/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <list>
#include <string>
#include <vector>
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "esp_brookesia.hpp"
#include "suite_settings_service.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "AppSettings"
#include "esp_lib_utils.h"

using namespace esp_brookesia::systems;
using namespace esp_brookesia::gui;

namespace {

using SettingsService = suite::SettingsService;
using WifiNetwork = suite::WifiNetwork;

constexpr const char *APP_NAME = "Parent Care";
constexpr uint32_t SETTINGS_ICON_COLOR = 0xFF9F7A;

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_brightness_slider = nullptr;
static lv_obj_t *s_brightness_value = nullptr;
static lv_obj_t *s_volume_slider = nullptr;
static lv_obj_t *s_volume_value = nullptr;
static lv_obj_t *s_wifi_status_label = nullptr;
static lv_obj_t *s_time_status_label = nullptr;
static lv_obj_t *s_wifi_list = nullptr;
static lv_obj_t *s_auto_time_switch = nullptr;
static lv_obj_t *s_scan_button_label = nullptr;
static lv_obj_t *s_password_overlay = nullptr;
static lv_obj_t *s_password_title = nullptr;
static lv_obj_t *s_password_textarea = nullptr;
static lv_obj_t *s_keyboard = nullptr;
static lv_timer_t *s_refresh_timer = nullptr;
static std::string s_selected_ssid;
static std::string s_wifi_signature;

struct WifiButtonInfo {
    std::string ssid;
    bool is_open = false;
    lv_obj_t *button = nullptr;
};

static std::list<WifiButtonInfo> s_wifi_buttons;

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

lv_obj_t *create_card(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 28, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFDF8), 0);
    lv_obj_set_style_bg_grad_color(card, color, 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_mix(color, lv_color_white(), LV_OPA_60), 0);
    lv_obj_set_style_shadow_width(card, 18, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xE8D9FF), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
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
    lv_obj_set_style_text_color(label, lv_color_hex(0x5B4B7A), 0);
    return label;
}

void update_value_label(lv_obj_t *label, int value, const char *suffix)
{
    if (label == nullptr) {
        return;
    }
    lv_label_set_text_fmt(label, "%d%s", value, suffix);
}

std::string build_wifi_signature()
{
    SettingsService &service = SettingsService::instance();
    std::vector<WifiNetwork> networks = service.getWifiScanResults();
    std::string signature = service.isWifiScanning() ? "scan:" : "idle:";
    signature += service.getConnectedSsid();
    for (const auto &network : networks) {
        signature += "|";
        signature += network.ssid;
        signature += ":";
        signature += std::to_string(network.rssi);
    }
    return signature;
}

void hide_password_dialog()
{
    if (s_password_overlay == nullptr) {
        return;
    }
    lv_obj_add_flag(s_password_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(s_password_textarea, "");
    s_selected_ssid.clear();
}

void refresh_status_texts()
{
    SettingsService &service = SettingsService::instance();

    if (s_wifi_status_label != nullptr) {
        if (service.isWifiConnected()) {
            lv_label_set_text_fmt(s_wifi_status_label, "Connected to %s", service.getConnectedSsid().c_str());
        } else if (service.isWifiConnecting()) {
            lv_label_set_text(s_wifi_status_label, "Connecting to Wi-Fi...");
        } else {
            lv_label_set_text(s_wifi_status_label, "Wi-Fi not connected");
        }
    }

    if (s_time_status_label != nullptr) {
        if (!service.getAutoTimeSync()) {
            lv_label_set_text(s_time_status_label, "Auto time sync is off");
        } else if (service.isTimeSynced()) {
            lv_label_set_text(s_time_status_label, "Internet time is ready");
        } else {
            lv_label_set_text(s_time_status_label, "Waiting for internet time...");
        }
    }

    if (s_scan_button_label != nullptr) {
        lv_label_set_text(s_scan_button_label, service.isWifiScanning() ? "Scanning..." : "Scan Wi-Fi");
    }
}

void rebuild_wifi_list()
{
    if (s_wifi_list == nullptr) {
        return;
    }

    lv_obj_clean(s_wifi_list);
    s_wifi_buttons.clear();

    SettingsService &service = SettingsService::instance();
    if (service.isWifiScanning()) {
        lv_obj_t *label = lv_label_create(s_wifi_list);
        lv_label_set_text(label, "Looking for nearby networks...");
        lv_obj_set_style_text_color(label, lv_color_hex(0x6D5E82), 0);
        return;
    }

    std::vector<WifiNetwork> networks = service.getWifiScanResults();
    if (networks.empty()) {
        lv_obj_t *label = lv_label_create(s_wifi_list);
        lv_label_set_text(label, "No Wi-Fi found yet.\nTap Scan Wi-Fi.");
        lv_obj_set_style_text_color(label, lv_color_hex(0x6D5E82), 0);
        return;
    }

    for (const auto &network : networks) {
        s_wifi_buttons.push_back({network.ssid, network.is_open, nullptr});
        WifiButtonInfo &entry = s_wifi_buttons.back();

        entry.button = lv_button_create(s_wifi_list);
        lv_obj_set_width(entry.button, lv_pct(100));
        lv_obj_set_height(entry.button, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(entry.button, 20, 0);
        lv_obj_set_style_pad_all(entry.button, 14, 0);
        lv_obj_set_style_bg_color(entry.button, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_grad_color(entry.button, lv_color_hex(0xFFEFF8), 0);
        lv_obj_set_style_bg_grad_dir(entry.button, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_border_width(entry.button, 1, 0);
        lv_obj_set_style_border_color(entry.button, lv_color_hex(0xF4BCD6), 0);
        lv_obj_add_event_cb(entry.button, [](lv_event_t *event) {
            auto *info = static_cast<WifiButtonInfo *>(lv_event_get_user_data(event));
            if (info == nullptr) {
                return;
            }

            if (info->is_open) {
                SettingsService::instance().connectWifi(info->ssid, "");
                refresh_status_texts();
                return;
            }

            s_selected_ssid = info->ssid;
            lv_label_set_text_fmt(s_password_title, "Password for %s", s_selected_ssid.c_str());
            lv_obj_clear_flag(s_password_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
        }, LV_EVENT_CLICKED, &entry);

        lv_obj_t *label = lv_label_create(entry.button);
        lv_label_set_text_fmt(
            label, "%s  %s  (%ddBm)", network.ssid.c_str(), network.is_open ? "Open" : "Secure", network.rssi
        );
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x5E4C70), 0);
    }
}

void refresh_wifi_ui_if_needed()
{
    std::string signature = build_wifi_signature();
    if (signature != s_wifi_signature) {
        s_wifi_signature = signature;
        rebuild_wifi_list();
    }
    refresh_status_texts();
}

void create_password_dialog(lv_obj_t *parent)
{
    s_password_overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(s_password_overlay);
    lv_obj_set_size(s_password_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_password_overlay, lv_color_hex(0x6B5E7A), 0);
    lv_obj_set_style_bg_opa(s_password_overlay, LV_OPA_70, 0);
    lv_obj_add_flag(s_password_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *panel = lv_obj_create(s_password_overlay);
    lv_obj_set_size(panel, lv_pct(88), lv_pct(78));
    lv_obj_center(panel);
    lv_obj_set_style_radius(panel, 28, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFF9FD), 0);
    lv_obj_set_style_pad_all(panel, 18, 0);
    lv_obj_set_style_pad_row(panel, 12, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

    s_password_title = lv_label_create(panel);
    lv_label_set_text(s_password_title, "Password");
    lv_obj_set_style_text_font(s_password_title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(s_password_title, lv_color_hex(0x5B4B7A), 0);

    s_password_textarea = lv_textarea_create(panel);
    lv_obj_set_width(s_password_textarea, lv_pct(100));
    lv_textarea_set_placeholder_text(s_password_textarea, "Type Wi-Fi password");
    lv_textarea_set_password_mode(s_password_textarea, true);
    lv_obj_set_style_radius(s_password_textarea, 18, 0);

    lv_obj_t *button_row = lv_obj_create(panel);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_width(button_row, lv_pct(100));
    lv_obj_set_height(button_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(button_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(button_row, 10, 0);

    lv_obj_t *cancel_btn = lv_button_create(button_row);
    lv_obj_set_flex_grow(cancel_btn, 1);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t *event) {
        (void)event;
        hide_password_dialog();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    lv_obj_t *connect_btn = lv_button_create(button_row);
    lv_obj_set_flex_grow(connect_btn, 1);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0xFF9AC8), 0);
    lv_obj_add_event_cb(connect_btn, [](lv_event_t *event) {
        (void)event;
        SettingsService::instance().connectWifi(s_selected_ssid, lv_textarea_get_text(s_password_textarea));
        hide_password_dialog();
        refresh_status_texts();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_center(connect_label);

    s_keyboard = lv_keyboard_create(panel);
    lv_obj_set_size(s_keyboard, lv_pct(100), 210);
    lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
    lv_obj_add_event_cb(s_keyboard, [](lv_event_t *event) {
        uint32_t code = lv_event_get_code(event);
        if (code == LV_EVENT_CANCEL) {
            hide_password_dialog();
        } else if (code == LV_EVENT_READY) {
            SettingsService::instance().connectWifi(s_selected_ssid, lv_textarea_get_text(s_password_textarea));
            hide_password_dialog();
            refresh_status_texts();
        }
    }, LV_EVENT_ALL, nullptr);
}

void refresh_screen(lv_timer_t *timer)
{
    (void)timer;
    refresh_wifi_ui_if_needed();
}

} // namespace

namespace esp_brookesia::apps {

class SettingsApp: public systems::phone::App {
public:
    static SettingsApp *requestInstance()
    {
        static SettingsApp app;
        return &app;
    }

    SettingsApp(): App(makeCoreConfig(APP_NAME, SETTINGS_ICON_COLOR), makePhoneConfig(1))
    {
    }

    bool run() override
    {
        ESP_UTILS_CHECK_ERROR_RETURN(SettingsService::instance().begin(), false, "Init settings service failed");

        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFF5FB), 0);
        lv_obj_set_style_bg_grad_color(lv_screen_active(), lv_color_hex(0xFFF0C7), 0);
        lv_obj_set_style_bg_grad_dir(lv_screen_active(), LV_GRAD_DIR_VER, 0);

        s_root = lv_obj_create(lv_screen_active());
        lv_obj_remove_style_all(s_root);
        lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
        lv_obj_set_style_pad_all(s_root, 14, 0);
        lv_obj_set_style_pad_row(s_root, 12, 0);
        lv_obj_set_layout(s_root, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(s_root, LV_DIR_VER);
        lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);

        lv_obj_t *hero = create_card(s_root, lv_color_hex(0xFFE0F0));
        lv_obj_t *hero_title = lv_label_create(hero);
        lv_label_set_text(hero_title, "Parent Care");
        lv_obj_set_style_text_font(hero_title, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(hero_title, lv_color_hex(0x5A4770), 0);
        lv_obj_t *hero_subtitle = lv_label_create(hero);
        lv_label_set_text(hero_subtitle, "Home Wi-Fi, clock, brightness, and sound");
        lv_obj_set_style_text_font(hero_subtitle, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(hero_subtitle, lv_color_hex(0x6D5E82), 0);

        lv_obj_t *screen_card = create_card(s_root, lv_color_hex(0xFFF3C9));
        create_section_title(screen_card, "Screen");
        s_brightness_slider = lv_slider_create(screen_card);
        lv_slider_set_range(s_brightness_slider, BSP_LCD_BACKLIGHT_BRIGHTNESS_MIN, BSP_LCD_BACKLIGHT_BRIGHTNESS_MAX);
        lv_slider_set_value(s_brightness_slider, SettingsService::instance().getBrightness(), LV_ANIM_OFF);
        lv_obj_add_event_cb(s_brightness_slider, [](lv_event_t *event) {
            int value = lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(event)));
            SettingsService::instance().setBrightness(value);
            update_value_label(s_brightness_value, value, "%");
        }, LV_EVENT_VALUE_CHANGED, nullptr);
        s_brightness_value = lv_label_create(screen_card);
        update_value_label(s_brightness_value, SettingsService::instance().getBrightness(), "%");

        lv_obj_t *sound_card = create_card(s_root, lv_color_hex(0xD8F6E8));
        create_section_title(sound_card, "Sound");
        s_volume_slider = lv_slider_create(sound_card);
        lv_slider_set_range(s_volume_slider, 0, 100);
        lv_slider_set_value(s_volume_slider, SettingsService::instance().getVolume(), LV_ANIM_OFF);
        lv_obj_add_event_cb(s_volume_slider, [](lv_event_t *event) {
            int value = lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(event)));
            SettingsService::instance().setVolume(value);
            update_value_label(s_volume_value, value, "%");
        }, LV_EVENT_VALUE_CHANGED, nullptr);
        s_volume_value = lv_label_create(sound_card);
        update_value_label(s_volume_value, SettingsService::instance().getVolume(), "%");

        lv_obj_t *time_card = create_card(s_root, lv_color_hex(0xE2E8FF));
        create_section_title(time_card, "Time");
        s_auto_time_switch = lv_switch_create(time_card);
        lv_obj_add_state(
            s_auto_time_switch, SettingsService::instance().getAutoTimeSync() ? LV_STATE_CHECKED : 0
        );
        lv_obj_add_event_cb(s_auto_time_switch, [](lv_event_t *event) {
            bool enabled = lv_obj_has_state(static_cast<lv_obj_t *>(lv_event_get_target(event)), LV_STATE_CHECKED);
            SettingsService::instance().setAutoTimeSync(enabled);
            refresh_status_texts();
        }, LV_EVENT_VALUE_CHANGED, nullptr);
        s_time_status_label = lv_label_create(time_card);
        lv_obj_set_style_text_color(s_time_status_label, lv_color_hex(0x6D5E82), 0);

        lv_obj_t *wifi_card = create_card(s_root, lv_color_hex(0xFFDDE6));
        create_section_title(wifi_card, "Wi-Fi");
        s_wifi_status_label = lv_label_create(wifi_card);
        lv_obj_set_style_text_color(s_wifi_status_label, lv_color_hex(0x6D5E82), 0);

        lv_obj_t *scan_btn = lv_button_create(wifi_card);
        lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0xFF9AC8), 0);
        lv_obj_add_event_cb(scan_btn, [](lv_event_t *event) {
            (void)event;
            SettingsService::instance().startWifiScan();
            refresh_status_texts();
            refresh_wifi_ui_if_needed();
        }, LV_EVENT_CLICKED, nullptr);
        s_scan_button_label = lv_label_create(scan_btn);
        lv_label_set_text(s_scan_button_label, "Scan Wi-Fi");
        lv_obj_center(s_scan_button_label);

        s_wifi_list = lv_obj_create(wifi_card);
        lv_obj_set_width(s_wifi_list, lv_pct(100));
        lv_obj_set_height(s_wifi_list, 220);
        lv_obj_set_style_radius(s_wifi_list, 22, 0);
        lv_obj_set_style_bg_color(s_wifi_list, lv_color_hex(0xFFFDFE), 0);
        lv_obj_set_style_pad_all(s_wifi_list, 10, 0);
        lv_obj_set_layout(s_wifi_list, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(s_wifi_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(s_wifi_list, 8, 0);

        create_password_dialog(lv_screen_active());
        refresh_status_texts();
        refresh_wifi_ui_if_needed();
        s_refresh_timer = lv_timer_create(refresh_screen, 1000, nullptr);
        return true;
    }

    bool back() override
    {
        hide_password_dialog();
        return notifyCoreClosed();
    }

    bool close() override
    {
        if (s_refresh_timer != nullptr) {
            lv_timer_delete(s_refresh_timer);
            s_refresh_timer = nullptr;
        }
        s_root = nullptr;
        s_brightness_slider = nullptr;
        s_brightness_value = nullptr;
        s_volume_slider = nullptr;
        s_volume_value = nullptr;
        s_wifi_status_label = nullptr;
        s_time_status_label = nullptr;
        s_wifi_list = nullptr;
        s_auto_time_switch = nullptr;
        s_scan_button_label = nullptr;
        s_password_overlay = nullptr;
        s_password_title = nullptr;
        s_password_textarea = nullptr;
        s_keyboard = nullptr;
        s_selected_ssid.clear();
        s_wifi_signature.clear();
        s_wifi_buttons.clear();
        return true;
    }
};

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, SettingsApp, APP_NAME, []() {
    return std::shared_ptr<SettingsApp>(SettingsApp::requestInstance(), [](SettingsApp *p) {
        (void)p;
    });
})

} // namespace esp_brookesia::apps
