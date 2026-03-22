/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_vad.h"

#include "esp_gmf_io.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_setup_peripheral.h"
#include "esp_gmf_setup_pool.h"

#include "esp_afe_config.h"
#include "esp_gmf_afe_manager.h"
#include "esp_gmf_afe.h"
#include "cli.h"

#define VOICE2FILE     (true)
#define WAKENET_ENABLE (true)
#define VAD_ENABLE     (true)
#define QUIT_CMD_FOUND (BIT0)

#define BOARD_LYRAT_MINI (0)
#define BOARD_KORVO_2    (1)

#if defined CONFIG_IDF_TARGET_ESP32S3
#define AUDIO_BOARD (BOARD_KORVO_2)
#elif defined CONFIG_IDF_TARGET_ESP32
#define AUDIO_BOARD (BOARD_LYRAT_MINI)
#endif  /* defined CONFIG_IDF_TARGET_ESP32S3 */

#if AUDIO_BOARD == BOARD_KORVO_2
#define AEC_ENABLE          (true)
#define VCMD_ENABLE         (true)

#define ADC_I2S_PORT        (0)
#define ADC_I2S_CH          (2)
#define ADC_I2S_BITS        (32)
#define INPUT_CH_NUM        (4)
#define INPUT_CH_BITS       (16) /* For board `ESP32-S3-Korvo-2`, the es7210 is configured as 32-bit,
                                   2-channel mode to accommodate 16-bit, 4-channel data */
#define INPUT_CH_ALLOCATION ("RMNM")
#elif AUDIO_BOARD == BOARD_LYRAT_MINI
#define AEC_ENABLE          (false)
#define VCMD_ENABLE         (false)

#define ADC_I2S_PORT        (1)
#define ADC_I2S_CH          (2)
#define ADC_I2S_BITS        (16)
#define INPUT_CH_NUM        (ADC_I2S_CH)
#define INPUT_CH_BITS       (ADC_I2S_BITS)
#define INPUT_CH_ALLOCATION ("RM")
#endif  /* AUDIO_BOARD == BOARD_KORVO_2 */

static const char *TAG = "AI_AUDIO_WWE";

static bool               speeching     = false;
static bool               wakeup        = false;
static EventGroupHandle_t g_event_group = NULL;

static esp_err_t _pipeline_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGI(TAG, "CB: RECV Pipeline EVT: el:%s-%p, type:%d, sub:%s, payload:%p, size:%d,%p",
             OBJ_GET_TAG(event->from), event->from, event->type, esp_gmf_event_get_state_str(event->sub),
             event->payload, event->payload_size, ctx);
    return 0;
}

