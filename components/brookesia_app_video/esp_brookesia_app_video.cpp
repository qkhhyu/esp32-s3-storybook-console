/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dirent.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_err.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "esp_jpeg_dec.h"
#include "avi_player.h"
#include "esp_brookesia.hpp"
#include "systems/phone/assets/esp_brookesia_phone_assets.h"
#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "AppVideo"
#include "esp_lib_utils.h"

using namespace esp_brookesia::systems;
using namespace esp_brookesia::gui;

namespace {

constexpr const char *APP_NAME = "Story TV";
constexpr uint32_t APP_ICON_COLOR = 0xFFA87D;
constexpr int DISP_WIDTH = 320;
constexpr int DISP_HEIGHT = 200;

static TaskHandle_t s_play_task = nullptr;
static avi_player_handle_t s_avi_handle = nullptr;
static lv_obj_t *s_canvas = nullptr;
static lv_obj_t *s_status_label = nullptr;
static lv_color_t *s_canvas_buf[2] = {nullptr, nullptr};
static int s_current_buf_idx = 0;
static bool s_is_playing = false;
static bool s_sd_mounted = false;
static jpeg_dec_handle_t s_jpeg_handle = nullptr;
static char **s_avi_file_list = nullptr;
static int s_avi_file_count = 0;

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

void free_file_list()
{
    if (s_avi_file_list == nullptr) {
        return;
    }
    for (int i = 0; i < s_avi_file_count; ++i) {
        free(s_avi_file_list[i]);
    }
    free(s_avi_file_list);
    s_avi_file_list = nullptr;
    s_avi_file_count = 0;
}

void set_status_text(const char *text)
{
    if ((s_status_label == nullptr) || !bsp_display_lock(pdMS_TO_TICKS(100))) {
        return;
    }
    lv_label_set_text(s_status_label, text);
    bsp_display_unlock();
}

void set_status_style(lv_color_t color)
{
    if ((s_status_label == nullptr) || !bsp_display_lock(pdMS_TO_TICKS(100))) {
        return;
    }
    lv_obj_set_style_text_color(s_status_label, color, 0);
    bsp_display_unlock();
}

esp_err_t get_avi_file_list(const char *dir_path)
{
    free_file_list();

    DIR *dir = opendir(dir_path);
    if (dir == nullptr) {
        return ESP_FAIL;
    }

    struct dirent *entry = nullptr;
    int count = 0;
    while ((entry = readdir(dir)) != nullptr) {
        char *ext = strrchr(entry->d_name, '.');
        if ((entry->d_type == DT_REG) && (ext != nullptr) && (strcasecmp(ext, ".avi") == 0)) {
            ++count;
        }
    }

    if (count == 0) {
        closedir(dir);
        return ESP_FAIL;
    }

    s_avi_file_list = static_cast<char **>(calloc(static_cast<size_t>(count), sizeof(char *)));
    ESP_UTILS_CHECK_NULL_GOTO(s_avi_file_list, err, "Allocate AVI file list failed");
    s_avi_file_count = count;

    rewinddir(dir);
    count = 0;
    while ((entry = readdir(dir)) != nullptr) {
        char *ext = strrchr(entry->d_name, '.');
        if ((entry->d_type != DT_REG) || (ext == nullptr) || (strcasecmp(ext, ".avi") != 0)) {
            continue;
        }

        size_t full_len = strlen(dir_path) + strlen(entry->d_name) + 2;
        s_avi_file_list[count] = static_cast<char *>(malloc(full_len));
        ESP_UTILS_CHECK_NULL_GOTO(s_avi_file_list[count], err, "Allocate AVI path failed");
        snprintf(s_avi_file_list[count], full_len, "%s/%s", dir_path, entry->d_name);
        ++count;
    }

    closedir(dir);
    return ESP_OK;

err:
    closedir(dir);
    free_file_list();
    return ESP_FAIL;
}

esp_err_t ensure_canvas_buffers()
{
    for (int i = 0; i < 2; ++i) {
        if (s_canvas_buf[i] == nullptr) {
            s_canvas_buf[i] = static_cast<lv_color_t *>(
                jpeg_calloc_align(DISP_WIDTH * DISP_HEIGHT * sizeof(lv_color_t), 16)
            );
            ESP_UTILS_CHECK_NULL_RETURN(s_canvas_buf[i], ESP_ERR_NO_MEM, "Allocate video buffer failed");
        }
    }
    return ESP_OK;
}

esp_err_t ensure_jpeg_decoder()
{
    if (s_jpeg_handle != nullptr) {
        return ESP_OK;
    }

    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    return (jpeg_dec_open(&config, &s_jpeg_handle) == JPEG_ERR_OK) ? ESP_OK : ESP_FAIL;
}

void video_cb(frame_data_t *data, void *arg)
{
    (void)arg;
    if ((data == nullptr) || (data->data == nullptr) || (data->data_bytes == 0)) {
        return;
    }
    if (data->data_bytes > static_cast<size_t>(INT_MAX)) {
        ESP_UTILS_LOGW("Skip oversized JPEG frame (%u bytes)", static_cast<unsigned>(data->data_bytes));
        return;
    }
    if ((ensure_canvas_buffers() != ESP_OK) || (ensure_jpeg_decoder() != ESP_OK)) {
        return;
    }

    int next_buf_idx = (s_current_buf_idx + 1) % 2;
    jpeg_dec_io_t io = {
        .inbuf = data->data,
        .inbuf_len = static_cast<int>(data->data_bytes),
        .outbuf = reinterpret_cast<uint8_t *>(s_canvas_buf[next_buf_idx]),
    };

    jpeg_dec_header_info_t header_info;
    if (jpeg_dec_parse_header(s_jpeg_handle, &io, &header_info) != JPEG_ERR_OK) {
        return;
    }
    if (jpeg_dec_process(s_jpeg_handle, &io) != JPEG_ERR_OK) {
        return;
    }

    if ((s_canvas != nullptr) && bsp_display_lock(pdMS_TO_TICKS(100))) {
        lv_canvas_set_buffer(s_canvas, s_canvas_buf[next_buf_idx], DISP_WIDTH, DISP_HEIGHT, LV_COLOR_FORMAT_RGB565);
        lv_obj_invalidate(s_canvas);
        s_current_buf_idx = next_buf_idx;
        bsp_display_unlock();
    }
}

void audio_cb(frame_data_t *data, void *arg)
{
    (void)arg;
    if ((data == nullptr) || (data->type != FRAME_TYPE_AUDIO) || (data->data == nullptr) || (data->data_bytes == 0)) {
        return;
    }
    size_t bytes_written = 0;
    bsp_extra_i2s_write(data->data, data->data_bytes, &bytes_written, portMAX_DELAY);
}

void audio_set_clock_callback(uint32_t rate, uint32_t bits_cfg, uint32_t ch, void *arg)
{
    (void)arg;
    if (rate == 0) {
        rate = CODEC_DEFAULT_SAMPLE_RATE;
    }
    if (bits_cfg == 0) {
        bits_cfg = CODEC_DEFAULT_BIT_WIDTH;
    }
    bsp_extra_codec_set_fs(rate, bits_cfg, (ch == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO);
}

void avi_end_cb(void *arg)
{
    (void)arg;
    s_is_playing = false;
}

void avi_play_task(void *arg)
{
    (void)arg;

    avi_player_config_t cfg = {
        .buffer_size = 256 * 1024,
        .video_cb = video_cb,
        .audio_cb = audio_cb,
        .audio_set_clock_cb = audio_set_clock_callback,
        .avi_play_end_cb = avi_end_cb,
        .priority = 7,
        .coreID = 0,
        .user_data = nullptr,
        .stack_size = 12 * 1024,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
        .stack_in_psram = false,
#endif
    };
    if (avi_player_init(cfg, &s_avi_handle) != ESP_OK) {
        set_status_text("AVI init failed");
        vTaskDelete(nullptr);
    }

    while (true) {
        for (int i = 0; i < s_avi_file_count; ++i) {
            set_status_text(s_avi_file_list[i]);
            s_is_playing = true;
            if (avi_player_play_from_file(s_avi_handle, s_avi_file_list[i]) != ESP_OK) {
                s_is_playing = false;
                vTaskDelay(pdMS_TO_TICKS(300));
                continue;
            }
            while (s_is_playing) {
                vTaskDelay(pdMS_TO_TICKS(30));
            }
        }
    }
}

void cleanup_video_runtime()
{
    if (s_avi_handle != nullptr) {
        avi_player_play_stop(s_avi_handle);
        avi_player_deinit(s_avi_handle);
        s_avi_handle = nullptr;
    }
    if (s_play_task != nullptr) {
        vTaskDelete(s_play_task);
        s_play_task = nullptr;
    }
    if (s_jpeg_handle != nullptr) {
        jpeg_dec_close(s_jpeg_handle);
        s_jpeg_handle = nullptr;
    }
    for (int i = 0; i < 2; ++i) {
        if (s_canvas_buf[i] != nullptr) {
            jpeg_free_align(s_canvas_buf[i]);
            s_canvas_buf[i] = nullptr;
        }
    }
    s_current_buf_idx = 0;
    s_is_playing = false;
    free_file_list();
    s_canvas = nullptr;
    s_status_label = nullptr;
}

} // namespace

namespace esp_brookesia::apps {

class VideoApp: public systems::phone::App {
public:
    static VideoApp *requestInstance()
    {
        static VideoApp app;
        return &app;
    }

