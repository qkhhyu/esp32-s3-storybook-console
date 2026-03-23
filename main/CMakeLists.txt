idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        brookesia_core
        suite_settings_service
        suite_ui_font_service
        brookesia_app_power
        brookesia_app_settings
        brookesia_app_lvgl_showcase
        brookesia_app_gyro_shapes
        brookesia_app_spectrum
        brookesia_app_video
        XPowersLib
        waveshare__esp32_s3_touch_amoled_2_06
)

set(FONT_PARTITION_IMAGE ${CMAKE_BINARY_DIR}/storage.bin)
set(FONT_SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../assets_storage/fonts/simyou.ttf)
set(FONT_PARTITION_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../tools/make_font_partition.py)
partition_table_get_partition_info(FONT_PARTITION_SIZE "--partition-name storage" "size")

add_custom_command(
    OUTPUT ${FONT_PARTITION_IMAGE}
    COMMAND ${PYTHON} ${FONT_PARTITION_SCRIPT} ${FONT_SOURCE_FILE} ${FONT_PARTITION_IMAGE} ${FONT_PARTITION_SIZE}
    DEPENDS ${FONT_SOURCE_FILE} ${FONT_PARTITION_SCRIPT}
    VERBATIM
)

add_custom_target(storage_font_bin ALL DEPENDS ${FONT_PARTITION_IMAGE})

esptool_py_flash_to_partition(flash "storage" "${FONT_PARTITION_IMAGE}")
add_dependencies(flash storage_font_bin)

target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)

function(get_component_library component_name output_var)
    idf_build_get_property(build_components BUILD_COMPONENTS)
    set(TARGET_COMPONENT "")
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "${component_name}" OR COMPONENT MATCHES "espressif__${component_name}")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()

    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component '${component_name}' not found.")
    else()
        idf_component_get_property(COMPONENT_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
        set(${output_var} ${COMPONENT_LIB} PARENT_SCOPE)
    endif()
endfunction()

get_component_library("lvgl" LVGL_LIB)
target_compile_options(
    ${LVGL_LIB}
    PUBLIC
        "-Wno-attributes"
)
