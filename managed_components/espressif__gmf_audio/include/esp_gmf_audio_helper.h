/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_audio_enc.h"
#include "esp_audio_simple_dec.h"
#include "esp_gmf_info.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief  Reconfigurate information of audio encoder by type and input sound information
 *
 * @param[in]   type     Type of audio encoder
 * @param[in]   info     Information of audio encoder
 * @param[out]  enc_cfg  Configuration of audio encoder
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_NOT_SUPPORT  Not supported encoder type
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_audio_helper_reconfig_enc_by_type(esp_audio_type_t type, esp_gmf_info_sound_t *info,
                                                        esp_audio_enc_config_t *enc_cfg);

/**
 * @brief  Get type of audio encoder by uri
 *
 * @param[in]   uri   URI of audio encoder
 * @param[out]  type  Type of audio encoder
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_NOT_SUPPORT  Not supported encoder type
 */
esp_gmf_err_t esp_gmf_audio_helper_get_audio_type_by_uri(const char *uri, esp_audio_type_t *type);

/**
 * @brief  Reconfigurate type of audio decoder by URI
 *
 * @param[in]   uri      URI of audio decoder
 * @param[in]   info     Information of audio decoder
 * @param[out]  dec_cfg  Configuration of audio decoder
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_NOT_SUPPORT  Not supported encoder type
 *       - ESP_GMF_ERR_MEMORY_LACK  Failed to allocate memory
 */
esp_gmf_err_t esp_gmf_audio_helper_reconfig_dec_by_uri(const char *uri, esp_gmf_info_sound_t *info, esp_audio_simple_dec_cfg_t *dec_cfg);

#ifdef __cplusplus
}
#endif /* __cplusplus */
