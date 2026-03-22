/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <cmath>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "bsp_board_extra.h"
#include "esp_brookesia.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"

using namespace esp_brookesia::systems;
using namespace esp_brookesia::gui;

namespace {

constexpr const char *APP_NAME = "Music Colors";
constexpr uint32_t APP_ICON_COLOR = 0x8ED8A8;
constexpr const char *TAG = "AppSpectrum";
constexpr int N_SAMPLES = 1024;
constexpr int CHANNELS = 2;
constexpr int STRIPE_COUNT = 64;
constexpr int CANVAS_WIDTH = 360;
constexpr int CANVAS_HEIGHT = 220;

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

static TaskHandle_t s_task = nullptr;
static lv_obj_t *s_canvas = nullptr;
static lv_timer_t *s_timer = nullptr;
static float s_display_spectrum[STRIPE_COUNT] = {};
static float s_peak[STRIPE_COUNT] = {};

__attribute__((aligned(16))) static int16_t s_raw_data[N_SAMPLES * CHANNELS];
__attribute__((aligned(16))) static float s_audio_buffer[N_SAMPLES];
__attribute__((aligned(16))) static float s_wind[N_SAMPLES];
__attribute__((aligned(16))) static float s_fft_buffer[N_SAMPLES * 2];
__attribute__((aligned(16))) static float s_spectrum[N_SAMPLES / 2];

void audio_fft_task(void *arg)
{
    (void)arg;

    esp_err_t ret = dsps_fft2r_init_fc32(nullptr, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FFT init failed: %d", ret);
        vTaskDelete(nullptr);
    }

    dsps_wind_hann_f32(s_wind, N_SAMPLES);
    if (bsp_extra_codec_init() != ESP_OK) {
        ESP_LOGE(TAG, "Audio codec init failed");
        vTaskDelete(nullptr);
    }

    size_t bytes_read = 0;
    while (true) {
        ret = bsp_extra_i2s_read(s_raw_data, N_SAMPLES * CHANNELS * sizeof(int16_t), &bytes_read, portMAX_DELAY);
        if ((ret != ESP_OK) || (bytes_read != (N_SAMPLES * CHANNELS * sizeof(int16_t)))) {
            continue;
        }

        for (int i = 0; i < N_SAMPLES; ++i) {
            int16_t left = s_raw_data[i * CHANNELS];
            int16_t right = s_raw_data[i * CHANNELS + 1];
            s_audio_buffer[i] = (left + right) / (2.0f * 32768.0f);
        }

        dsps_mul_f32(s_audio_buffer, s_wind, s_audio_buffer, N_SAMPLES, 1, 1, 1);
        for (int i = 0; i < N_SAMPLES; ++i) {
            s_fft_buffer[2 * i] = s_audio_buffer[i];
            s_fft_buffer[2 * i + 1] = 0.0f;
        }

        dsps_fft2r_fc32(s_fft_buffer, N_SAMPLES);
        dsps_bit_rev_fc32(s_fft_buffer, N_SAMPLES);

        for (int i = 0; i < (N_SAMPLES / 2); ++i) {
            float real = s_fft_buffer[2 * i];
            float imag = s_fft_buffer[2 * i + 1];
            float magnitude = std::sqrt(real * real + imag * imag);
            s_spectrum[i] = 20.0f * std::log10((magnitude / (N_SAMPLES / 2)) + 1e-9f);
        }

        for (int i = 0; i < STRIPE_COUNT; ++i) {
            int fft_idx = i * (N_SAMPLES / 2) / STRIPE_COUNT;
            s_display_spectrum[i] = std::max(-90.0f, std::min(0.0f, s_spectrum[fft_idx]));
        }
    }
}

void timer_cb(lv_timer_t *timer)
{
    lv_obj_t *canvas = static_cast<lv_obj_t *>(lv_timer_get_user_data(timer));
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

    const int stripe_width = CANVAS_WIDTH / STRIPE_COUNT;
    const int center_y = CANVAS_HEIGHT / 2;
    const int bar_gap_px = 2;

    for (int i = 0; i < STRIPE_COUNT; ++i) {
        float db = s_display_spectrum[i];
        float norm = (db + 90.0f) / 90.0f;
        norm = std::max(0.0f, std::min(1.0f, norm));
        norm = std::sqrt(norm);

        int bar_height = static_cast<int>(norm * (CANVAS_HEIGHT / 2));
        if (s_peak[i] < bar_height) {
            s_peak[i] = static_cast<float>(bar_height);
        } else {
            s_peak[i] = std::max(0.0f, s_peak[i] - 2.0f);
        }

        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = lv_color_hsv_to_rgb(static_cast<uint16_t>(i * (270.0f / STRIPE_COUNT)), 100, 100);
        rect_dsc.bg_opa = LV_OPA_COVER;

        int x_start = i * stripe_width + (bar_gap_px / 2);
        int x_end = ((i + 1) * stripe_width) - (bar_gap_px / 2) - 1;
        lv_area_t bar_area = {
            .x1 = x_start,
            .y1 = center_y - bar_height,
            .x2 = x_end,
            .y2 = center_y + bar_height,
        };
        lv_draw_rect(&layer, &rect_dsc, &bar_area);

        int peak_top = center_y - static_cast<int>(s_peak[i]) - 2;
        int peak_bottom = center_y + static_cast<int>(s_peak[i]);
        lv_area_t top_area = {
            .x1 = x_start,
            .y1 = peak_top,
            .x2 = x_end,
            .y2 = peak_top + 2,
        };
        lv_area_t bottom_area = {
            .x1 = x_start,
            .y1 = peak_bottom,
            .x2 = x_end,
            .y2 = peak_bottom + 2,
        };
        lv_draw_rect(&layer, &rect_dsc, &top_area);
        lv_draw_rect(&layer, &rect_dsc, &bottom_area);
    }

    lv_canvas_finish_layer(canvas, &layer);
}

} // namespace

