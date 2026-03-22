/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_audio_element.h"
#include "simple_dec/esp_audio_simple_dec.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DEFAULT_ESP_GMF_AUDIO_DEC_CONFIG() {    \
    .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_NONE,  \
    .dec_cfg  = NULL,                           \
    .cfg_size = 0,                              \
}

/**
 * @brief  Initializes the GMF audio decoder with the provided configuration
 *
 * @param[in]   config  Pointer to the audio decoder configuration
 * @param[out]  handle  Pointer to the audio decoder handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_audio_dec_init(esp_audio_simple_dec_cfg_t *config, esp_gmf_obj_handle_t *handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */
