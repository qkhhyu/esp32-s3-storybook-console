/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
extern "C" {
#include "qmi8658.h"
}
#include "bsp/display.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_brookesia.hpp"
#include "esp_log.h"
#include "storybook_ui_helpers.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "AppGravity"
#include "esp_lib_utils.h"

using namespace esp_brookesia::systems;
using namespace esp_brookesia::gui;

namespace {

constexpr const char *TAG = "GravityBlocks";
constexpr const char *APP_NAME = "Gravity";
constexpr uint32_t APP_ICON_COLOR = 0x56C0FF;
constexpr int MAX_BLOCKS = 15;
constexpr int MIN_BLOCK_RADIUS = 14;
constexpr int MAX_BLOCK_RADIUS = 28;
constexpr int TIMER_INTERVAL_MS = 20;
constexpr int CALIBRATION_SAMPLES = 72;
constexpr int HUD_RESERVED_HEIGHT = 126;
constexpr float ACCEL_SCALE_FACTOR = 4.5F;
constexpr float ACCEL_FILTER_ALPHA = 0.22F;
constexpr float CALIBRATION_DEADZONE = 0.05F;
constexpr float CALIBRATION_STABLE_RANGE = 0.18F;
constexpr float SCREEN_WIDTH_MM = 33.09F;
constexpr float SCREEN_HEIGHT_MM = 41.51F;
constexpr float CORNER_RADIUS_MM = 9.2F;

struct Block {
    lv_obj_t *obj = nullptr;
    int radius = 0;
    int x_pos = 0;
    int y_pos = 0;
};

enum class BlockStyle : uint8_t {
    Round = 0,
    SoftSquare,
    SharpSquare,
    Capsule,
};

constexpr base::App::Config make_core_config()
{
    auto config = base::App::Config::SIMPLE_CONSTRUCTOR(APP_NAME, nullptr, true);
    config.launcher_icon = StyleImage::IMAGE_RECOLOR(&esp_brookesia_image_small_app_launcher_default_98_98, APP_ICON_COLOR);
    return config;
}

phone::App::Config make_phone_config()
{
    storybook::ui::PhoneAppChromeOptions options = {};
    options.show_status_bar = false;
    options.show_navigation_bar = false;
    options.launcher_page_index = 0;
    return storybook::ui::makePhoneAppConfig(options);
}

int clamp_int(int value, int min_value, int max_value)
{
    return std::clamp(value, min_value, max_value);
}

lv_color_t random_block_color()
{
    return lv_color_hsv_to_rgb(std::rand() % 360, 60 + (std::rand() % 25), 88 + (std::rand() % 10));
}

bool blocks_overlap(const Block &lhs, const Block &rhs)
{
    int dx = lhs.x_pos - rhs.x_pos;
    int dy = lhs.y_pos - rhs.y_pos;
    int min_distance = lhs.radius + rhs.radius;
    return ((dx * dx) + (dy * dy)) < (min_distance * min_distance);
}

bool blocks_overlap_at(const Block &lhs, const Block &rhs, int lhs_x, int lhs_y)
{
    int dx = lhs_x - rhs.x_pos;
    int dy = lhs_y - rhs.y_pos;
    int min_distance = lhs.radius + rhs.radius;
    return ((dx * dx) + (dy * dy)) < (min_distance * min_distance);
}

void constrain_to_rounded_rect(int &x, int &y, int width, int height, int top_inset, int shape_radius)
{
    float px_per_mm_x = static_cast<float>(width) / SCREEN_WIDTH_MM;
    float px_per_mm_y = static_cast<float>(height) / SCREEN_HEIGHT_MM;
    float px_per_mm = std::min(px_per_mm_x, px_per_mm_y);

    int corner_radius_px = static_cast<int>(CORNER_RADIUS_MM * px_per_mm);
    corner_radius_px = std::max(corner_radius_px, shape_radius + 6);

    int min_y = top_inset + shape_radius;
    int safe_left = corner_radius_px;
    int safe_right = width - corner_radius_px;
    int safe_top = std::max(corner_radius_px, top_inset + corner_radius_px);
    int safe_bottom = height - corner_radius_px;
    int corner_radius_effective = corner_radius_px - shape_radius;

    x = clamp_int(x, shape_radius, width - shape_radius);
    y = clamp_int(y, min_y, height - shape_radius);

    auto pull_to_corner_arc = [&](int center_x, int center_y, int sign_x, int sign_y) {
        float dx = static_cast<float>(sign_x * (center_x - x));
        float dy = static_cast<float>(sign_y * (center_y - y));
        float dist = std::sqrt((dx * dx) + (dy * dy));
        if ((dist > 0.0F) && (dist > corner_radius_effective)) {
            float ratio = static_cast<float>(corner_radius_effective) / dist;
            x = center_x - static_cast<int>((dx * ratio) * sign_x);
            y = center_y - static_cast<int>((dy * ratio) * sign_y);
        }
    };

    if ((x < safe_left) && (y < safe_top)) {
        pull_to_corner_arc(safe_left, safe_top, 1, 1);
    } else if ((x > safe_right) && (y < safe_top)) {
        pull_to_corner_arc(safe_right, safe_top, -1, 1);
    } else if ((x < safe_left) && (y > safe_bottom)) {
        pull_to_corner_arc(safe_left, safe_bottom, 1, -1);
    } else if ((x > safe_right) && (y > safe_bottom)) {
        pull_to_corner_arc(safe_right, safe_bottom, -1, -1);
    }

    y = std::max(y, min_y);
}

lv_obj_t *create_block(lv_obj_t *parent, BlockStyle style, int radius, lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, radius * 2, radius * 2);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 18, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_shadow_offset_y(obj, 6, 0);

