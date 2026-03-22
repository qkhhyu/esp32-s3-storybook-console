idf_component_register(
    SRCS "esp_brookesia_app_settings.cpp"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
    REQUIRES
        brookesia_core
        suite_settings_service
        bsp_extra
        waveshare__esp32_s3_touch_amoled_2_06
)
