idf_component_register(
    SRCS "esp_brookesia_app_gravity_blocks.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        brookesia_core
        storybook_ui_helpers
        storybook_qmi8658
        waveshare__esp32_s3_touch_amoled_2_06
    WHOLE_ARCHIVE
)

set_source_files_properties(
    esp_brookesia_app_gravity_blocks.cpp
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)

