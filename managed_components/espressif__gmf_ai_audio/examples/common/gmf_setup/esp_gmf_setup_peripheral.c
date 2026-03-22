/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "driver/sdmmc_host.h"
#include "vfs_fat_internal.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_gmf_oal_mem.h"
#include "driver/i2c_master.h"
#include "driver/i2c.h"
#include "esp_gmf_gpio_config.h"
#include "esp_gmf_setup_peripheral.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
#include "esp_gmf_io_codec_dev.h"
#include "esp_gmf_io_i2s_pdm.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/i2s_pdm.h"
#include "esp_codec_dev_defaults.h"
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */

#define WIFI_CONNECTED_BIT     BIT0
#define WIFI_FAIL_BIT          BIT1
#define WIFI_RECONNECT_RETRIES 30

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
i2s_chan_handle_t            rx_handle   = NULL;
const audio_codec_data_if_t *in_data_if  = NULL;
const audio_codec_ctrl_if_t *in_ctrl_if  = NULL;
const audio_codec_if_t      *in_codec_if = NULL;

i2s_chan_handle_t            tx_handle    = NULL;
const audio_codec_data_if_t *out_data_if  = NULL;
const audio_codec_ctrl_if_t *out_ctrl_if  = NULL;
const audio_codec_if_t      *out_codec_if = NULL;

const audio_codec_gpio_if_t *gpio_if = NULL;
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */

#ifndef CONFIG_EXAMPLE_ESP_GMF_WIFI
#define CONFIG_EXAMPLE_ESP_GMF_WIFI_SSID "****"
#define CONFIG_EXAMPLE_ESP_GMF_WIFI_PASS "****"
#endif  /* CONFIG_EXAMPLE_ESP_GMF_WIFI */

typedef enum {
    I2S_CREATE_MODE_TX_ONLY   = 0,
    I2S_CREATE_MODE_RX_ONLY   = 1,
    I2S_CREATE_MODE_TX_AND_RX = 2,
} i2s_create_mode_t;

static const char        *TAG = "SETUP_PERIPH";
static esp_netif_t       *s_esp_gmf_setup_sta_netif;
static EventGroupHandle_t s_wifi_event_group;
static int                s_retry_num = 0;
i2c_master_bus_handle_t   i2c_handle  = NULL;

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
static esp_err_t setup_periph_i2s_tx_init(esp_gmf_setup_periph_aud_info *aud_info)
{
#if defined CONFIG_IDF_TARGET_ESP32C3
    i2s_pdm_tx_config_t pdm_tx_cfg = {
        .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(aud_info->sample_rate),
        /* The data bit-width of PDM mode is fixed to 16 */
        .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(aud_info->bits_per_sample, aud_info->channel),
        .gpio_cfg = {
            .clk = ESP_GMF_I2S_DAC_BCLK_IO_NUM,
            .dout = ESP_GMF_I2S_DAC_DO_IO_NUM,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    return i2s_channel_init_pdm_tx_mode(tx_handle, &pdm_tx_cfg);
#else
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(aud_info->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(aud_info->bits_per_sample, aud_info->channel),
        .gpio_cfg = {
            .mclk = ESP_GMF_I2S_DAC_MCLK_IO_NUM,
            .bclk = ESP_GMF_I2S_DAC_BCLK_IO_NUM,
            .ws = ESP_GMF_I2S_DAC_WS_IO_NUM,
            .dout = ESP_GMF_I2S_DAC_DO_IO_NUM,
            .din = ESP_GMF_I2S_DAC_DI_IO_NUM,
        },
    };
#if defined CONFIG_IDF_TARGET_ESP32
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
#endif  /* defined CONFIG_IDF_TARGET_ESP32 */
    return i2s_channel_init_std_mode(tx_handle, &std_cfg);
#endif  /* defined CONFIG_IDF_TARGET_ESP32C3 */
}

static esp_err_t setup_periph_i2s_rx_init(esp_gmf_setup_periph_aud_info *aud_info)
{
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(aud_info->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(aud_info->bits_per_sample, aud_info->channel),
        .gpio_cfg = {
            .mclk = ESP_GMF_I2S_ADC_MCLK_IO_NUM,
            .bclk = ESP_GMF_I2S_ADC_BCLK_IO_NUM,
            .ws = ESP_GMF_I2S_ADC_WS_IO_NUM,
            .dout = ESP_GMF_I2S_ADC_DO_IO_NUM,
            .din = ESP_GMF_I2S_ADC_DI_IO_NUM,
        },
    };
    return i2s_channel_init_std_mode(rx_handle, &std_cfg);
}

static esp_err_t setup_periph_create_i2s(i2s_create_mode_t mode, esp_gmf_setup_periph_aud_info *aud_info)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(aud_info->port_num, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t ret = ESP_OK;
    if (mode == I2S_CREATE_MODE_TX_ONLY) {
        ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to new I2S tx handle");
        ret = setup_periph_i2s_tx_init(aud_info);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to initialize I2S tx");
    } else if (mode == I2S_CREATE_MODE_RX_ONLY) {
        ret = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to new I2S rx handle");
        ret = setup_periph_i2s_rx_init(aud_info);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to initialize I2S rx");
    } else {
        ret = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to new I2S tx and rx handle");
        ret = setup_periph_i2s_tx_init(aud_info);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to initialize I2S tx");
        ret = setup_periph_i2s_rx_init(aud_info);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ret;}, "Failed to initialize I2S rx");
    }
    return ret;
}

