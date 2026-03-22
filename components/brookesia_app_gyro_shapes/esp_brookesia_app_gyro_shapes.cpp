/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <algorithm>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "qmi8658.h"
#include "esp_brookesia.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "AppGyro"
#include "esp_lib_utils.h"

using namespace esp_brookesia::systems;
using namespace esp_brookesia::gui;

namespace {

constexpr const char *APP_NAME = "Rocket Tilt";
constexpr uint32_t APP_ICON_COLOR = 0x7FB9FF;
constexpr int MAX_SHAPES = 15;
constexpr int MIN_SHAPE_SIZE = 15;
constexpr int MAX_SHAPE_SIZE = 30;
constexpr int ACCEL_SCALE_FACTOR = 5;
constexpr int TASK_DELAY_MS = 20;
constexpr float CALIBRATION_DEADZONE = 0.05f;
constexpr float SCREEN_WIDTH_MM = 33.09f;
constexpr float SCREEN_HEIGHT_MM = 41.51f;
constexpr float CORNER_RADIUS_MM = 9.2f;
constexpr gpio_num_t CALIB_BUTTON_GPIO = GPIO_NUM_0;

enum ShapeType {
    SHAPE_CIRCLE,
    SHAPE_SQUARE,
    SHAPE_TRIANGLE,
    SHAPE_HEXAGON,
    SHAPE_COUNT
};

struct Shape {
    lv_obj_t *obj;
    ShapeType type;
    int radius;
    int x_pos;
    int y_pos;
    lv_color_t color;
};

static TaskHandle_t s_task = nullptr;
static qmi8658_dev_t *s_dev = nullptr;
static i2c_master_bus_handle_t s_i2c_bus = nullptr;
static lv_obj_t *s_hint_label = nullptr;
static lv_obj_t *s_status_label = nullptr;
static Shape s_shapes[MAX_SHAPES] = {};
static int s_shape_count = 0;
static int s_display_width = 0;
static int s_display_height = 0;
static float s_accel_bias_x = 0.0f;
static float s_accel_bias_y = 0.0f;
static bool s_calibration_done = false;
static volatile bool s_recalibration_requested = false;
static int s_button_last_level = 1;

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

lv_color_t get_random_color()
{
    return lv_color_hsv_to_rgb(std::rand() % 360, 70, 90);
}

bool check_overlap(const Shape *a, const Shape *b)
{
    int dx = a->x_pos - b->x_pos;
    int dy = a->y_pos - b->y_pos;
    int distance_squared = dx * dx + dy * dy;
    int min_distance = a->radius + b->radius;
    return distance_squared < (min_distance * min_distance);
}

lv_obj_t *create_shape_obj(ShapeType type, int size, lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(obj, size * 2, size * 2);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 3, 0);
    lv_obj_set_style_bg_color(obj, color, 0);

    switch (type) {
    case SHAPE_CIRCLE:
    case SHAPE_HEXAGON:
        lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
        break;
    case SHAPE_SQUARE:
    case SHAPE_TRIANGLE:
        lv_obj_set_style_radius(obj, 0, 0);
        break;
    default:
        break;
    }

    return obj;
}

void generate_random_shapes()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    s_shape_count = 0;

    while (s_shape_count < MAX_SHAPES) {
        Shape new_shape = {};
        new_shape.type = static_cast<ShapeType>(std::rand() % SHAPE_COUNT);
        new_shape.radius = MIN_SHAPE_SIZE + (std::rand() % (MAX_SHAPE_SIZE - MIN_SHAPE_SIZE + 1));
        new_shape.color = get_random_color();
        new_shape.obj = create_shape_obj(new_shape.type, new_shape.radius, new_shape.color);

        bool valid_position = false;
        int attempts = 0;
        while (!valid_position && attempts < 100) {
            new_shape.x_pos = new_shape.radius + std::rand() % (s_display_width - 2 * new_shape.radius);
            new_shape.y_pos = new_shape.radius + std::rand() % (s_display_height - 2 * new_shape.radius);

            valid_position = true;
            for (int i = 0; i < s_shape_count; ++i) {
                if (check_overlap(&new_shape, &s_shapes[i])) {
                    valid_position = false;
                    break;
                }
            }
            ++attempts;
        }

        if (!valid_position) {
            lv_obj_del(new_shape.obj);
            continue;
        }

        lv_obj_set_pos(new_shape.obj, new_shape.x_pos - new_shape.radius, new_shape.y_pos - new_shape.radius);
        s_shapes[s_shape_count++] = new_shape;
    }
}

void apply_calibration_and_deadzone(qmi8658_data_t *data)
{
    if (!s_calibration_done) {
        return;
    }

    data->accelX -= s_accel_bias_x;
    data->accelY -= s_accel_bias_y;
    if (std::fabs(data->accelX) < CALIBRATION_DEADZONE) {
        data->accelX = 0.0f;
    }
    if (std::fabs(data->accelY) < CALIBRATION_DEADZONE) {
        data->accelY = 0.0f;
    }
}

void update_status_text(const char *text)
{
    if (s_status_label != nullptr) {
        lv_label_set_text(s_status_label, text);
    }
}

void perform_level_calibration()
{
    qmi8658_data_t data = {};
    constexpr int CALIB_SAMPLES = 200;
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float max_x = -10.0f;
    float min_x = 10.0f;
    float max_y = -10.0f;
    float min_y = 10.0f;

    update_status_text("Calibrating...");
    for (int i = 0; i < CALIB_SAMPLES; ++i) {
        if (qmi8658_read_sensor_data(s_dev, &data) == ESP_OK) {
            sum_x += data.accelX;
            sum_y += data.accelY;
            max_x = std::max(max_x, data.accelX);
            min_x = std::min(min_x, data.accelX);
            max_y = std::max(max_y, data.accelY);
            min_y = std::min(min_y, data.accelY);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (((max_x - min_x) > 0.1f) || ((max_y - min_y) > 0.1f)) {
        update_status_text("Calibration unstable, retrying...");
        perform_level_calibration();
        return;
    }

    s_accel_bias_x = sum_x / CALIB_SAMPLES;
    s_accel_bias_y = sum_y / CALIB_SAMPLES;
    s_calibration_done = true;
    update_status_text("Move the board to roll shapes");
}

void constrain_to_rounded_rect(int *x, int *y, int shape_radius)
{
    float px_per_mm_x = static_cast<float>(s_display_width) / SCREEN_WIDTH_MM;
    float px_per_mm_y = static_cast<float>(s_display_height) / SCREEN_HEIGHT_MM;
    float px_per_mm = (px_per_mm_x < px_per_mm_y) ? px_per_mm_x : px_per_mm_y;

    int corner_radius_px = static_cast<int>(CORNER_RADIUS_MM * px_per_mm);
    if (corner_radius_px < shape_radius) {
        corner_radius_px = shape_radius + 5;
    }

    int safe_left = corner_radius_px;
    int safe_right = s_display_width - corner_radius_px;
    int safe_top = corner_radius_px;
    int safe_bottom = s_display_height - corner_radius_px;

    *x = std::max(shape_radius, std::min(*x, s_display_width - shape_radius));
    *y = std::max(shape_radius, std::min(*y, s_display_height - shape_radius));

    int corner_radius_effective = corner_radius_px - shape_radius;
    auto clamp_corner = [corner_radius_effective](int *px, int *py, int origin_x, int origin_y, int sign_x, int sign_y) {
        float dx = static_cast<float>(sign_x * (origin_x - *px));
        float dy = static_cast<float>(sign_y * (origin_y - *py));
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > corner_radius_effective) {
            float ratio = static_cast<float>(corner_radius_effective) / dist;
            *px = origin_x - (sign_x * static_cast<int>(dx * ratio));
            *py = origin_y - (sign_y * static_cast<int>(dy * ratio));
        }
    };

    if ((*x < safe_left) && (*y < safe_top)) {
        clamp_corner(x, y, safe_left, safe_top, 1, 1);
    } else if ((*x > safe_right) && (*y < safe_top)) {
        clamp_corner(x, y, safe_right, safe_top, -1, 1);
    } else if ((*x < safe_left) && (*y > safe_bottom)) {
        clamp_corner(x, y, safe_left, safe_bottom, 1, -1);
    } else if ((*x > safe_right) && (*y > safe_bottom)) {
        clamp_corner(x, y, safe_right, safe_bottom, -1, -1);
    }
}

bool check_collision_impending(const Shape *a, const Shape *b, int a_new_x, int a_new_y)
{
    int dx = a_new_x - b->x_pos;
    int dy = a_new_y - b->y_pos;
    int distance_squared = dx * dx + dy * dy;
    int min_distance = a->radius + b->radius;
    return distance_squared < (min_distance * min_distance);
}

void handle_shape_collisions(int index)
{
    for (int i = 0; i < s_shape_count; ++i) {
        if (i == index) {
            continue;
        }

        if (!check_overlap(&s_shapes[index], &s_shapes[i])) {
            continue;
        }

        int dx = s_shapes[index].x_pos - s_shapes[i].x_pos;
        int dy = s_shapes[index].y_pos - s_shapes[i].y_pos;
        float distance = std::sqrt(static_cast<float>(dx * dx + dy * dy));
        if (distance <= 0.0f) {
            continue;
        }
        float overlap = (s_shapes[index].radius + s_shapes[i].radius) - distance;
        float ratio = overlap / (2.0f * distance);
        s_shapes[index].x_pos += static_cast<int>(dx * ratio);
        s_shapes[index].y_pos += static_cast<int>(dy * ratio);
        s_shapes[i].x_pos -= static_cast<int>(dx * ratio);
        s_shapes[i].y_pos -= static_cast<int>(dy * ratio);
    }
}

void shapes_update_task(void *arg)
{
    (void)arg;
    qmi8658_data_t data = {};

    while (true) {
        int button_level = gpio_get_level(CALIB_BUTTON_GPIO);
        if ((s_button_last_level == 1) && (button_level == 0)) {
            s_recalibration_requested = true;
        }
        s_button_last_level = button_level;

        if (s_recalibration_requested) {
            s_recalibration_requested = false;
            perform_level_calibration();
        }

        bool ready = false;
        if (qmi8658_is_data_ready(s_dev, &ready) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
            continue;
        }

        if (ready && (qmi8658_read_sensor_data(s_dev, &data) == ESP_OK)) {
            apply_calibration_and_deadzone(&data);
            int move_x = -static_cast<int>(data.accelY * ACCEL_SCALE_FACTOR);
            int move_y = static_cast<int>(data.accelX * ACCEL_SCALE_FACTOR);

            bsp_display_lock(pdMS_TO_TICKS(100));
            for (int i = 0; i < s_shape_count; ++i) {
                int new_x = s_shapes[i].x_pos + move_x;
                int new_y = s_shapes[i].y_pos + move_y;

                bool collision = false;
                for (int j = 0; j < s_shape_count; ++j) {
                    if ((i != j) && check_collision_impending(&s_shapes[i], &s_shapes[j], new_x, new_y)) {
                        collision = true;
                        break;
                    }
                }

                if (!collision) {
                    s_shapes[i].x_pos = new_x;
                    s_shapes[i].y_pos = new_y;
                }

                constrain_to_rounded_rect(&s_shapes[i].x_pos, &s_shapes[i].y_pos, s_shapes[i].radius);
                handle_shape_collisions(i);
                lv_obj_set_pos(s_shapes[i].obj, s_shapes[i].x_pos - s_shapes[i].radius,
                               s_shapes[i].y_pos - s_shapes[i].radius);
            }
            bsp_display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));
    }
}