namespace esp_brookesia::apps {

class SpectrumApp: public systems::phone::App {
public:
    static SpectrumApp *requestInstance()
    {
        static SpectrumApp app;
        return &app;
    }

    SpectrumApp(): App(makeCoreConfig(APP_NAME, APP_ICON_COLOR), makePhoneConfig(0))
    {
    }

    bool run() override
    {
        lv_obj_t *title = lv_label_create(lv_screen_active());
        lv_label_set_text(title, "Music Colors");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

        LV_DRAW_BUF_DEFINE_STATIC(draw_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_RGB565);
        LV_DRAW_BUF_INIT_STATIC(draw_buf);

        s_canvas = lv_canvas_create(lv_screen_active());
        lv_obj_set_size(s_canvas, CANVAS_WIDTH, CANVAS_HEIGHT);
        lv_obj_align(s_canvas, LV_ALIGN_CENTER, 0, 24);
        lv_canvas_set_draw_buf(s_canvas, &draw_buf);

        s_timer = lv_timer_create(timer_cb, 33, s_canvas);
        if (s_task == nullptr) {
            xTaskCreate(audio_fft_task, "suite_fft", 6 * 1024, nullptr, 5, &s_task);
        }
        return true;
    }

    bool back() override
    {
        return notifyCoreClosed();
    }

    bool close() override
    {
        if (s_timer != nullptr) {
            lv_timer_delete(s_timer);
            s_timer = nullptr;
        }
        if (s_task != nullptr) {
            vTaskDelete(s_task);
            s_task = nullptr;
        }
        s_canvas = nullptr;
        memset(s_display_spectrum, 0, sizeof(s_display_spectrum));
        memset(s_peak, 0, sizeof(s_peak));
        return true;
    }
};

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, SpectrumApp, APP_NAME, []() {
    return std::shared_ptr<SpectrumApp>(SpectrumApp::requestInstance(), [](SpectrumApp *p) {
        (void)p;
    });
})

} // namespace esp_brookesia::apps
