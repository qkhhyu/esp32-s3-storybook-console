idf_component_register(
    SRCS "esp_brookesia_app_gyro_shapes.cpp"
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
    REQUIRES brookesia_core
)
