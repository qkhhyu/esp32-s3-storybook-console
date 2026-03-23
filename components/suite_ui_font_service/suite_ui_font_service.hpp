#pragma once

#include <cstddef>
#include "esp_err.h"
#include "esp_partition.h"
#include "lvgl.h"

namespace suite {

class UiFontService {
public:
    static UiFontService &instance();

    esp_err_t begin();
    const lv_font_t *getChineseFont16() const;
    bool hasChineseFont16() const;
    esp_err_t getLastError() const;

private:
    UiFontService() = default;
    UiFontService(const UiFontService &) = delete;
    UiFontService &operator=(const UiFontService &) = delete;

    bool _started = false;
    lv_font_t *_chinese_font_16 = nullptr;
    const void *_mapped_font_data = nullptr;
    size_t _mapped_font_size = 0;
    esp_partition_mmap_handle_t _font_mmap_handle = 0;
    esp_err_t _last_error = ESP_OK;
};

} // namespace suite