static const audio_codec_data_if_t *setup_periph_new_i2s_data(void *tx_hd, void *rx_hd)
{
    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = rx_hd,
        .tx_handle = tx_hd,
    };
    return audio_codec_new_i2s_data(&i2s_cfg);
}

static void setup_periph_new_play_codec()
{
#ifdef CONFIG_IDF_TARGET_ESP32C3
    gpio_if = audio_codec_new_gpio();
    gpio_if->setup(ESP_GMF_AMP_IO_NUM, AUDIO_GPIO_DIR_OUT, AUDIO_GPIO_MODE_PULL_DOWN);
    gpio_if->set(ESP_GMF_AMP_IO_NUM, 1);
#else
    audio_codec_i2c_cfg_t i2c_ctrl_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR, .port = 0, .bus_handle = i2c_handle};
    out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);
    gpio_if = audio_codec_new_gpio();
    // New output codec interface
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = out_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = ESP_GMF_AMP_IO_NUM,
        .use_mclk = true,
    };
    out_codec_if = es8311_codec_new(&es8311_cfg);
#endif  /* CONFIG_IDF_TARGET_ESP32C3 */
}

static void setup_periph_new_record_codec()
{
#if defined CONFIG_IDF_TARGET_ESP32P4
    audio_codec_i2c_cfg_t i2c_ctrl_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR, .port = 0, .bus_handle = i2c_handle};
    in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);
    gpio_if = audio_codec_new_gpio();
    // New output codec interface
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .ctrl_if = in_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = ESP_GMF_AMP_IO_NUM,
        .use_mclk = true,
    };
    in_codec_if = es8311_codec_new(&es8311_cfg);
#elif CONFIG_IDF_TARGET_ESP32
    audio_codec_i2c_cfg_t i2c_ctrl_cfg = {.addr = ES7243_CODEC_DEFAULT_ADDR, .port = 0, .bus_handle = i2c_handle};
    in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);
    gpio_if = audio_codec_new_gpio();
    // New output codec interface
    es7243_codec_cfg_t es7243_cfg = {
        .ctrl_if = in_ctrl_if,
    };
    in_codec_if = es7243_codec_new(&es7243_cfg);
#else
    audio_codec_i2c_cfg_t i2c_ctrl_cfg = {.addr = ES7210_CODEC_DEFAULT_ADDR, .port = 0, .bus_handle = i2c_handle};
    in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = in_ctrl_if,
        .mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC2 | ES7120_SEL_MIC3,
    };
    in_codec_if = es7210_codec_new(&es7210_cfg);
#endif  /* defined CONFIG_IDF_TARGET_ESP32P4 */
}

