idf_component_register(
    SRCS "suite_ui_font_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        lvgl
        esp_partition
        waveshare__esp32_s3_touch_amoled_2_06
)
