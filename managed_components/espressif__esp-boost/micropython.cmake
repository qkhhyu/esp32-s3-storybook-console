# This file is to be given as "make USER_C_MODULES=..." when building Micropython port

add_library(usermod_esp_boost INTERFACE)

# Find all source files in the `src` directory.
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
file(GLOB_RECURSE SRCS_C ${SRC_DIR}/*.c)
file(GLOB_RECURSE SRCS_CXX ${SRC_DIR}/*.cpp)

# Add source files to the library.
target_sources(usermod_esp_boost INTERFACE ${SRCS_C} ${SRCS_CXX})

# Add the current directory as an include directory.
target_include_directories(usermod_esp_boost INTERFACE ${SRC_DIR})

# Add compile options. Since the target is not created by `idf_component_register()`, we need to add the `ESP_PLATFORM` define manually.
target_compile_definitions(usermod_esp_boost
    INTERFACE
        -DESP_PLATFORM
)

# Link INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_esp_boost)

# Include core source components.
include(${MICROPY_DIR}/py/py.cmake)

micropy_gather_target_properties(usermod_esp_boost)
