cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Target board")

include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

project(dummymouse C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

option(DUMMYMOUSE_RUST "Build the Rust firmware application" OFF)
set(DUMMYMOUSE_REPORT_INTERVAL_MS 60000 CACHE STRING "Mouse report interval in milliseconds")

if(DUMMYMOUSE_RUST)
    find_program(CARGO_EXECUTABLE cargo REQUIRED)
    set(RUST_TARGET thumbv8m.main-none-eabi)
    set(RUST_MANIFEST ${CMAKE_CURRENT_LIST_DIR}/rust/Cargo.toml)
    set(RUST_ELF ${CMAKE_CURRENT_BINARY_DIR}/dummymouse-rust.a)
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rust_report_interval.txt
         CONTENT "${DUMMYMOUSE_REPORT_INTERVAL_MS}")
    add_custom_command(
        OUTPUT ${RUST_ELF}
        COMMAND ${CMAKE_COMMAND} -E env DUMMYMOUSE_REPORT_INTERVAL_MS=${DUMMYMOUSE_REPORT_INTERVAL_MS}
                ${CARGO_EXECUTABLE} build --manifest-path ${RUST_MANIFEST}
                --target ${RUST_TARGET} --release
                --target-dir ${CMAKE_CURRENT_BINARY_DIR}/cargo
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_BINARY_DIR}/cargo/${RUST_TARGET}/release/libdummymouse_rust.a
                ${RUST_ELF}
        DEPENDS rust/Cargo.toml rust/build.rs rust/src/lib.rs rust/.cargo/config.toml
                ${CMAKE_CURRENT_BINARY_DIR}/rust_report_interval.txt
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        VERBATIM
    )
    add_custom_target(dummymouse_rust_build DEPENDS ${RUST_ELF})
    add_executable(dummymouse src/rust_main.c ${RUST_ELF})
    add_dependencies(dummymouse dummymouse_rust_build)
    set_source_files_properties(${RUST_ELF} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    target_link_libraries(dummymouse PRIVATE ${RUST_ELF})
else()
    add_executable(dummymouse src/main.c)
endif()
target_include_directories(dummymouse PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_compile_definitions(dummymouse PRIVATE REPORT_INTERVAL_MS=${DUMMYMOUSE_REPORT_INTERVAL_MS})
target_link_libraries(dummymouse PRIVATE
    pico_stdlib
    pico_btstack_ble
    pico_btstack_cyw43
    pico_cyw43_arch_none
)
pico_btstack_make_gatt_header(dummymouse PRIVATE ${CMAKE_CURRENT_LIST_DIR}/dummymouse.gatt)
pico_enable_stdio_usb(dummymouse 1)
pico_enable_stdio_uart(dummymouse 0)
pico_add_extra_outputs(dummymouse)