    VideoApp(): App(makeCoreConfig(APP_NAME, APP_ICON_COLOR), makePhoneConfig(0))
    {
    }

    bool run() override
    {
        if (bsp_extra_codec_init() != ESP_OK) {
            ESP_UTILS_LOGE("Init codec failed");
        }
        bsp_extra_codec_volume_set(80, nullptr);

        lv_obj_t *title = lv_label_create(lv_screen_active());
        lv_label_set_text(title, "Story TV");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

        s_status_label = lv_label_create(lv_screen_active());
        lv_label_set_text(s_status_label, "Scanning /sdcard/avi ...");
        lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(s_status_label, 340);
        lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 0);

        s_canvas = lv_canvas_create(lv_screen_active());
        lv_obj_align(s_canvas, LV_ALIGN_CENTER, 0, 36);
        lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);

        if (!s_sd_mounted) {
            esp_err_t mount_ret = bsp_sdcard_mount();
            if (mount_ret != ESP_OK) {
                ESP_UTILS_LOGE("Mount SD card failed [%s]", esp_err_to_name(mount_ret));
                set_status_style(lv_color_hex(0xFF5A5A));
                set_status_text("SD card mount failed.\nInsert/check TF card,\nthen reopen this app.");
                return true;
            }
            s_sd_mounted = true;
        }

