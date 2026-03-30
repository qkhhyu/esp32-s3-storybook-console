#pragma once

#include <cstdint>
#include "esp_brookesia.hpp"

namespace storybook::ui {

struct PhoneAppChromeOptions {
    bool show_status_bar = true;
    bool show_navigation_bar = false;
    uint8_t launcher_page_index = 0;
};

esp_brookesia::systems::phone::App::Config makePhoneAppConfig(const PhoneAppChromeOptions &options = {});

int estimateStatusBarInset();

void applyScreenVerticalGradient(lv_obj_t *screen, uint32_t color_top, uint32_t color_bottom);

struct StatusBarBackdropStyle {
    uint32_t color_from = 0x8B573F;
    uint32_t color_to = 0xC47A57;
    int extra_height = 10;
    lv_opa_t opacity = LV_OPA_COVER;
};

lv_obj_t *createStatusBarBackdrop(lv_obj_t *parent, const StatusBarBackdropStyle &style = {});

struct ColumnRootStyle {
    int side_padding = 14;
    int top_padding = 14;
    int bottom_padding = 14;
    int row_padding = 12;
    bool reserve_status_bar = true;
    bool transparent_background = true;
    uint32_t background_color = 0x000000;
    lv_opa_t background_opacity = LV_OPA_TRANSP;
};

lv_obj_t *createColumnRoot(lv_obj_t *screen, const ColumnRootStyle &style = {});

} // namespace storybook::ui
