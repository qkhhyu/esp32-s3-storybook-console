/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// ALC method
#define ESP_GMF_METHOD_ALC_SET_GAIN          "set_gain"
#define ESP_GMF_METHOD_ALC_SET_GAIN_ARG_IDX  "index"
#define ESP_GMF_METHOD_ALC_SET_GAIN_ARG_GAIN "gain"

#define ESP_GMF_METHOD_ALC_GET_GAIN          "get_gain"
#define ESP_GMF_METHOD_ALC_GET_GAIN_ARG_IDX  "index"
#define ESP_GMF_METHOD_ALC_GET_GAIN_ARG_GAIN "gain"

// BIT CVT method
#define ESP_GMF_METHOD_BIT_CVT_SET_DEST_BITS          "set_dest_bits"
#define ESP_GMF_METHOD_BIT_CVT_SET_DEST_BITS_ARG_BITS "bits"

// CH CVT method
#define ESP_GMF_METHOD_CH_CVT_SET_DEST_CH        "set_dest_ch"
#define ESP_GMF_METHOD_CH_CVT_SET_DEST_CH_ARG_CH "ch"

// RATE CVT method
#define ESP_GMF_METHOD_RATE_CVT_SET_DEST_RATE          "set_dest_rate"
#define ESP_GMF_METHOD_RATE_CVT_SET_DEST_RATE_ARG_RATE "rate"

// EQ method
#define ESP_GMF_METHOD_EQ_SET_PARA               "set_para"
#define ESP_GMF_METHOD_EQ_SET_PARA_ARG_IDX       "index"
#define ESP_GMF_METHOD_EQ_SET_PARA_ARG_PARA      "para"
#define ESP_GMF_METHOD_EQ_SET_PARA_ARG_PARA_FT   "filter_type"
#define ESP_GMF_METHOD_EQ_SET_PARA_ARG_PARA_FC   "fc"
#define ESP_GMF_METHOD_EQ_SET_PARA_ARG_PARA_Q    "q"
#define ESP_GMF_METHOD_EQ_SET_PARA_ARG_PARA_GAIN "gain"

#define ESP_GMF_METHOD_EQ_GET_PARA               "get_para"
#define ESP_GMF_METHOD_EQ_GET_PARA_ARG_IDX       "index"
#define ESP_GMF_METHOD_EQ_GET_PARA_ARG_PARA      "para"
#define ESP_GMF_METHOD_EQ_GET_PARA_ARG_PARA_FT   "filter_type"
#define ESP_GMF_METHOD_EQ_GET_PARA_ARG_PARA_FC   "fc"
#define ESP_GMF_METHOD_EQ_GET_PARA_ARG_PARA_Q    "q"
#define ESP_GMF_METHOD_EQ_GET_PARA_ARG_PARA_GAIN "gain"

#define ESP_GMF_METHOD_EQ_ENABLE_FILTER          "enable_filter"
#define ESP_GMF_METHOD_EQ_ENABLE_FILTER_ARG_IDX  "index"
#define ESP_GMF_METHOD_EQ_ENABLE_FILTER_ARG_PARA "is_enable"

// FADE method
#define ESP_GMF_METHOD_FADE_SET_MODE          "set_mode"
#define ESP_GMF_METHOD_FADE_SET_MODE_ARG_MODE "mode"

#define ESP_GMF_METHOD_FADE_GET_MODE          "get_mode"
#define ESP_GMF_METHOD_FADE_GET_MODE_ARG_MODE "mode"

#define ESP_GMF_METHOD_FADE_RESET_WEIGHT "reset_weight"

// MIXER method
#define ESP_GMF_METHOD_MIXER_SET_MODE          "set_mode"
#define ESP_GMF_METHOD_MIXER_SET_MODE_ARG_IDX  "index"
#define ESP_GMF_METHOD_MIXER_SET_MODE_ARG_MODE "mode"

#define ESP_GMF_METHOD_MIXER_SET_INFO          "set_info"
#define ESP_GMF_METHOD_MIXER_SET_INFO_ARG_RATE "rate"
#define ESP_GMF_METHOD_MIXER_SET_INFO_ARG_CH   "ch"
#define ESP_GMF_METHOD_MIXER_SET_INFO_ARG_BITS "bits"

// SONIC method
#define ESP_GMF_METHOD_SONIC_SET_SPEED           "set_speed"
#define ESP_GMF_METHOD_SONIC_SET_SPEED_ARG_SPEED "speed"

#define ESP_GMF_METHOD_SONIC_GET_SPEED           "get_speed"
#define ESP_GMF_METHOD_SONIC_GET_SPEED_ARG_SPEED "speed"

#define ESP_GMF_METHOD_SONIC_SET_PITCH           "set_pitch"
#define ESP_GMF_METHOD_SONIC_SET_PITCH_ARG_PITCH "pitch"

#define ESP_GMF_METHOD_SONIC_GET_PITCH           "get_pitch"
#define ESP_GMF_METHOD_SONIC_GET_PITCH_ARG_PITCH "pitch"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
