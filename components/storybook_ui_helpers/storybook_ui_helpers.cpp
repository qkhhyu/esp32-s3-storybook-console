#include "storybook_ui_helpers.hpp"

#include <algorithm>

using namespace esp_brookesia::systems;

namespace {

constexpr int STATUS_BAR_HEIGHT_PERCENT = 10;
constexpr int STATUS_BAR_HEIGHT_MIN = 24;
constexpr int STATUS_BAR_HEIGHT_MAX = 50;

}

namespace storybook::ui {

phone::App::Config makePhoneAppConfig(const PhoneAppChromeOptions &options)
{
    auto config = phone::App::Config::SIMPLE_CONSTRUCTOR(nullptr, options.show_status_bar, options.show_navigation_bar);
    config.app_launcher_page_index = options.launcher_page_index;
    return config;
}

int estimateStatusBarInset()
{
    lv_display_t *display = lv_display_get_default();
    int display_height = (display != nullptr) ? lv_display_get_vertical_resolution(display) : 0;
    int estimated = (display_height * STATUS_BAR_HEIGHT_PERCENT) / 100;
    return std::clamp(estimated, STATUS_BAR_HEIGHT_MIN, STATUS_BAR_HEIGHT_MAX);
}

void applyScreenVerticalGradient(lv_obj_t *screen, uint32_t color_top, uint32_t color_bottom)
{
    if (screen == nullptr) {
        return;
    }

    lv_obj_set_style_bg_color(screen, lv_color_hex(color_top), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(color_bottom), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
}

lv_obj_t *createStatusBarBackdrop(lv_obj_t *parent, const StatusBarBackdropStyle &style)
{
    if (parent == nullptr) {
        return nullptr;
    }

    int height = estimateStatusBarInset() + std::max(style.extra_height, 0);
    lv_obj_t *backdrop = lv_obj_create(parent);
    if (backdrop == nullptr) {
        return nullptr;
    }

    lv_obj_remove_style_all(backdrop);
    lv_obj_set_size(backdrop, lv_pct(100), height);
    lv_obj_align(backdrop, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(backdrop, style.opacity, 0);
    lv_obj_set_style_bg_color(backdrop, lv_color_hex(style.color_from), 0);
    lv_obj_set_style_bg_grad_color(backdrop, lv_color_hex(style.color_to), 0);
    lv_obj_set_style_bg_grad_dir(backdrop, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(backdrop, 0, 0);

    return backdrop;
}

lv_obj_t *createColumnRoot(lv_obj_t *screen, const ColumnRootStyle &style)
{
    if (screen == nullptr) {
        return nullptr;
    }

    int top_offset = style.reserve_status_bar ? estimateStatusBarInset() : 0;
    int screen_width = lv_obj_get_width(screen);
    int screen_height = lv_obj_get_height(screen);
    if ((screen_width <= 0) || (screen_height <= 0)) {
        lv_display_t *display = lv_display_get_default();
        if (display != nullptr) {
            screen_width = lv_display_get_horizontal_resolution(display);
            screen_height = lv_display_get_vertical_resolution(display);
        }
    }
    screen_width = std::max(screen_width, 1);
    screen_height = std::max(screen_height, top_offset + 1);

    lv_obj_t *root = lv_obj_create(screen);
    if (root == nullptr) {
        return nullptr;
    }

    lv_obj_remove_style_all(root);
    lv_obj_set_pos(root, 0, top_offset);
    lv_obj_set_size(root, screen_width, screen_height - top_offset);
    lv_obj_set_style_pad_left(root, style.side_padding, 0);
    lv_obj_set_style_pad_right(root, style.side_padding, 0);
    lv_obj_set_style_pad_bottom(root, style.bottom_padding, 0);
    lv_obj_set_style_pad_top(root, style.top_padding, 0);
    lv_obj_set_style_pad_row(root, style.row_padding, 0);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);
    lv_obj_set_style_bg_color(root, lv_color_hex(style.background_color), 0);
    lv_opa_t background_opa = style.transparent_background ? static_cast<lv_opa_t>(LV_OPA_TRANSP) : style.background_opacity;
    lv_obj_set_style_bg_opa(root, background_opa, 0);

    return root;
}


} // namespace storybook::ui

