cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w)
set(PICO_PLATFORM rp2350)

include(pico_sdk_import.cmake)

project(piconet C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(piconet
    main.c
    bus.c
    sio.c
    net.c
    hayes.c
    telnet.c
)

pico_generate_pio_header(piconet ${CMAKE_CURRENT_LIST_DIR}/bus_pio.pio)

target_include_directories(piconet PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(piconet
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    hardware_irq
    pico_cyw43_arch_lwip_threadsafe_background
)

pico_enable_stdio_usb(piconet 1)
pico_enable_stdio_uart(piconet 0)

pico_add_extra_outputs(piconet)
