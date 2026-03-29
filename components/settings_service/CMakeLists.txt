idf_component_register(
    SRCS "settings_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_app_format
        esp_event
        esp_hw_support
        esp_netif
        esp_psram
        esp_wifi
        nvs_flash
        spi_flash
        storybook_axp2101
        espressif__esp_codec_dev
        waveshare__esp32_s3_touch_amoled_2_06
)


