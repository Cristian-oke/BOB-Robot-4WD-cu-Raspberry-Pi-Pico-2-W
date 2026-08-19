cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

set(PICO_BOARD pico2_w CACHE STRING "Board type")

#embed index.html ca array C linkuit in firmware
function(embed_file target input_file symbol_name)
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/${symbol_name}.c")
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${input_file}
            -DOUTPUT_FILE=${output_file}
            -DSYMBOL=${symbol_name}
            -P ${CMAKE_CURRENT_LIST_DIR}/embed_file.cmake
        DEPENDS ${input_file}
    )
    target_sources(${target} PRIVATE ${output_file})
endfunction()

include(pico_sdk_import.cmake)

project(BOB_CAR C CXX ASM)

pico_sdk_init()

# toate fisierele sursa
add_executable(BOB_CAR
    BOB_CAR.c
    motors.c
    sensors.c
    ble_uart.c
    wifi_server.c
    obstacle.c
    follow_me.c
    buzzer.c
    dhcpserver.c
    # Nordic SPP Service Server (NUS oficial din BTstack)
    ${PICO_SDK_PATH}/lib/btstack/src/ble/gatt-service/nordic_spp_service_server.c
)

#embed pagina HTML giroscop in firmware
embed_file(BOB_CAR "${CMAKE_CURRENT_LIST_DIR}/lhf_web/index.html" "lhf_web_index_html")

pico_set_program_name(BOB_CAR "BOB_CAR")
pico_set_program_version(BOB_CAR "2.0")

# USB stdio pentru debug, UART dezactivat
pico_enable_stdio_uart(BOB_CAR 0)
pico_enable_stdio_usb(BOB_CAR 1)

#genereaza header GATT din fisierul .gatt (NUS Service pentru BLE)
pico_btstack_make_gatt_header(BOB_CAR PRIVATE "${CMAKE_CURRENT_LIST_DIR}/bob_ble.gatt")

# include directoare
target_include_directories(BOB_CAR PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}  # pentru bob_ble.h generat
)

# Librarii necesare
target_link_libraries(BOB_CAR
    pico_stdlib
    pico_multicore   # dual-core support
    hardware_pwm     # PWM pentru motoare (mod giroscop)
    hardware_gpio
    hardware_timer
    hardware_irq
    # Wi-Fi + lwIP (mod giroscop, AP mode)
    pico_cyw43_arch_lwip_threadsafe_background
    # BLE (mod manual + Follow-Me RSSI)
    pico_btstack_ble
    pico_btstack_cyw43
)

pico_add_extra_outputs(BOB_CAR)
