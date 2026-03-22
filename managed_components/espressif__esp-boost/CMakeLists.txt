#
# Set variables
#

# Define configuration options with default values
if(NOT DEFINED CONFIG_BOOST_MATH_ENABLED)
    set(CONFIG_BOOST_MATH_ENABLED OFF)
endif()

if(NOT DEFINED CONFIG_BOOST_SERIALIZATION_ENABLED)
    set(CONFIG_BOOST_SERIALIZATION_ENABLED OFF)
endif()

set(COMPONENT_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

# Collect all source files
file(GLOB_RECURSE ALL_COMPONENT_SRC_FILES ${COMPONENT_SRC_DIR}/*.cpp ${COMPONENT_SRC_DIR}/*.c)

# Filter out math and serialization files based on configuration
set(COMPONENT_SRC_FILES ${ALL_COMPONENT_SRC_FILES})

if(NOT CONFIG_BOOST_MATH_ENABLED)
    # Remove math directory files
    list(FILTER COMPONENT_SRC_FILES EXCLUDE REGEX ".*/boost/math.*\\.(cpp|c)$")
    message(STATUS "Boost Math library disabled - excluding math files")
endif()

if(NOT CONFIG_BOOST_SERIALIZATION_ENABLED)
    # Remove serialization directory files
    list(FILTER COMPONENT_SRC_FILES EXCLUDE REGEX ".*/boost/serialization.*\\.(cpp|c)$")
    message(STATUS "Boost Serialization library disabled - excluding serialization files")
endif()

set(COMPONENT_INCLUDE_DIRS ${COMPONENT_SRC_DIR})
set(COMPONENT_PUBLIC_REQUIRES pthread)

#
# Register component
#
idf_component_register(${COMPONENT_LIB}
    SRCS
        ${COMPONENT_SRC_FILES}
    INCLUDE_DIRS
        ${COMPONENT_INCLUDE_DIRS}
    REQUIRES
        ${COMPONENT_PUBLIC_REQUIRES}
)

# esp-boost must be building from ESP platform
if (NOT ESP_PLATFORM)
    message(FATAL_ERROR "ESP_PLATFORM not defined. Please build from the ESP platform.")
endif()

# set necessary macros
if ((NOT DEFINED CONFIG_COMPILER_CXX_RTTI) OR (NOT DEFINED CONFIG_COMPILER_CXX_EXCEPTIONS))
    # Try to detect sdkconfig.h
    set(SDKCONFIG_PATH "${CMAKE_BINARY_DIR}/config/sdkconfig.h")
    if(EXISTS ${SDKCONFIG_PATH})
        message(STATUS "Found sdkconfig.h at ${SDKCONFIG_PATH}")

        file(READ ${SDKCONFIG_PATH} SDKCONFIG_CONTENT)

        string(REGEX MATCH "#define CONFIG_COMPILER_CXX_RTTI 1" _rtti "${SDKCONFIG_CONTENT}")
        string(REGEX MATCH "#define CONFIG_COMPILER_CXX_EXCEPTIONS 1" _exceptions "${SDKCONFIG_CONTENT}")

        set(CONFIG_COMPILER_CXX_RTTI OFF)
        set(CONFIG_COMPILER_CXX_EXCEPTIONS OFF)

        if (_rtti)
            message(STATUS "RTTI is enabled")
            set(CONFIG_COMPILER_CXX_RTTI ON)
        endif()

        if (_exceptions)
            message(STATUS "Exceptions are enabled")
            set(CONFIG_COMPILER_CXX_EXCEPTIONS ON)
        endif()
    else()
        message(FATAL_ERROR "sdkconfig.h not found.")
    endif()

    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        CONFIG_COMPILER_CXX_RTTI=$<BOOL:${CONFIG_COMPILER_CXX_RTTI}>
        CONFIG_COMPILER_CXX_EXCEPTIONS=$<BOOL:${CONFIG_COMPILER_CXX_EXCEPTIONS}>
    )
else()
    message(STATUS "CONFIG_COMPILER_CXX_RTTI and CONFIG_COMPILER_CXX_EXCEPTIONS are inherited from upper layer.")
endif()
