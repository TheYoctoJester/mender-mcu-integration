cmake_minimum_required(VERSION 3.20.0)

# ESP32-S3-DevkitC is the default board
set(BOARD
    "esp32s3_devkitc/esp32s3/procpu"
    CACHE
    STRING
    "Board for the Zephyr application"
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(mender-mcu-integration)

target_sources(app PRIVATE
    src/main.c
    src/utils/netup.c
    src/utils/certs.c
)

target_include_directories(app PUBLIC
    src
)

if(CONFIG_MENDER_APP_NOOP_UPDATE_MODULE MATCHES "y")
    target_sources(app PRIVATE src/modules/noop-update-module.c)
endif()

if(CONFIG_DISPLAY)
    target_sources(app PRIVATE src/utils/display.c)
endif()

# Self-signed CA certificate for docker.mender.io demo server
generate_inc_file_for_target(app
    "certs/docker-mender-io.cer"
    "${ZEPHYR_BINARY_DIR}/include/generated/docker-mender-io.cer.inc"
)

file(COPY_FILE "config-tls-mender.h" "${ZEPHYR_BINARY_DIR}/include/generated/config-tls-mender.h")

# Generate RGB565 logo header from PNG at build time
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(DEFINED ENV{MENDER_LOGO_FILE})
    set(LOGO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/$ENV{MENDER_LOGO_FILE}")
else()
    set(LOGO_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/logo.png")
endif()
set(LOGO_OUTPUT "${ZEPHYR_BINARY_DIR}/include/generated/mender_logo.h")
set(LOGO_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/png_to_rgb565.py")

add_custom_command(
    OUTPUT ${LOGO_OUTPUT}
    COMMAND ${Python3_EXECUTABLE} ${LOGO_SCRIPT}
        --input ${LOGO_INPUT}
        --output ${LOGO_OUTPUT}
        --width 280
        --height 94
        --swap-bytes
    DEPENDS ${LOGO_INPUT} ${LOGO_SCRIPT}
    COMMENT "Converting logo.png to RGB565 header"
)
add_custom_target(generate_logo DEPENDS ${LOGO_OUTPUT})
add_dependencies(app generate_logo)
