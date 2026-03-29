idf_component_register(
    SRCS "esp_brookesia_app_settings.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        brookesia_core
        settings_service
    WHOLE_ARCHIVE
)

set_source_files_properties(
    esp_brookesia_app_settings.cpp
    PROPERTIES
        COMPILE_FLAGS "-Wno-missing-field-initializers"
)
