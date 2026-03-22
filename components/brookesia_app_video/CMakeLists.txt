idf_component_register(
    SRCS "esp_brookesia_app_video.cpp"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
    REQUIRES brookesia_core bsp_extra waveshare__esp32_s3_touch_amoled_2_06
)
