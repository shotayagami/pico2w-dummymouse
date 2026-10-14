# Resolve the Pico SDK from PICO_SDK_PATH, or from ../pico-sdk by default.
if(NOT PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
    else()
        get_filename_component(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../pico-sdk" ABSOLUTE)
    endif()
endif()

if(EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    include("${PICO_SDK_PATH}/pico_sdk_init.cmake")
else()
    message(FATAL_ERROR "Pico SDK not found at '${PICO_SDK_PATH}'. Set PICO_SDK_PATH to the SDK directory.")
endif()