bool ensure_sensor_ready()
{
    if (s_dev != nullptr) {
        return true;
    }

    s_i2c_bus = bsp_i2c_get_handle();
    ESP_UTILS_CHECK_NULL_RETURN(s_i2c_bus, false, "Get board I2C bus failed");

    s_dev = static_cast<qmi8658_dev_t *>(malloc(sizeof(qmi8658_dev_t)));
    ESP_UTILS_CHECK_NULL_RETURN(s_dev, false, "Allocate qmi8658 device failed");
    ESP_UTILS_CHECK_ERROR_RETURN(qmi8658_init(s_dev, s_i2c_bus, QMI8658_ADDRESS_HIGH), false, "Init qmi8658 failed");

    qmi8658_set_accel_range(s_dev, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(s_dev, QMI8658_ACCEL_ODR_500HZ);
    qmi8658_set_accel_unit_mps2(s_dev, true);
    qmi8658_write_register(s_dev, QMI8658_CTRL5, 0x03);
    return true;
}

void ensure_button_ready()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CALIB_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    s_button_last_level = gpio_get_level(CALIB_BUTTON_GPIO);
}

} // namespace

namespace esp_brookesia::apps {

class GyroShapesApp: public systems::phone::App {
public:
    static GyroShapesApp *requestInstance()
    {
        static GyroShapesApp app;
        return &app;
    }

    GyroShapesApp(): App(makeCoreConfig(APP_NAME, APP_ICON_COLOR), makePhoneConfig(0))
    {
    }

    bool run() override
    {
        s_display_width = lv_disp_get_hor_res(nullptr);
        s_display_height = lv_disp_get_ver_res(nullptr);
        s_calibration_done = false;
        s_recalibration_requested = false;

        s_hint_label = lv_label_create(lv_screen_active());
        lv_label_set_text(s_hint_label, "Press BOOT to balance again");
        lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, -12);

        s_status_label = lv_label_create(lv_screen_active());
        lv_label_set_text(s_status_label, "Waking up the rocket...");
        lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 16);

        ESP_UTILS_CHECK_FALSE_RETURN(ensure_sensor_ready(), false, "Prepare qmi8658 failed");
        ensure_button_ready();
        generate_random_shapes();
        perform_level_calibration();

        if (s_task == nullptr) {
            xTaskCreatePinnedToCore(shapes_update_task, "gyro_shapes", 8192, nullptr, 3, &s_task, 1);
        }
        return true;
    }

    bool back() override
    {
        return notifyCoreClosed();
    }

    bool close() override
    {
        if (s_task != nullptr) {
            vTaskDelete(s_task);
            s_task = nullptr;
        }
        s_shape_count = 0;
        memset(s_shapes, 0, sizeof(s_shapes));
        s_hint_label = nullptr;
        s_status_label = nullptr;
        return true;
    }
};

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, GyroShapesApp, APP_NAME, []() {
    return std::shared_ptr<GyroShapesApp>(GyroShapesApp::requestInstance(), [](GyroShapesApp *p) {
        (void)p;
    });
})

} // namespace esp_brookesia::apps