    switch (style) {
    case BlockStyle::Round:
        lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
        break;
    case BlockStyle::SoftSquare:
        lv_obj_set_style_radius(obj, 18, 0);
        break;
    case BlockStyle::SharpSquare:
        lv_obj_set_style_radius(obj, 4, 0);
        break;
    case BlockStyle::Capsule:
    default:
        lv_obj_set_size(obj, radius * 2, static_cast<int>(radius * 1.5F));
        lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
        break;
    }

    return obj;
}

lv_obj_t *create_hud_button(lv_obj_t *parent, const char *text, lv_color_t color_from, lv_color_t color_to,
                            lv_event_cb_t event_cb, void *user_data, lv_obj_t **label_output = nullptr)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_style_bg_color(button, color_from, 0);
    lv_obj_set_style_bg_grad_color(button, color_to, 0);
    lv_obj_set_style_bg_grad_dir(button, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_add_event_cb(button, event_cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0x092033), 0);
    lv_obj_center(label);

    if (label_output != nullptr) {
        *label_output = label;
    }
    return button;
}

} // namespace

namespace esp_brookesia::apps {

class GravityBlocksApp final : public systems::phone::App {
public:
    static GravityBlocksApp *requestInstance()
    {
        static GravityBlocksApp instance;
        return &instance;
    }

    GravityBlocksApp(): App(make_core_config(), make_phone_config())
    {
    }

