/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "suite_ui_font_service.hpp"

#include <cstdint>
#include <cstring>
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_log.h"
#include "esp_partition.h"

namespace {

constexpr const char *TAG = "UiFontService";
constexpr const char *FONT_PARTITION_LABEL = "storage";
constexpr uint32_t FONT_PARTITION_MAGIC = 0x54465553; // 'SUFT'
constexpr size_t FONT_PARTITION_HEADER_SIZE = 4096;
constexpr int CHINESE_FONT_SIZE = 14;
constexpr size_t CHINESE_FONT_CACHE_SIZE = 16;

#pragma pack(push, 1)
struct FontPartitionHeader {
    uint32_t magic;
    uint32_t font_size;
};
#pragma pack(pop)

} // namespace

namespace suite {

UiFontService &UiFontService::instance()
{
    static UiFontService service;
    return service;
}

esp_err_t UiFontService::begin()
{
    if (_chinese_font_16 != nullptr) {
        return ESP_OK;
    }
    if (_started) {
        return _last_error;
    }
    _started = true;

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        static_cast<esp_partition_subtype_t>(ESP_PARTITION_SUBTYPE_ANY),
        FONT_PARTITION_LABEL
    );
    if (partition == nullptr) {
        _last_error = ESP_ERR_NOT_FOUND;
        ESP_LOGW(TAG, "Font partition '%s' not found", FONT_PARTITION_LABEL);
        return _last_error;
    }

    const void *mapped = nullptr;
    esp_err_t ret = esp_partition_mmap(
        partition,
        0,
        partition->size,
        ESP_PARTITION_MMAP_DATA,
        &mapped,
        &_font_mmap_handle
    );
    if (ret != ESP_OK) {
        _last_error = ret;
        ESP_LOGW(TAG, "Map font partition failed: %s", esp_err_to_name(ret));
        return _last_error;
    }

    if (partition->size < FONT_PARTITION_HEADER_SIZE) {
        _last_error = ESP_ERR_INVALID_SIZE;
        ESP_LOGW(TAG, "Font partition is too small");
        return _last_error;
    }

    const auto *header = static_cast<const FontPartitionHeader *>(mapped);
    if ((header->magic != FONT_PARTITION_MAGIC) ||
        (header->font_size == 0) ||
        (FONT_PARTITION_HEADER_SIZE + header->font_size > partition->size)) {
        _last_error = ESP_ERR_INVALID_RESPONSE;
        ESP_LOGW(TAG, "Font partition header is invalid");
        return _last_error;
    }

    const uint8_t *font_bytes = static_cast<const uint8_t *>(mapped) + FONT_PARTITION_HEADER_SIZE;
    _mapped_font_data = font_bytes;
    _mapped_font_size = header->font_size;

    _chinese_font_16 = lv_tiny_ttf_create_data_ex(
        _mapped_font_data,
        _mapped_font_size,
        CHINESE_FONT_SIZE,
        LV_FONT_KERNING_NORMAL,
        CHINESE_FONT_CACHE_SIZE
    );
    if (_chinese_font_16 == nullptr) {
        _last_error = ESP_FAIL;
        ESP_LOGW(TAG, "Create Chinese font from mapped partition failed");
        return _last_error;
    }

    _last_error = ESP_OK;
    ESP_LOGI(TAG, "Loaded Chinese font from mapped partition (%u bytes)", static_cast<unsigned>(_mapped_font_size));
    return ESP_OK;
}

const lv_font_t *UiFontService::getChineseFont16() const
{
    return (_chinese_font_16 != nullptr) ? _chinese_font_16 : &lv_font_simsun_16_cjk;
}

bool UiFontService::hasChineseFont16() const
{
    return _chinese_font_16 != nullptr;
}

esp_err_t UiFontService::getLastError() const
{
    return _last_error;
}

} // namespace suite
