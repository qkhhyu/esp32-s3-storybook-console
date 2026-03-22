/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "encoder/esp_audio_enc.h"
#include "esp_gmf_audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DEFAULT_ESP_GMF_AUDIO_ENC_CONFIG() {  \
    .type   = ESP_AUDIO_TYPE_UNSUPPORT,       \
    .cfg    = NULL,                           \
    .cfg_sz = 0,                              \
}

/**
 * @brief  Initializes the GMF audio encoder with the provided configuration
 *
 * @param[in]   config  Pointer to the audio encoder configuration
 * @param[out]  handle  Pointer to the audio encoder handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_audio_enc_init(esp_audio_enc_config_t *config, esp_gmf_obj_handle_t *handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */
