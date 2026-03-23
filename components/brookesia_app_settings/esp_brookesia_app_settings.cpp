/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "esp_brookesia.hpp"
#include "suite_settings_service.hpp"
#include "suite_ui_font_service.hpp"
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
using UiLanguage = suite::UiLanguage;
using WifiNetwork = suite::WifiNetwork;

constexpr const char *APP_NAME = "家庭配置";
constexpr uint32_t SETTINGS_ICON_COLOR = 0xFF9F7A;

static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_hero_title = nullptr;
static lv_obj_t *s_hero_subtitle = nullptr;
static lv_obj_t *s_screen_title = nullptr;
static lv_obj_t *s_brightness_slider = nullptr;
static lv_obj_t *s_brightness_value = nullptr;
static lv_obj_t *s_sound_title = nullptr;
static lv_obj_t *s_volume_slider = nullptr;
static lv_obj_t *s_volume_value = nullptr;
static lv_obj_t *s_wifi_title = nullptr;
static lv_obj_t *s_wifi_status_label = nullptr;
static lv_obj_t *s_time_title = nullptr;
static lv_obj_t *s_time_status_label = nullptr;
static lv_obj_t *s_language_title = nullptr;
static lv_obj_t *s_language_dropdown = nullptr;
static lv_obj_t *s_wifi_list = nullptr;
static lv_obj_t *s_auto_time_switch = nullptr;
static lv_obj_t *s_scan_button_label = nullptr;
static lv_obj_t *s_password_overlay = nullptr;
static lv_obj_t *s_password_title = nullptr;
static lv_obj_t *s_password_textarea = nullptr;
static lv_obj_t *s_keyboard = nullptr;
static lv_obj_t *s_password_cancel_label = nullptr;
static lv_obj_t *s_password_connect_label = nullptr;
static lv_timer_t *s_refresh_timer = nullptr;
static std::string s_selected_ssid;
static std::string s_wifi_signature;

struct WifiButtonInfo {
    std::string ssid;
    bool is_open = false;
    lv_obj_t *button = nullptr;
};

struct PendingWifiConnect {
    std::string ssid;
    std::string password;
};

static std::list<WifiButtonInfo> s_wifi_buttons;

bool is_chinese_language()
{
    return SettingsService::instance().getLanguage() == UiLanguage::Chinese;
}

const char *tr(const char *zh, const char *en)
{
    return is_chinese_language() ? zh : en;
}

bool contains_utf8_multibyte(const char *text)
{
    if (text == nullptr) {
        return false;
    }

    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p != '\0'; ++p) {
        if (*p >= 0x80) {
            return true;
        }
    }

    return false;
}

const lv_font_t *builtin_chinese_font()
{
    return &lv_font_simsun_16_cjk;
}

const lv_font_t *preferred_chinese_font()
{
    return suite::UiFontService::instance().getChineseFont16();
}

const lv_font_t *title_font()
{
    return is_chinese_language() ? preferred_chinese_font() : &lv_font_montserrat_24;
}

const lv_font_t *hero_title_font()
{
    return is_chinese_language() ? preferred_chinese_font() : &lv_font_montserrat_28;
}

const lv_font_t *body_font()
{
    return is_chinese_language() ? preferred_chinese_font() : &lv_font_montserrat_18;
}

const lv_font_t *dialog_title_font()
{
    return is_chinese_language() ? preferred_chinese_font() : &lv_font_montserrat_22;
}

const lv_font_t *wifi_item_font()
{
    return preferred_chinese_font();
}

const char *language_dropdown_options()
{
    return "中文\nEnglish";
}

void apply_text_font(lv_obj_t *obj, const lv_font_t *font)
{
    if ((obj == nullptr) || (font == nullptr)) {
        return;
    }
    lv_obj_set_style_text_font(obj, font, 0);
}

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
    apply_text_font(label, title_font());
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
    signature += std::to_string(static_cast<int>(service.getLanguage()));
    signature += service.getConnectedSsid();
    for (const auto &network : networks) {
        signature += "|";
        signature += network.ssid;
        signature += ":";
        signature += std::to_string(network.rssi);
    }
    return signature;
}

void update_password_title()
{
    if (s_password_title == nullptr) {
        return;
    }

    apply_text_font(s_password_title, dialog_title_font());
    std::string title;
    if (s_selected_ssid.empty()) {
        title = tr("Wi-Fi 密码", "Password");
    } else if (is_chinese_language()) {
        title = s_selected_ssid + " 的密码";
    } else {
        title = "Password for " + s_selected_ssid;
    }
    lv_label_set_text(s_password_title, title.c_str());
}

