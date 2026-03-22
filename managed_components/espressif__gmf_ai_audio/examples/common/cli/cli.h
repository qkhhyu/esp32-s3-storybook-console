/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CLI_H__
#define __CLI_H__
#include <stdint.h>
#include "esp_err.h"

esp_err_t cli_init(char *prompt);

#endif  /* __CLI_H__ */
