idf_component_register(
    SRCS "esp_brookesia_app_spectrum.cpp"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
    REQUIRES brookesia_core bsp_extra
)