void apply_localized_texts()
{
    if (s_hero_title != nullptr) {
                lv_label_set_text(s_hero_title, tr("家庭配置", "Parent Care"));
        apply_text_font(s_hero_title, hero_title_font());
    }
    if (s_hero_subtitle != nullptr) {
        lv_label_set_text(s_hero_subtitle, tr("管理 Wi-Fi、时间和声音", "Home Wi-Fi, time, and sound"));
        apply_text_font(s_hero_subtitle, body_font());
    }
    if (s_screen_title != nullptr) {
        lv_label_set_text(s_screen_title, tr("屏幕", "Screen"));
        apply_text_font(s_screen_title, title_font());
    }
    if (s_sound_title != nullptr) {
        lv_label_set_text(s_sound_title, tr("声音", "Sound"));
        apply_text_font(s_sound_title, title_font());
    }
    if (s_time_title != nullptr) {
        lv_label_set_text(s_time_title, tr("时间", "Time"));
        apply_text_font(s_time_title, title_font());
    }
    if (s_language_title != nullptr) {
        lv_label_set_text(s_language_title, tr("语言", "Language"));
        apply_text_font(s_language_title, title_font());
    }
    if (s_wifi_title != nullptr) {
        lv_label_set_text(s_wifi_title, tr("无线网络", "Wi-Fi"));
        apply_text_font(s_wifi_title, title_font());
    }
    if (s_brightness_value != nullptr) {
        apply_text_font(s_brightness_value, body_font());
    }
    if (s_volume_value != nullptr) {
        apply_text_font(s_volume_value, body_font());
    }
    if (s_wifi_status_label != nullptr) {
        apply_text_font(s_wifi_status_label, body_font());
    }
    if (s_time_status_label != nullptr) {
        apply_text_font(s_time_status_label, body_font());
    }
    if (s_password_textarea != nullptr) {
        lv_textarea_set_placeholder_text(s_password_textarea, tr("输入 Wi-Fi 密码", "Type Wi-Fi password"));
        apply_text_font(s_password_textarea, body_font());
    }
    if (s_password_cancel_label != nullptr) {
        lv_label_set_text(s_password_cancel_label, tr("取消", "Cancel"));
        apply_text_font(s_password_cancel_label, body_font());
    }
    if (s_password_connect_label != nullptr) {
        lv_label_set_text(s_password_connect_label, tr("连接", "Connect"));
        apply_text_font(s_password_connect_label, body_font());
    }
    if (s_scan_button_label != nullptr) {
        apply_text_font(s_scan_button_label, body_font());
    }
    if (s_language_dropdown != nullptr) {
        lv_dropdown_set_options(s_language_dropdown, language_dropdown_options());
        apply_text_font(s_language_dropdown, body_font());
        uint16_t desired_index = is_chinese_language() ? 0 : 1;
        if (lv_dropdown_get_selected(s_language_dropdown) != desired_index) {
            lv_dropdown_set_selected(s_language_dropdown, desired_index);
        }
    }
    update_password_title();
}

void hide_password_dialog()
{
    if (s_password_overlay == nullptr) {
        return;
    }
    if (s_keyboard != nullptr) {
        lv_keyboard_set_textarea(s_keyboard, nullptr);
    }
    lv_obj_add_flag(s_password_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_password_textarea != nullptr) {
        lv_textarea_set_text(s_password_textarea, "");
    }
    s_selected_ssid.clear();
}

void submit_wifi_connect_request(std::string ssid, std::string password)
{
    if (ssid.empty()) {
        return;
    }

    auto *request = new (std::nothrow) PendingWifiConnect{std::move(ssid), std::move(password)};
    if (request == nullptr) {
        ESP_UTILS_LOGW("Allocate Wi-Fi connect request failed");
        return;
    }

    BaseType_t ok = xTaskCreateWithCaps(
        [](void *arg) {
            std::unique_ptr<PendingWifiConnect> request(static_cast<PendingWifiConnect *>(arg));
            SettingsService &service = SettingsService::instance();
            esp_err_t ret = service.connectWifi(request->ssid, request->password);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGW("Connect to %s failed immediately [%s]", request->ssid.c_str(), esp_err_to_name(ret));
            }
            vTaskDelete(nullptr);
        },
        "wifi_connect", 3584, request, 4, nullptr, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (ok != pdPASS) {
        delete request;
        ESP_UTILS_LOGW("Create Wi-Fi connect task failed");
    }
}