void esp_gmf_afe_event_cb(esp_gmf_obj_handle_t obj, esp_gmf_afe_evt_t *event, void *user_data)
{
    switch (event->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START: {
            wakeup = true;
#if WAKENET_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* WAKENET_ENABLE == true */
            esp_gmf_afe_wakeup_info_t *info = event->event_data;
            ESP_LOGI(TAG, "WAKEUP_START [%d : %d]", info->wake_word_index, info->wakenet_model_index);
            break;
        }
        case ESP_GMF_AFE_EVT_WAKEUP_END: {
            wakeup = false;
#if WAKENET_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* WAKENET_ENABLE == true */
            ESP_LOGI(TAG, "WAKEUP_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_START: {
#if WAKENET_ENABLE != true
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* WAKENET_ENABLE != true */
            speeching = true;
            ESP_LOGI(TAG, "VAD_START");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_END: {
#if WAKENET_ENABLE != true
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* WAKENET_ENABLE != true */
            speeching = false;
            ESP_LOGI(TAG, "VAD_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT: {
            ESP_LOGI(TAG, "VCMD_DECT_TIMEOUT");
            break;
        }
        default: {
            esp_gmf_afe_vcmd_info_t *info = event->event_data;
            ESP_LOGW(TAG, "Command %d, phrase_id %d, prob %f, str: %s",
                     event->type, info->phrase_id, info->prob, info->str);
            /* Here use the first command to quit this demo
             * For Chinese model, the first default command is `ba xiao shi hou guan ji`
             * For English model, the first default command is `tell me a joke`
             * If user had modified the commands, please refer to the commands setting.
             */
            if (event->type == 1) {
                xEventGroupSetBits(g_event_group, QUIT_CMD_FOUND);
            }
            break;
        }
    }
}

static void voice_2_file(uint8_t *buffer, int len)
{
#if VOICE2FILE == true
#define MAX_FNAME_LEN (50)

    static FILE *fp = NULL;
    static int fcnt = 0;

    if (speeching) {
        if (!fp) {
            char fname[MAX_FNAME_LEN] = {0};
            snprintf(fname, MAX_FNAME_LEN - 1, "/sdcard/16k_16bit_1ch_%d.pcm", fcnt++);
            fp = fopen(fname, "wb");
            if (!fp) {
                ESP_LOGE(TAG, "File open failed");
                return;
            }
        }
        if (len) {
            fwrite(buffer, len, 1, fp);
        }
    } else {
        if (fp) {
            ESP_LOGI(TAG, "File closed");
            fclose(fp);
            fp = NULL;
        }
    }
#endif  /* VOICE2FILE == true */
}

static int outport_acquire_write(void *handle, esp_gmf_payload_t *load, int wanted_size, int block_ticks)
{
    ESP_LOGD(TAG, "Acquire write");
    return wanted_size;
}

static int outport_release_write(void *handle, esp_gmf_payload_t *load, int block_ticks)
{
    ESP_LOGD(TAG, "Release write");
    voice_2_file(load->buf, load->valid_size);
    return load->valid_size;
}

void app_main(void)
{
    int ret = 0;
    esp_log_level_set("*", ESP_LOG_INFO);

    void *card = NULL;
    esp_gmf_setup_periph_sdmmc(&card);
    esp_gmf_setup_periph_i2c(0);
    esp_gmf_setup_periph_aud_info audio_info = {
        .sample_rate = 16000,
        .channel = ADC_I2S_CH,
        .bits_per_sample = ADC_I2S_BITS,
        .port_num = ADC_I2S_PORT,
    };
    void *record_dev = NULL;
    ret = esp_gmf_setup_periph_codec(NULL, &audio_info, NULL, &record_dev);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return;}, "Failed to setup audio codec");
    g_event_group = xEventGroupCreate();

    esp_gmf_pool_handle_t pool = NULL;
    esp_gmf_pool_init(&pool);
    pool_register_io(pool);
    pool_register_audio_codecs(pool);
    pool_register_audio_effects(pool);
    pool_register_codec_dev_io(pool, NULL, record_dev);

    esp_gmf_afe_manager_handle_t afe_manager = NULL;
    srmodel_list_t *models = esp_srmodel_init("model");
    const char *ch_format = INPUT_CH_ALLOCATION;
    afe_config_t *afe_cfg = afe_config_init(ch_format, models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_cfg->vad_init = VAD_ENABLE;
    afe_cfg->vad_mode = VAD_MODE_3;
    afe_cfg->vad_min_speech_ms = 64;
    afe_cfg->vad_min_noise_ms = 1000;
    afe_cfg->wakenet_init = WAKENET_ENABLE;
    afe_cfg->aec_init = AEC_ENABLE;
    esp_gmf_afe_manager_cfg_t afe_manager_cfg = DEFAULT_GMF_AFE_MANAGER_CFG(afe_cfg, NULL, NULL, NULL, NULL);
    ESP_GOTO_ON_ERROR(esp_gmf_afe_manager_create(&afe_manager_cfg, &afe_manager), __quit, TAG, "AFE Manager Create failed");
    esp_gmf_element_handle_t gmf_afe = NULL;
    esp_gmf_afe_cfg_t gmf_afe_cfg = DEFAULT_GMF_AFE_CFG(afe_manager, esp_gmf_afe_event_cb, NULL, models);
    gmf_afe_cfg.vcmd_detect_en = VCMD_ENABLE;
    esp_gmf_afe_init(&gmf_afe_cfg, &gmf_afe);
    esp_gmf_pool_register_element(pool, gmf_afe, NULL);
    esp_gmf_pipeline_handle_t pipe = NULL;
    const char *name[] = {"gmf_afe"};
    esp_gmf_pool_new_pipeline(pool, "codec_dev_rx", name, sizeof(name) / sizeof(char *), NULL, &pipe);
    if (pipe == NULL) {
        ESP_LOGE(TAG, "There is no pipeline");
        goto __quit;
    }
    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(outport_acquire_write,
                                                              outport_release_write,
                                                              NULL,
                                                              NULL,
                                                              2048,
                                                              100);
    esp_gmf_pipeline_reg_el_port(pipe, "gmf_afe", ESP_GMF_IO_DIR_WRITER, outport);

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    cfg.thread.core = 0;
    cfg.thread.prio = 5;
    cfg.thread.stack = 5120;
    esp_gmf_task_handle_t task = NULL;
    esp_gmf_task_init(&cfg, &task);
    esp_gmf_pipeline_bind_task(pipe, task);
    esp_gmf_pipeline_loading_jobs(pipe);
    esp_gmf_pipeline_set_event(pipe, _pipeline_event, NULL);
    esp_gmf_pipeline_run(pipe);

    cli_init("Audio >");

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(g_event_group, QUIT_CMD_FOUND, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & QUIT_CMD_FOUND) {
            ESP_LOGI(TAG, "Quit command found, stopping pipeline");
            break;
        }
    }

__quit:
    esp_gmf_pipeline_stop(pipe);
    esp_gmf_task_deinit(task);
    esp_gmf_pipeline_destroy(pipe);
    afe_config_free(afe_cfg);
    esp_gmf_afe_manager_destroy(afe_manager);
    pool_unregister_audio_codecs();
    esp_gmf_pool_deinit(pool);
    esp_gmf_teardown_periph_codec(NULL, record_dev);
    esp_gmf_teardown_periph_i2c(0);
    esp_gmf_teardown_periph_sdmmc(card);
    vEventGroupDelete(g_event_group);
    ESP_LOGW(TAG, "Wake word engine demo finished");
}