static esp_codec_dev_handle_t setup_periph_create_codec_dev(esp_codec_dev_type_t dev_type, esp_gmf_setup_periph_aud_info *aud_info)
{
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = aud_info->sample_rate,
        .channel = aud_info->channel,
        .bits_per_sample = aud_info->bits_per_sample,
    };
    esp_codec_dev_cfg_t dev_cfg = {0};
    esp_codec_dev_handle_t codec_dev = NULL;
    if (dev_type == ESP_CODEC_DEV_TYPE_OUT) {
        // New output codec device
        dev_cfg.codec_if = out_codec_if;
        dev_cfg.data_if = out_data_if;
        dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
        codec_dev = esp_codec_dev_new(&dev_cfg);
        esp_codec_dev_set_out_vol(codec_dev, 80.0);
        esp_codec_dev_open(codec_dev, &fs);
    } else {
        // New input codec device
        dev_cfg.codec_if = in_codec_if;
        dev_cfg.data_if = in_data_if;
        dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
        codec_dev = esp_codec_dev_new(&dev_cfg);
        esp_codec_dev_set_in_gain(codec_dev, 30.0);
#ifdef CONFIG_IDF_TARGET_ESP32
        if (fs.channel == 1) {
            fs.channel = 2;
            fs.channel_mask = 2;
        }
#endif  /* CONFIG_IDF_TARGET_ESP32 */
        esp_codec_dev_open(codec_dev, &fs);
    }
    return codec_dev;
}

static void setup_periph_play_codec(esp_gmf_setup_periph_aud_info *aud_info, void **play_dev)
{
    out_data_if = setup_periph_new_i2s_data(tx_handle, NULL);
    setup_periph_new_play_codec();
    *play_dev = setup_periph_create_codec_dev(ESP_CODEC_DEV_TYPE_OUT, aud_info);
}

static void setup_periph_record_codec(esp_gmf_setup_periph_aud_info *aud_info, void **record_dev)
{
    in_data_if = setup_periph_new_i2s_data(NULL, rx_handle);
    setup_periph_new_record_codec();
    *record_dev = setup_periph_create_codec_dev(ESP_CODEC_DEV_TYPE_IN, aud_info);
}

void teardown_periph_play_codec(void *play_dev)
{
    esp_codec_dev_close(play_dev);
    esp_codec_dev_delete(play_dev);
    audio_codec_delete_codec_if(out_codec_if);
    audio_codec_delete_ctrl_if(out_ctrl_if);
    audio_codec_delete_gpio_if(gpio_if);
    audio_codec_delete_data_if(out_data_if);
    i2s_channel_disable(tx_handle);
    i2s_del_channel(tx_handle);
    tx_handle = NULL;
}

void teardown_periph_record_codec(void *record_dev)
{
    esp_codec_dev_close(record_dev);
    esp_codec_dev_delete(record_dev);
    audio_codec_delete_codec_if(in_codec_if);
    audio_codec_delete_ctrl_if(in_ctrl_if);
    audio_codec_delete_data_if(in_data_if);
    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
    rx_handle = NULL;
}
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */

void esp_gmf_setup_periph_sdmmc(void **out_card)
{
    sdmmc_card_t *card = NULL;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
#if defined CONFIG_IDF_TARGET_ESP32P4
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
#endif  /* defined CONFIG_IDF_TARGET_ESP32P4 */

#if defined CONFIG_IDF_TARGET_ESP32
    gpio_config_t sdcard_pwr_pin_cfg = {
        .pin_bit_mask = 1UL << GPIO_NUM_13,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sdcard_pwr_pin_cfg);
    gpio_set_level(GPIO_NUM_13, 0);
#endif  /* defined CONFIG_IDF_TARGET_ESP32 */

    slot_config.width = ESP_GMF_SD_WIDTH;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 12 * 1024};