void refresh_status_texts()
{
    SettingsService &service = SettingsService::instance();
    const esp_err_t last_wifi_error = service.getLastWifiError();

    if (s_wifi_status_label != nullptr) {
        std::string wifi_status;
        if (service.isWifiConnected()) {
            wifi_status = is_chinese_language() ? ("已连接到 " + service.getConnectedSsid())
                                                : ("Connected to " + service.getConnectedSsid());
        } else if (service.isWifiConnecting()) {
            wifi_status = tr("正在连接 Wi-Fi...", "Connecting to Wi-Fi...");
        } else if (service.isWifiScanning()) {
            wifi_status = tr("正在搜索附近的 Wi-Fi...", "Looking for nearby Wi-Fi...");
        } else if (last_wifi_error == ESP_ERR_WIFI_STATE) {
            wifi_status = tr("Wi-Fi 正忙，请再试一次扫描。", "Wi-Fi was busy. Try Scan Wi-Fi again.");
        } else if (last_wifi_error != ESP_OK) {
            wifi_status = std::string(tr("Wi-Fi 错误: ", "Wi-Fi error: ")) + esp_err_to_name(last_wifi_error);
        } else {
            wifi_status = tr("Wi-Fi 未连接", "Wi-Fi not connected");
        }
        lv_label_set_text(s_wifi_status_label, wifi_status.c_str());
    }

    if (s_time_status_label != nullptr) {
        const char *time_status = nullptr;
        if (!service.getAutoTimeSync()) {
            time_status = tr("自动对时已关闭", "Auto time sync is off");
        } else if (service.isTimeSynced()) {
            time_status = tr("网络时间已同步", "Internet time is ready");
        } else {
            time_status = tr("正在等待网络时间...", "Waiting for internet time...");
        }
        lv_label_set_text(s_time_status_label, time_status);
    }

    if (s_scan_button_label != nullptr) {
        lv_label_set_text(s_scan_button_label, service.isWifiScanning() ? tr("扫描中...", "Scanning...")
                                                                        : tr("扫描 Wi-Fi", "Scan Wi-Fi"));
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
        lv_label_set_text(label, tr("正在搜索附近的网络...", "Looking for nearby networks..."));
        lv_obj_set_width(label, lv_pct(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        apply_text_font(label, body_font());
        lv_obj_set_style_text_color(label, lv_color_hex(0x6D5E82), 0);
        return;
    }

    std::vector<WifiNetwork> networks = service.getWifiScanResults();
    if (networks.empty()) {
        lv_obj_t *label = lv_label_create(s_wifi_list);
        lv_label_set_text(label, tr("还没有找到 Wi-Fi。\n请先点击扫描。", "No Wi-Fi found yet.\nTap Scan Wi-Fi."));
        lv_obj_set_width(label, lv_pct(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        apply_text_font(label, body_font());
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
                submit_wifi_connect_request(info->ssid, "");
                refresh_status_texts();
                return;
            }

            s_selected_ssid = info->ssid;
            update_password_title();
            lv_obj_clear_flag(s_password_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
        }, LV_EVENT_CLICKED, &entry);

        lv_obj_t *label = lv_label_create(entry.button);
        std::string text = network.ssid;
        text += "  ";
        text += network.is_open ? tr("开放", "Open") : tr("加密", "Secure");
        text += "  (";
        text += std::to_string(network.rssi);
        text += "dBm)";
        lv_label_set_text(label, text.c_str());
        lv_obj_set_width(label, lv_pct(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        apply_text_font(label, contains_utf8_multibyte(text.c_str()) ? wifi_item_font() : body_font());
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
    lv_obj_set_style_text_color(s_password_title, lv_color_hex(0x5B4B7A), 0);

    s_password_textarea = lv_textarea_create(panel);
    lv_obj_set_width(s_password_textarea, lv_pct(100));
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
    s_password_cancel_label = lv_label_create(cancel_btn);
    lv_obj_center(s_password_cancel_label);

    lv_obj_t *connect_btn = lv_button_create(button_row);
    lv_obj_set_flex_grow(connect_btn, 1);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0xFF9AC8), 0);
    lv_obj_add_event_cb(connect_btn, [](lv_event_t *event) {
        (void)event;
        std::string ssid = s_selected_ssid;
        std::string password = (s_password_textarea != nullptr) ? lv_textarea_get_text(s_password_textarea) : "";
        hide_password_dialog();
        submit_wifi_connect_request(std::move(ssid), std::move(password));
        refresh_status_texts();
    }, LV_EVENT_CLICKED, nullptr);
    s_password_connect_label = lv_label_create(connect_btn);
    lv_obj_center(s_password_connect_label);

    s_keyboard = lv_keyboard_create(panel);
    lv_obj_set_size(s_keyboard, lv_pct(100), 210);
    lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
    lv_obj_add_event_cb(s_keyboard, [](lv_event_t *event) {
        uint32_t code = lv_event_get_code(event);
        if (code == LV_EVENT_CANCEL) {
            hide_password_dialog();
        } else if (code == LV_EVENT_READY) {
            std::string ssid = s_selected_ssid;
            std::string password = (s_password_textarea != nullptr) ? lv_textarea_get_text(s_password_textarea) : "";
            hide_password_dialog();
            submit_wifi_connect_request(std::move(ssid), std::move(password));
            refresh_status_texts();
        }
    }, LV_EVENT_ALL, nullptr);

    apply_localized_texts();
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
        s_hero_title = lv_label_create(hero);
        lv_obj_set_style_text_color(s_hero_title, lv_color_hex(0x5A4770), 0);
        s_hero_subtitle = lv_label_create(hero);
        lv_obj_set_style_text_color(s_hero_subtitle, lv_color_hex(0x6D5E82), 0);

        lv_obj_t *screen_card = create_card(s_root, lv_color_hex(0xFFF3C9));
        s_screen_title = create_section_title(screen_card, "");
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
        s_sound_title = create_section_title(sound_card, "");
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
        s_time_title = create_section_title(time_card, "");
        s_auto_time_switch = lv_switch_create(time_card);
        if (SettingsService::instance().getAutoTimeSync()) {
            lv_obj_add_state(s_auto_time_switch, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(s_auto_time_switch, [](lv_event_t *event) {
            bool enabled = lv_obj_has_state(static_cast<lv_obj_t *>(lv_event_get_target(event)), LV_STATE_CHECKED);
            SettingsService::instance().setAutoTimeSync(enabled);
            refresh_status_texts();
        }, LV_EVENT_VALUE_CHANGED, nullptr);
        s_time_status_label = lv_label_create(time_card);
        lv_obj_set_style_text_color(s_time_status_label, lv_color_hex(0x6D5E82), 0);

        lv_obj_t *language_card = create_card(s_root, lv_color_hex(0xF1E0FF));
        s_language_title = create_section_title(language_card, "");
        s_language_dropdown = lv_dropdown_create(language_card);
        lv_obj_set_width(s_language_dropdown, lv_pct(100));
        lv_dropdown_set_options(s_language_dropdown, language_dropdown_options());
        lv_dropdown_set_selected(s_language_dropdown, is_chinese_language() ? 0 : 1);
        lv_obj_add_event_cb(s_language_dropdown, [](lv_event_t *event) {
            uint16_t selected = lv_dropdown_get_selected(static_cast<lv_obj_t *>(lv_event_get_target(event)));
            UiLanguage language = (selected == 0) ? UiLanguage::Chinese : UiLanguage::English;
            esp_err_t ret = SettingsService::instance().setLanguage(language);
            if (ret != ESP_OK) {
                ESP_UTILS_LOGW("Save language failed [%s]", esp_err_to_name(ret));
            }
            s_wifi_signature.clear();
            apply_localized_texts();
            refresh_wifi_ui_if_needed();
        }, LV_EVENT_VALUE_CHANGED, nullptr);

        lv_obj_t *wifi_card = create_card(s_root, lv_color_hex(0xFFDDE6));
        s_wifi_title = create_section_title(wifi_card, "");
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
        apply_localized_texts();
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
        s_hero_title = nullptr;
        s_hero_subtitle = nullptr;
        s_screen_title = nullptr;
        s_brightness_slider = nullptr;
        s_brightness_value = nullptr;
        s_sound_title = nullptr;
        s_volume_slider = nullptr;
        s_volume_value = nullptr;
        s_wifi_title = nullptr;
        s_wifi_status_label = nullptr;
        s_time_title = nullptr;
        s_time_status_label = nullptr;
        s_language_title = nullptr;
        s_language_dropdown = nullptr;
        s_wifi_list = nullptr;
        s_auto_time_switch = nullptr;
        s_scan_button_label = nullptr;
        s_password_overlay = nullptr;
        s_password_title = nullptr;
        s_password_textarea = nullptr;
        s_keyboard = nullptr;
        s_password_cancel_label = nullptr;
        s_password_connect_label = nullptr;
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