    bool run() override
    {
        if (!_rng_seeded) {
            std::srand(static_cast<unsigned>(std::time(nullptr)));
            _rng_seeded = true;
        }

        lv_obj_t *screen = lv_screen_active();
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x08131F), 0);
        lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x12324B), 0);
        lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

        _display_width = BSP_LCD_H_RES;
        _display_height = BSP_LCD_V_RES;

        _root = lv_obj_create(screen);
        lv_obj_remove_style_all(_root);
        lv_obj_set_size(_root, lv_pct(100), lv_pct(100));
        lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(_root, LV_OPA_TRANSP, 0);

        _scene = lv_obj_create(_root);
        lv_obj_remove_style_all(_scene);
        lv_obj_set_size(_scene, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_opa(_scene, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(_scene, LV_OBJ_FLAG_SCROLLABLE);

        buildHud();
        generateBlocks();

        if (ensureImuReady() == ESP_OK) {
            startCalibration("Hold level for calibration");
        } else {
            setStatusText("QMI8658 init failed");
        }

        refreshInfoText();
        _physics_timer = lv_timer_create(&GravityBlocksApp::onPhysicsTimer, TIMER_INTERVAL_MS, this);
        return true;
    }

    bool back() override
    {
        return notifyCoreClosed();
    }

    bool close() override
    {
        if (_physics_timer != nullptr) {
            lv_timer_delete(_physics_timer);
            _physics_timer = nullptr;
        }

        clearBlocks();
        _root = nullptr;
        _scene = nullptr;
        _hud = nullptr;
        _status_label = nullptr;
        _tilt_label = nullptr;
        _meta_label = nullptr;
        _recalibrate_button = nullptr;
        _pause_button = nullptr;
        _pause_button_label = nullptr;
        _reset_button = nullptr;
        _calibrating = false;
        _recalibration_requested = false;
        _reset_requested = false;
        _paused = false;
        _calibration_sample_count = 0;
        _filtered_accel_x = 0.0F;
        _filtered_accel_y = 0.0F;
        return true;
    }

private:
    void buildHud()
    {
        _hud = lv_obj_create(_root);
        lv_obj_remove_style_all(_hud);
        lv_obj_set_size(_hud, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_align(_hud, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_set_style_pad_hor(_hud, 14, 0);
        lv_obj_set_layout(_hud, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(_hud, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(_hud, 8, 0);
        lv_obj_clear_flag(_hud, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *panel = lv_obj_create(_hud);
        lv_obj_set_width(panel, lv_pct(100));
        lv_obj_set_height(panel, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(panel, 24, 0);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x13314A), 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_70, 0);
        lv_obj_set_style_border_width(panel, 1, 0);
        lv_obj_set_style_border_color(panel, lv_color_hex(0x5EA7DB), 0);
        lv_obj_set_style_shadow_width(panel, 22, 0);
        lv_obj_set_style_shadow_color(panel, lv_color_hex(0x050A12), 0);
        lv_obj_set_style_shadow_opa(panel, LV_OPA_40, 0);
        lv_obj_set_style_pad_all(panel, 14, 0);
        lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_row(panel, 8, 0);
        lv_obj_set_style_pad_column(panel, 10, 0);

        lv_obj_t *title = lv_label_create(panel);
        lv_label_set_text(title, "Gravity Blocks");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xF2FAFF), 0);
        lv_obj_set_flex_grow(title, 1);

        _pause_button = create_hud_button(
            panel, "Pause", lv_color_hex(0xFFD166), lv_color_hex(0xFF9F1C), &GravityBlocksApp::onPauseClicked, this,
            &_pause_button_label
        );
        _reset_button = create_hud_button(
            panel, "Reset", lv_color_hex(0xF78C6B), lv_color_hex(0xFF5D73), &GravityBlocksApp::onResetClicked, this
        );
        _recalibrate_button = create_hud_button(
            panel, "Recalibrate", lv_color_hex(0x5BC0FF), lv_color_hex(0x9BE15D), &GravityBlocksApp::onRecalibrateClicked,
            this
        );

        _status_label = lv_label_create(panel);
        lv_obj_set_width(_status_label, lv_pct(100));
        lv_label_set_long_mode(_status_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0xB7D8EB), 0);

        _tilt_label = lv_label_create(panel);
        lv_obj_set_width(_tilt_label, lv_pct(100));
        lv_obj_set_style_text_font(_tilt_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(_tilt_label, lv_color_hex(0x7EB6D8), 0);

        _meta_label = lv_label_create(panel);
        lv_obj_set_width(_meta_label, lv_pct(100));
        lv_obj_set_style_text_font(_meta_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(_meta_label, lv_color_hex(0x81C3E7), 0);
    }

    void clearBlocks()
    {
        for (auto &block : _blocks) {
            if (block.obj != nullptr) {
                lv_obj_delete(block.obj);
                block.obj = nullptr;
            }
        }
        _blocks.clear();
    }

    void setStatusText(const char *text)
    {
        if (_status_label != nullptr) {
            lv_label_set_text(_status_label, text);
        }
    }

    void refreshInfoText()
    {
        if (_pause_button_label != nullptr) {
            lv_label_set_text(_pause_button_label, _paused ? "Resume" : "Pause");
        }
        if (_meta_label != nullptr) {
            char buffer[72] = {0};
            std::snprintf(
                buffer, sizeof(buffer), "%u blocks | %s", static_cast<unsigned>(_blocks.size()),
                _paused ? "Paused" : (_calibrating ? "Calibrating" : "Live")
            );
            lv_label_set_text(_meta_label, buffer);
        }
        if (_tilt_label != nullptr) {
            if (_calibrating) {
                lv_label_set_text(_tilt_label, "Keep device flat and still");
            } else if (_paused) {
                lv_label_set_text(_tilt_label, "Motion updates are paused");
            }
        }
    }

    esp_err_t ensureImuReady()
    {
        if (_imu_ready) {
            return ESP_OK;
        }

        i2c_master_bus_handle_t bus_handle = bsp_i2c_get_handle();
        if (bus_handle == nullptr) {
            ESP_LOGE(TAG, "Get I2C bus failed");
            return ESP_FAIL;
        }

        esp_err_t ret = qmi8658_init(&_imu, bus_handle, QMI8658_ADDRESS_HIGH);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Init QMI8658 failed: %s", esp_err_to_name(ret));
            return ret;
        }

        ESP_ERROR_CHECK_WITHOUT_ABORT(qmi8658_set_accel_range(&_imu, QMI8658_ACCEL_RANGE_8G));
        ESP_ERROR_CHECK_WITHOUT_ABORT(qmi8658_set_accel_odr(&_imu, QMI8658_ACCEL_ODR_500HZ));
        qmi8658_set_accel_unit_mps2(&_imu, true);
        ESP_ERROR_CHECK_WITHOUT_ABORT(qmi8658_write_register(&_imu, QMI8658_CTRL5, 0x03));

        _imu_ready = true;
        ESP_LOGI(TAG, "QMI8658 ready for gravity blocks app");
        return ESP_OK;
    }

    void startCalibration(const char *reason)
    {
        _calibrating = true;
        _recalibration_requested = false;
        _calibration_sample_count = 0;
        _calibration_sum_x = 0.0F;
        _calibration_sum_y = 0.0F;
        _calibration_max_x = -100.0F;
        _calibration_min_x = 100.0F;
        _calibration_max_y = -100.0F;
        _calibration_min_y = 100.0F;
        _filtered_accel_x = 0.0F;
        _filtered_accel_y = 0.0F;
        setStatusText(reason);
        refreshInfoText();
    }

    void updateCalibration(const qmi8658_data_t &data)
    {
        _calibration_sum_x += data.accelX;
        _calibration_sum_y += data.accelY;
        _calibration_max_x = std::max(_calibration_max_x, data.accelX);
        _calibration_min_x = std::min(_calibration_min_x, data.accelX);
        _calibration_max_y = std::max(_calibration_max_y, data.accelY);
        _calibration_min_y = std::min(_calibration_min_y, data.accelY);
        _calibration_sample_count++;

        if (_calibration_sample_count < CALIBRATION_SAMPLES) {
            char buffer[72] = {0};
            int percent = (_calibration_sample_count * 100) / CALIBRATION_SAMPLES;
            std::snprintf(buffer, sizeof(buffer), "Calibrating level... %d%%", percent);
            setStatusText(buffer);
            refreshInfoText();
            return;
        }

        float range_x = _calibration_max_x - _calibration_min_x;
        float range_y = _calibration_max_y - _calibration_min_y;
        if ((range_x > CALIBRATION_STABLE_RANGE) || (range_y > CALIBRATION_STABLE_RANGE)) {
            startCalibration("Keep device still and try again");
            return;
        }

        _accel_bias_x = _calibration_sum_x / static_cast<float>(CALIBRATION_SAMPLES);
        _accel_bias_y = _calibration_sum_y / static_cast<float>(CALIBRATION_SAMPLES);
        _calibrating = false;
        setStatusText(_paused ? "Calibrated. Press Resume to continue" : "Tilt device to move the blocks");
        refreshInfoText();
        ESP_LOGI(TAG, "Calibration done: bias_x=%.4f bias_y=%.4f", static_cast<double>(_accel_bias_x), static_cast<double>(_accel_bias_y));
    }

    void applyCalibration(qmi8658_data_t &data) const
    {
        data.accelX -= _accel_bias_x;
        data.accelY -= _accel_bias_y;
        if (std::fabs(data.accelX) < CALIBRATION_DEADZONE) {
            data.accelX = 0.0F;
        }
        if (std::fabs(data.accelY) < CALIBRATION_DEADZONE) {
            data.accelY = 0.0F;
        }
    }

    void generateBlocks()
    {
        clearBlocks();
        for (int i = 0; i < MAX_BLOCKS; ++i) {
            Block block;
            block.radius = MIN_BLOCK_RADIUS + (std::rand() % ((MAX_BLOCK_RADIUS - MIN_BLOCK_RADIUS) + 1));
            BlockStyle style = static_cast<BlockStyle>(std::rand() % 4);
            lv_color_t color = random_block_color();
            block.obj = create_block(_scene, style, block.radius, color);
            if (block.obj == nullptr) {
                continue;
            }

            bool valid_position = false;
            for (int attempt = 0; attempt < 140; ++attempt) {
                block.x_pos = block.radius + (std::rand() % std::max(1, _display_width - (block.radius * 2)));
                block.y_pos = HUD_RESERVED_HEIGHT + block.radius +
                              (std::rand() % std::max(1, (_display_height - HUD_RESERVED_HEIGHT) - (block.radius * 2)));

                valid_position = true;
                for (const auto &existing : _blocks) {
                    if (blocks_overlap(block, existing)) {
                        valid_position = false;
                        break;
                    }
                }
                if (valid_position) {
                    break;
                }
            }

            if (!valid_position) {
                lv_obj_delete(block.obj);
                continue;
            }

            lv_obj_set_pos(block.obj, block.x_pos - block.radius, block.y_pos - block.radius);
            _blocks.push_back(block);
        }
        refreshInfoText();
    }

    void resolveCollisions(size_t active_index)
    {
        for (size_t i = 0; i < _blocks.size(); ++i) {
            if (i == active_index) {
                continue;
            }

            if (!blocks_overlap(_blocks[active_index], _blocks[i])) {
                continue;
            }

            int dx = _blocks[active_index].x_pos - _blocks[i].x_pos;
            int dy = _blocks[active_index].y_pos - _blocks[i].y_pos;
            float distance = std::sqrt(static_cast<float>((dx * dx) + (dy * dy)));
            float overlap = static_cast<float>(_blocks[active_index].radius + _blocks[i].radius) - distance;

            if (distance > 0.0F) {
                float ratio = overlap / (2.0F * distance);
                _blocks[active_index].x_pos += static_cast<int>(dx * ratio);
                _blocks[active_index].y_pos += static_cast<int>(dy * ratio);
                _blocks[i].x_pos -= static_cast<int>(dx * ratio);
                _blocks[i].y_pos -= static_cast<int>(dy * ratio);
            } else {
                _blocks[active_index].x_pos += (std::rand() % 7) - 3;
                _blocks[active_index].y_pos += (std::rand() % 7) - 3;
            }

            constrain_to_rounded_rect(_blocks[i].x_pos, _blocks[i].y_pos, _display_width, _display_height, HUD_RESERVED_HEIGHT,
                                      _blocks[i].radius);
            lv_obj_set_pos(_blocks[i].obj, _blocks[i].x_pos - _blocks[i].radius, _blocks[i].y_pos - _blocks[i].radius);
        }
    }

    void updateBlocks(qmi8658_data_t &data)
    {
        applyCalibration(data);
        _filtered_accel_x = (_filtered_accel_x * (1.0F - ACCEL_FILTER_ALPHA)) + (data.accelX * ACCEL_FILTER_ALPHA);
        _filtered_accel_y = (_filtered_accel_y * (1.0F - ACCEL_FILTER_ALPHA)) + (data.accelY * ACCEL_FILTER_ALPHA);

        int move_x = clamp_int(-static_cast<int>(_filtered_accel_y * ACCEL_SCALE_FACTOR), -10, 10);
        int move_y = clamp_int(static_cast<int>(_filtered_accel_x * ACCEL_SCALE_FACTOR), -10, 10);

        char tilt_buffer[72] = {0};
        std::snprintf(tilt_buffer, sizeof(tilt_buffer), "Tilt X %.2f  Y %.2f", static_cast<double>(_filtered_accel_x), static_cast<double>(_filtered_accel_y));
        if (_tilt_label != nullptr) {
            lv_label_set_text(_tilt_label, tilt_buffer);
        }

        for (size_t i = 0; i < _blocks.size(); ++i) {
            int next_x = _blocks[i].x_pos + move_x;
            int next_y = _blocks[i].y_pos + move_y;
            bool collision = false;

            for (size_t j = 0; j < _blocks.size(); ++j) {
                if (i == j) {
                    continue;
                }
                if (blocks_overlap_at(_blocks[i], _blocks[j], next_x, next_y)) {
                    collision = true;
                    break;
                }
            }

            if (!collision) {
                _blocks[i].x_pos = next_x;
                _blocks[i].y_pos = next_y;
            }

            constrain_to_rounded_rect(_blocks[i].x_pos, _blocks[i].y_pos, _display_width, _display_height, HUD_RESERVED_HEIGHT,
                                      _blocks[i].radius);
            resolveCollisions(i);
            constrain_to_rounded_rect(_blocks[i].x_pos, _blocks[i].y_pos, _display_width, _display_height, HUD_RESERVED_HEIGHT,
                                      _blocks[i].radius);
            lv_obj_set_pos(_blocks[i].obj, _blocks[i].x_pos - _blocks[i].radius, _blocks[i].y_pos - _blocks[i].radius);
        }
    }

    static GravityBlocksApp *fromEvent(lv_event_t *event)
    {
        return static_cast<GravityBlocksApp *>(lv_event_get_user_data(event));
    }

    static void onPauseClicked(lv_event_t *event)
    {
        auto *self = fromEvent(event);
        if (self == nullptr) {
            return;
        }

        self->_paused = !self->_paused;
        if (!self->_calibrating) {
            self->setStatusText(self->_paused ? "Physics paused" : "Tilt device to move the blocks");
        }
        self->refreshInfoText();
    }

    static void onResetClicked(lv_event_t *event)
    {
        auto *self = fromEvent(event);
        if (self != nullptr) {
            self->_reset_requested = true;
        }
    }

    static void onRecalibrateClicked(lv_event_t *event)
    {
        auto *self = fromEvent(event);
        if (self != nullptr) {
            self->_recalibration_requested = true;
        }
    }

    static void onPhysicsTimer(lv_timer_t *timer)
    {
        auto *self = static_cast<GravityBlocksApp *>(timer->user_data);
        if ((self == nullptr) || !self->_imu_ready) {
            return;
        }

        if (self->_reset_requested) {
            self->_reset_requested = false;
            self->generateBlocks();
            self->setStatusText(self->_paused ? "Blocks reset while paused" : "Blocks reset");
            self->refreshInfoText();
        }

        if (self->_recalibration_requested && !self->_calibrating) {
            self->startCalibration("Recalibrating...");
        }

        bool ready = false;
        if (qmi8658_is_data_ready(&self->_imu, &ready) != ESP_OK) {
            return;
        }
        if (!ready) {
            return;
        }

        qmi8658_data_t data = {};
        if (qmi8658_read_sensor_data(&self->_imu, &data) != ESP_OK) {
            return;
        }

        if (self->_calibrating) {
            self->updateCalibration(data);
        } else if (!self->_paused) {
            self->updateBlocks(data);
        }
    }

    lv_obj_t *_root = nullptr;
    lv_obj_t *_scene = nullptr;
    lv_obj_t *_hud = nullptr;
    lv_obj_t *_status_label = nullptr;
    lv_obj_t *_tilt_label = nullptr;
    lv_obj_t *_meta_label = nullptr;
    lv_obj_t *_recalibrate_button = nullptr;
    lv_obj_t *_pause_button = nullptr;
    lv_obj_t *_pause_button_label = nullptr;
    lv_obj_t *_reset_button = nullptr;
    lv_timer_t *_physics_timer = nullptr;
    std::vector<Block> _blocks;
    qmi8658_dev_t _imu = {};
    bool _imu_ready = false;
    bool _calibrating = false;
    bool _recalibration_requested = false;
    bool _reset_requested = false;
    bool _paused = false;
    bool _rng_seeded = false;
    int _display_width = 0;
    int _display_height = 0;
    int _calibration_sample_count = 0;
    float _accel_bias_x = 0.0F;
    float _accel_bias_y = 0.0F;
    float _filtered_accel_x = 0.0F;
    float _filtered_accel_y = 0.0F;
    float _calibration_sum_x = 0.0F;
    float _calibration_sum_y = 0.0F;
    float _calibration_max_x = -100.0F;
    float _calibration_min_x = 100.0F;
    float _calibration_max_y = -100.0F;
    float _calibration_min_y = 100.0F;
};

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, GravityBlocksApp, APP_NAME, []() {
    return std::shared_ptr<GravityBlocksApp>(GravityBlocksApp::requestInstance(), [](GravityBlocksApp *app) {
        (void)app;
    });
})

} // namespace esp_brookesia::apps