        if (get_avi_file_list("/sdcard/avi") != ESP_OK) {
            ESP_UTILS_LOGE("Enumerate AVI files failed");
            set_status_style(lv_color_hex(0xFFB000));
            set_status_text("No AVI files found in\n/sdcard/avi");
            return true;
        }
        if (ensure_canvas_buffers() != ESP_OK) {
            ESP_UTILS_LOGE("Allocate frame buffers failed");
            set_status_style(lv_color_hex(0xFF5A5A));
            set_status_text("Not enough memory to\nstart video playback.");
            return true;
        }

        lv_canvas_set_buffer(s_canvas, s_canvas_buf[0], DISP_WIDTH, DISP_HEIGHT, LV_COLOR_FORMAT_RGB565);
        lv_obj_remove_flag(s_canvas, LV_OBJ_FLAG_HIDDEN);
        set_status_style(lv_color_hex(0xFFFFFF));

        if (s_play_task == nullptr) {
            xTaskCreatePinnedToCore(avi_play_task, "suite_avi", 12288, nullptr, 7, &s_play_task, 0);
        }
        return true;
    }

    bool back() override
    {
        return notifyCoreClosed();
    }

    bool close() override
    {
        cleanup_video_runtime();
        return true;
    }
};

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, VideoApp, APP_NAME, []() {
    return std::shared_ptr<VideoApp>(VideoApp::requestInstance(), [](VideoApp *p) {
        (void)p;
    });
})

} // namespace esp_brookesia::apps