#if SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = ESP_GMF_SD_CLK_IO_NUM;
    slot_config.cmd = ESP_GMF_SD_CMD_IO_NUM;
    slot_config.d0 = ESP_GMF_SD_D0_IO_NUM;
    slot_config.d1 = ESP_GMF_SD_D1_IO_NUM;
    slot_config.d2 = ESP_GMF_SD_D2_IO_NUM;
    slot_config.d3 = ESP_GMF_SD_D3_IO_NUM;
    slot_config.d4 = ESP_GMF_SD_D4_IO_NUM;
    slot_config.d5 = ESP_GMF_SD_D5_IO_NUM;
    slot_config.d6 = ESP_GMF_SD_D6_IO_NUM;
    slot_config.d7 = ESP_GMF_SD_D7_IO_NUM;
    slot_config.cd = ESP_GMF_SD_CD_IO_NUM;
    slot_config.wp = ESP_GMF_SD_WP_IO_NUM;
#endif  /* SOC_SDMMC_USE_GPIO_MATRIX */
#if defined CONFIG_IDF_TARGET_ESP32P4
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
#endif  /* defined CONFIG_IDF_TARGET_ESP32P4 */
    esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    *out_card = card;
}

void esp_gmf_teardown_periph_sdmmc(void *card)
{
    esp_vfs_fat_sdcard_unmount("/sdcard", card);
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_RECONNECT_RETRIES) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void esp_gmf_setup_periph_wifi(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_esp_gmf_setup_sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_EXAMPLE_ESP_GMF_WIFI_SSID,
            .password = CONFIG_EXAMPLE_ESP_GMF_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 CONFIG_EXAMPLE_ESP_GMF_WIFI_SSID, CONFIG_EXAMPLE_ESP_GMF_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_EXAMPLE_ESP_GMF_WIFI_SSID, CONFIG_EXAMPLE_ESP_GMF_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void esp_gmf_teardown_periph_wifi(void)
{
    esp_wifi_disconnect();
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "WIFI stop got an error");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_deinit());
    esp_event_loop_delete_default();
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_esp_gmf_setup_sta_netif));
    esp_netif_destroy(s_esp_gmf_setup_sta_netif);
    s_esp_gmf_setup_sta_netif = NULL;
}

void esp_gmf_setup_periph_i2c(int port)
{
    i2c_master_bus_config_t i2c_config = {
        .i2c_port = 0,
        .sda_io_num = ESP_GMF_I2C_SDA_IO_NUM,
        .scl_io_num = ESP_GMF_I2C_SCL_IO_NUM,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
        .glitch_ignore_cnt = 7,
    };
    i2c_new_master_bus(&i2c_config, &i2c_handle);
}

void esp_gmf_teardown_periph_i2c(int port)
{
    if (i2c_handle != NULL) {
        i2c_del_master_bus(i2c_handle);
        i2c_handle = NULL;
    }
}

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO
esp_gmf_err_t esp_gmf_setup_periph_codec(esp_gmf_setup_periph_aud_info *play_info, esp_gmf_setup_periph_aud_info *rec_info,
                                         void **play_dev, void **record_dev)
{
    if ((play_dev != NULL) && (record_dev != NULL)) {
        if (play_info->port_num == rec_info->port_num) {
            ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_TX_AND_RX, play_info), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S tx and rx");
        } else {
            ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_TX_ONLY, play_info), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S tx");
            ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_RX_ONLY, rec_info), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S rx");
        }
        setup_periph_play_codec(play_info, play_dev);
        setup_periph_record_codec(rec_info, record_dev);
    } else if (play_dev != NULL) {
        ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_TX_ONLY, play_info), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S tx");
        setup_periph_play_codec(play_info, play_dev);
    } else if (record_dev != NULL) {
        ESP_GMF_RET_ON_NOT_OK(TAG, setup_periph_create_i2s(I2S_CREATE_MODE_RX_ONLY, rec_info), { return ESP_GMF_ERR_FAIL;}, "Failed to create I2S rx");
        setup_periph_record_codec(rec_info, record_dev);
    } else {
        return ESP_GMF_ERR_FAIL;
    }
    return ESP_GMF_ERR_OK;
}

void esp_gmf_teardown_periph_codec(void *play_dev, void *record_dev)
{
    if (play_dev != NULL) {
        teardown_periph_play_codec(play_dev);
    }
    if (record_dev != NULL) {
        teardown_periph_record_codec(record_dev);
    }
}
#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */
