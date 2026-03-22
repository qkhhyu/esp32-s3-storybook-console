/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/widgets/app_launcher/esp_brookesia_app_launcher.hpp"

namespace esp_brookesia::systems::phone {

constexpr AppLauncherIcon::Data STYLESHEET_410_502_DARK_APP_LAUNCHER_ICON_DATA = {
    .main = {
        .size = gui::StyleSize::SQUARE(188),
        .layout_row_pad = 14,
    },
    .image = {
        .default_size = gui::StyleSize::SQUARE(126),
        .press_size = gui::StyleSize::SQUARE(116),
    },
    .label = {
        .text_font = gui::StyleFont::SIZE(18),
        .text_color = gui::StyleColor::COLOR(0x6B5770),
    },
};

constexpr AppLauncherData STYLESHEET_410_502_DARK_APP_LAUNCHER_DATA = {
    .main = {
        .y_start = 0,
        .size = gui::StyleSize::RECT_PERCENT(100, 100),
    },
    .table = {
        .default_num = 3,
        .size = gui::StyleSize::RECT_PERCENT(100, 76),
    },
    .indicator = {
        .main_size = gui::StyleSize::RECT_W_PERCENT(100, 20),
        .main_layout_column_pad = 10,
        .main_layout_bottom_offset = 20,
        .spot_inactive_size = gui::StyleSize::SQUARE(12),
        .spot_active_size = gui::StyleSize::RECT(44, 14),
        .spot_inactive_background_color = gui::StyleColor::COLOR(0xE9C7D8),
        .spot_active_background_color = gui::StyleColor::COLOR(0xFF9BC3),
    },
    .icon = STYLESHEET_410_502_DARK_APP_LAUNCHER_ICON_DATA,
    .flags = {
        .enable_table_scroll_anim = 0,
    },
};

} // namespace esp_brookesia::systems::phone
