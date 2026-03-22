idf_component_register(
    SRCS "esp_brookesia_app_power.cpp"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
    REQUIRES brookesia_core XPowersLib waveshare__esp32_s3_touch_amoled_2_06
)
