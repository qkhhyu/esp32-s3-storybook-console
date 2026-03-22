/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_gmf_obj.h"
#include "esp_gmf_err.h"
#include "esp_gmf_oal_mem.h"
#include "esp_audio_enc_default.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_gmf_audio_helper.h"

static const char *TAG = "ESP_GMF_AUDIO_HELPER";

esp_gmf_err_t esp_gmf_audio_helper_reconfig_enc_by_type(esp_audio_type_t type, esp_gmf_info_sound_t *info,
                                                        esp_audio_enc_config_t *enc_cfg)
{
    switch (type) {
        case ESP_AUDIO_TYPE_AAC: {
            enc_cfg->type = ESP_AUDIO_TYPE_AAC;
            esp_aac_enc_config_t aac_enc_cfg = ESP_AAC_ENC_CONFIG_DEFAULT();
            aac_enc_cfg.sample_rate = info->sample_rates;
            aac_enc_cfg.channel = info->channels;
            aac_enc_cfg.bits_per_sample = info->bits;
            aac_enc_cfg.bitrate = 64000;
            aac_enc_cfg.adts_used = true;
            if (enc_cfg->cfg && enc_cfg->cfg_sz) {
                esp_gmf_oal_free(enc_cfg->cfg);
                enc_cfg->cfg = NULL;
                enc_cfg->cfg_sz = 0;
            }
            enc_cfg->cfg = esp_gmf_oal_calloc(1, sizeof(esp_aac_enc_config_t));
            ESP_GMF_MEM_CHECK(TAG, enc_cfg->cfg, return ESP_GMF_ERR_MEMORY_LACK;);
            memcpy(enc_cfg->cfg, &aac_enc_cfg, sizeof(esp_aac_enc_config_t));
            enc_cfg->cfg_sz = sizeof(esp_aac_enc_config_t);
            break;
        }
        case ESP_AUDIO_TYPE_AMRNB: {
            enc_cfg->type = ESP_AUDIO_TYPE_AMRNB;
            esp_amrnb_enc_config_t amr_enc_cfg = ESP_AMRNB_ENC_CONFIG_DEFAULT();
            amr_enc_cfg.sample_rate = info->sample_rates;
            amr_enc_cfg.channel = info->channels;
            amr_enc_cfg.bits_per_sample = info->bits;
            amr_enc_cfg.bitrate_mode = ESP_AMRNB_ENC_BITRATE_MR122;
            amr_enc_cfg.no_file_header = false;
            if (enc_cfg->cfg && enc_cfg->cfg_sz) {
                esp_gmf_oal_free(enc_cfg->cfg);
                enc_cfg->cfg = NULL;
                enc_cfg->cfg_sz = 0;
            }
            enc_cfg->cfg = esp_gmf_oal_calloc(1, sizeof(esp_amrnb_enc_config_t));
            ESP_GMF_MEM_CHECK(TAG, enc_cfg->cfg, return ESP_GMF_ERR_MEMORY_LACK;);
            memcpy(enc_cfg->cfg, &amr_enc_cfg, sizeof(esp_amrnb_enc_config_t));
            enc_cfg->cfg_sz = sizeof(esp_amrnb_enc_config_t);
            break;
        }
        case ESP_AUDIO_TYPE_G711A: {
            enc_cfg->type = ESP_AUDIO_TYPE_G711A;
            esp_g711_enc_config_t g711_enc_cfg = ESP_G711_ENC_CONFIG_DEFAULT();
            g711_enc_cfg.sample_rate = info->sample_rates;
            g711_enc_cfg.channel = info->channels;
            g711_enc_cfg.bits_per_sample = info->bits;
            if (enc_cfg->cfg && enc_cfg->cfg_sz) {
                esp_gmf_oal_free(enc_cfg->cfg);
                enc_cfg->cfg = NULL;
                enc_cfg->cfg_sz = 0;
            }
            enc_cfg->cfg = esp_gmf_oal_calloc(1, sizeof(esp_g711_enc_config_t));
            ESP_GMF_MEM_CHECK(TAG, enc_cfg->cfg, return ESP_GMF_ERR_MEMORY_LACK;);
            memcpy(enc_cfg->cfg, &g711_enc_cfg, sizeof(esp_g711_enc_config_t));
            enc_cfg->cfg_sz = sizeof(esp_g711_enc_config_t);
            break;
        }
        case ESP_AUDIO_TYPE_AMRWB: {
            enc_cfg->type = ESP_AUDIO_TYPE_AMRWB;
            esp_amrnb_enc_config_t amr_enc_cfg = ESP_AMRNB_ENC_CONFIG_DEFAULT();
            amr_enc_cfg.sample_rate = info->sample_rates;
            amr_enc_cfg.channel = info->channels;
            amr_enc_cfg.bits_per_sample = info->bits;
            amr_enc_cfg.bitrate_mode = ESP_AMRWB_ENC_BITRATE_MD885;
            amr_enc_cfg.no_file_header = false;
            if (enc_cfg->cfg && enc_cfg->cfg_sz) {
                esp_gmf_oal_free(enc_cfg->cfg);
                enc_cfg->cfg = NULL;
                enc_cfg->cfg_sz = 0;
            }
            enc_cfg->cfg = esp_gmf_oal_calloc(1, sizeof(esp_amrnb_enc_config_t));
            ESP_GMF_MEM_CHECK(TAG, enc_cfg->cfg, return ESP_GMF_ERR_MEMORY_LACK;);
            memcpy(enc_cfg->cfg, &amr_enc_cfg, sizeof(esp_amrnb_enc_config_t));
            enc_cfg->cfg_sz = sizeof(esp_amrnb_enc_config_t);
            break;
        }
        case ESP_AUDIO_TYPE_ALAC: {
            enc_cfg->type = ESP_AUDIO_TYPE_ALAC;
            esp_alac_enc_config_t alac_enc_cfg = ESP_ALAC_ENC_CONFIG_DEFAULT();
            alac_enc_cfg.sample_rate = info->sample_rates;
            alac_enc_cfg.channel = info->channels;
            alac_enc_cfg.bits_per_sample = info->bits;
            if (enc_cfg->cfg && enc_cfg->cfg_sz) {
                esp_gmf_oal_free(enc_cfg->cfg);
                enc_cfg->cfg = NULL;
                enc_cfg->cfg_sz = 0;
            }
            enc_cfg->cfg = esp_gmf_oal_calloc(1, sizeof(esp_alac_enc_config_t));
            ESP_GMF_MEM_CHECK(TAG, enc_cfg->cfg, return ESP_GMF_ERR_MEMORY_LACK;);
            memcpy(enc_cfg->cfg, &alac_enc_cfg, sizeof(esp_alac_enc_config_t));
            enc_cfg->cfg_sz = sizeof(esp_alac_enc_config_t);
            break;
        }
        case ESP_AUDIO_TYPE_PCM: {
            enc_cfg->type = ESP_AUDIO_TYPE_PCM;
            esp_pcm_enc_config_t pcm_enc_cfg = ESP_PCM_ENC_CONFIG_DEFAULT();
            pcm_enc_cfg.sample_rate = info->sample_rates;
            pcm_enc_cfg.channel = info->channels;
            pcm_enc_cfg.bits_per_sample = info->bits;
            if (enc_cfg->cfg && enc_cfg->cfg_sz) {
                esp_gmf_oal_free(enc_cfg->cfg);
                enc_cfg->cfg = NULL;
                enc_cfg->cfg_sz = 0;
            }
            enc_cfg->cfg = esp_gmf_oal_calloc(1, sizeof(esp_pcm_enc_config_t));
            ESP_GMF_MEM_CHECK(TAG, enc_cfg->cfg, return ESP_GMF_ERR_MEMORY_LACK;);
            memcpy(enc_cfg->cfg, &pcm_enc_cfg, sizeof(esp_pcm_enc_config_t));
            enc_cfg->cfg_sz = sizeof(esp_pcm_enc_config_t);
            break;
        }
        case ESP_AUDIO_TYPE_OPUS: {
            enc_cfg->type = ESP_AUDIO_TYPE_OPUS;
            esp_opus_enc_config_t opus_enc_cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
            opus_enc_cfg.sample_rate = info->sample_rates;
            opus_enc_cfg.channel = info->channels;
            opus_enc_cfg.bits_per_sample = info->bits;
            if (enc_cfg->cfg && enc_cfg->cfg_sz) {
                esp_gmf_oal_free(enc_cfg->cfg);
                enc_cfg->cfg = NULL;
                enc_cfg->cfg_sz = 0;
            }
            enc_cfg->cfg = esp_gmf_oal_calloc(1, sizeof(esp_opus_enc_config_t));
            ESP_GMF_MEM_CHECK(TAG, enc_cfg->cfg, return ESP_GMF_ERR_MEMORY_LACK;);
            memcpy(enc_cfg->cfg, &opus_enc_cfg, sizeof(esp_opus_enc_config_t));
            enc_cfg->cfg_sz = sizeof(esp_opus_enc_config_t);
            break;
        }
        case ESP_AUDIO_TYPE_ADPCM: {
            enc_cfg->type = ESP_AUDIO_TYPE_ADPCM;
            esp_adpcm_enc_config_t adpcm_enc_cfg = ESP_ADPCM_ENC_CONFIG_DEFAULT();
            adpcm_enc_cfg.sample_rate = info->sample_rates;
            adpcm_enc_cfg.channel = info->channels;
            adpcm_enc_cfg.bits_per_sample = info->bits;
            if (enc_cfg->cfg && enc_cfg->cfg_sz) {
                esp_gmf_oal_free(enc_cfg->cfg);
                enc_cfg->cfg = NULL;
                enc_cfg->cfg_sz = 0;
            }
            enc_cfg->cfg = esp_gmf_oal_calloc(1, sizeof(esp_adpcm_enc_config_t));
            ESP_GMF_MEM_CHECK(TAG, enc_cfg->cfg, return ESP_GMF_ERR_MEMORY_LACK;);
            memcpy(enc_cfg->cfg, &adpcm_enc_cfg, sizeof(esp_adpcm_enc_config_t));
            enc_cfg->cfg_sz = sizeof(esp_adpcm_enc_config_t);
            break;
        }
        default:
            ESP_LOGE(TAG, "Not support for encoder, %d", type);
            enc_cfg->type = ESP_AUDIO_TYPE_UNSUPPORT;
            return ESP_GMF_ERR_NOT_SUPPORT;
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_audio_helper_get_audio_type_by_uri(const char *uri, esp_audio_type_t *type)
{
    const char *ext = strchr(uri, '.');
    if (ext == NULL) {
        return ESP_GMF_ERR_NOT_SUPPORT;
    }
    ext++;
    if (strncasecmp(ext, "aac", 3) == 0) {
        *type = ESP_AUDIO_TYPE_AAC;
    } else if (strncasecmp(ext, "g711", 4) == 0) {
        *type = ESP_AUDIO_TYPE_G711A;
    } else if (strncasecmp(ext, "amrnb", 5) == 0) {
        *type = ESP_AUDIO_TYPE_AMRNB;
    } else if (strncasecmp(ext, "amrwb", 5) == 0) {
        *type = ESP_AUDIO_TYPE_AMRWB;
    } else if (strncasecmp(ext, "alac", 4) == 0) {
        *type = ESP_AUDIO_TYPE_ALAC;
    } else if (strncasecmp(ext, "pcm", 3) == 0) {
        *type = ESP_AUDIO_TYPE_PCM;
    } else if (strncasecmp(ext, "opus", 4) == 0) {
        *type = ESP_AUDIO_TYPE_OPUS;
    } else if (strncasecmp(ext, "adpcm", 5) == 0) {
        *type = ESP_AUDIO_TYPE_ADPCM;
    } else {
        *type = ESP_AUDIO_TYPE_UNSUPPORT;
        ESP_LOGE(TAG, "Not support for encoder, %s", uri);
        return ESP_GMF_ERR_NOT_SUPPORT;
    }
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_audio_helper_reconfig_dec_by_uri(const char *uri, esp_gmf_info_sound_t *info, esp_audio_simple_dec_cfg_t *dec_cfg)
{
    // free sub cfg first
    if (dec_cfg->dec_cfg) {
        esp_gmf_oal_free(dec_cfg->dec_cfg);
        dec_cfg->dec_cfg = NULL;
        dec_cfg->cfg_size = 0;
    }
    if (strstr(uri, ".aac")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_AAC;
        esp_aac_dec_cfg_t aac_cfg = {
            .no_adts_header = false,
            .aac_plus_enable = true,
        };
        dec_cfg->dec_cfg = esp_gmf_oal_calloc(1, sizeof(esp_aac_dec_cfg_t));
        ESP_GMF_MEM_CHECK(TAG, dec_cfg->dec_cfg, return ESP_GMF_ERR_MEMORY_LACK;);
        dec_cfg->cfg_size = sizeof(esp_aac_dec_cfg_t);
        memcpy(dec_cfg->dec_cfg, &aac_cfg, dec_cfg->cfg_size);
    } else if (strstr(uri, ".mp3")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
    } else if (strstr(uri, ".amrwb")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_AMRWB;
    } else if (strstr(uri, ".amr")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_AMRNB;
    } else if (strstr(uri, ".flac")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC;
    } else if (strstr(uri, ".wav")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_WAV;
    } else if (strstr(uri, ".m4a")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_M4A;
    } else if (strstr(uri, ".ts")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_TS;
    } else if (strstr(uri, ".opus")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_RAW_OPUS;
        esp_opus_dec_cfg_t opus_cfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
        opus_cfg.channel = info->channels;
        opus_cfg.sample_rate = info->sample_rates;
        dec_cfg->dec_cfg = esp_gmf_oal_calloc(1, sizeof(esp_opus_dec_cfg_t));
        ESP_GMF_MEM_CHECK(TAG, dec_cfg->dec_cfg, return ESP_GMF_ERR_MEMORY_LACK;);
        dec_cfg->cfg_size = sizeof(esp_opus_dec_cfg_t);
        memcpy(dec_cfg->dec_cfg, &opus_cfg, dec_cfg->cfg_size);
    } else if (strstr(uri, ".pcm")) {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_PCM;
        esp_pcm_dec_cfg_t *cfg = dec_cfg->dec_cfg;
        cfg = esp_gmf_oal_calloc(1, sizeof(esp_pcm_dec_cfg_t));
        ESP_GMF_MEM_CHECK(TAG, cfg, return ESP_GMF_ERR_MEMORY_LACK;);
        dec_cfg->cfg_size = sizeof(esp_pcm_dec_cfg_t);
        dec_cfg->dec_cfg = cfg;
        cfg->sample_rate = info->sample_rates;
        cfg->channel = info->channels;
        cfg->bits_per_sample = info->bits;
    } else {
        dec_cfg->dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_NONE;
        ESP_LOGW(TAG, "Not support for simple decoder, %s", uri);
        return ESP_GMF_ERR_NOT_SUPPORT;
    }
    ESP_LOGD(TAG, "The new dec type is %d", dec_cfg->dec_type);
    return ESP_GMF_ERR_OK;
}
