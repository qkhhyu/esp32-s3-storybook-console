/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_brookesia.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "AppLVGL"
#include "esp_lib_utils.h"

using namespace esp_brookesia::systems;
using namespace esp_brookesia::gui;

namespace {

constexpr const char *APP_NAME = "Toy Box";
constexpr uint32_t APP_ICON_COLOR = 0xFFD570;

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

void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
    lv_obj_t *label = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
    lv_label_set_text_fmt(label, "Value: %ld", static_cast<long>(lv_slider_get_value(slider)));
}

} // namespace

namespace esp_brookesia::apps {

class LvglShowcaseApp: public systems::phone::App {
public:
    static LvglShowcaseApp *requestInstance()
    {
        static LvglShowcaseApp app;
        return &app;
    }

    LvglShowcaseApp(): App(makeCoreConfig(APP_NAME, APP_ICON_COLOR), makePhoneConfig(0))
    {
    }

    bool run() override
    {
        lv_obj_t *title = lv_label_create(lv_screen_active());
        lv_label_set_text(title, "Toy Box");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

        lv_obj_t *card = lv_obj_create(lv_screen_active());
        lv_obj_set_size(card, 360, 300);
        lv_obj_center(card);
        lv_obj_set_style_radius(card, 24, 0);
        lv_obj_set_style_pad_all(card, 20, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xFFF7E7), 0);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xF7CFA2), 0);

        lv_obj_t *desc = lv_label_create(card);
        lv_label_set_text(desc, "Tap, slide, and explore a playful little widget page.");
        lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(desc, 300);
        lv_obj_align(desc, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_text_color(desc, lv_color_hex(0x755B56), 0);

        lv_obj_t *value_label = lv_label_create(card);
        lv_label_set_text(value_label, "Value: 40");
        lv_obj_align(value_label, LV_ALIGN_TOP_LEFT, 0, 90);
        lv_obj_set_style_text_color(value_label, lv_color_hex(0x755B56), 0);

        lv_obj_t *slider = lv_slider_create(card);
        lv_slider_set_range(slider, 0, 100);
        lv_slider_set_value(slider, 40, LV_ANIM_OFF);
        lv_obj_set_width(slider, 300);
        lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 0, 120);
        lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, value_label);

        lv_obj_t *bar = lv_bar_create(card);
        lv_obj_set_size(bar, 300, 16);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, 72, LV_ANIM_OFF);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 178);

        lv_obj_t *button = lv_button_create(card);
        lv_obj_set_size(button, 180, 54);
        lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(0xFFB7D5), 0);

        lv_obj_t *button_label = lv_label_create(button);
        lv_label_set_text(button_label, "Tap Me");
        lv_obj_center(button_label);

        return true;
    }

    bool back() override
    {
        return notifyCoreClosed();
    }
};

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, LvglShowcaseApp, APP_NAME, []() {
    return std::shared_ptr<LvglShowcaseApp>(LvglShowcaseApp::requestInstance(), [](LvglShowcaseApp *p) {
        (void)p;
    });
})

} // namespace esp_brookesia::apps
