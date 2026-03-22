idf_component_register(
    SRCS "suite_settings_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_event
        esp_netif
        esp_wifi
        lwip
        nvs_flash
        bsp_extra
        waveshare__esp32_s3_touch_amoled_2_06
)
