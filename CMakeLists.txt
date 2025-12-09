# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

# Importa o Pico SDK 
include(pico_sdk_import.cmake)

project(smartstop_bitdoglab C CXX ASM)

pico_sdk_init()

add_executable(smartstop_bitdoglab
    main.c
    smartstop.c
)

target_link_libraries(smartstop_bitdoglab
    pico_stdlib
)

# Habilita saída via USB (Monitor Serial)
pico_enable_stdio_usb(smartstop_bitdoglab 1)
pico_enable_stdio_uart(smartstop_bitdoglab 0)

pico_add_extra_outputs(smartstop_bitdoglab)
