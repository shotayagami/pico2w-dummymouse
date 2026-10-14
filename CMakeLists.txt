cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Target board")

include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

project(dummymouse C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_executable(dummymouse src/main.c)
target_include_directories(dummymouse PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
set(DUMMYMOUSE_REPORT_INTERVAL_MS 60000 CACHE STRING "Mouse report interval in milliseconds")
target_compile_definitions(dummymouse PRIVATE REPORT_INTERVAL_MS=${DUMMYMOUSE_REPORT_INTERVAL_MS})
target_link_libraries(dummymouse
    pico_stdlib
    pico_btstack_ble
    pico_btstack_cyw43
    pico_cyw43_arch_none
)
pico_btstack_make_gatt_header(dummymouse PRIVATE ${CMAKE_CURRENT_LIST_DIR}/dummymouse.gatt)
pico_enable_stdio_usb(dummymouse 1)
pico_enable_stdio_uart(dummymouse 0)
pico_add_extra_outputs(dummymouse)
