cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
pico_sdk_init()
project(pivend)

add_executable(pivend
        pivend.c 
        vend_driver.c
        )
pico_enable_stdio_usb(pivend 1)
pico_enable_stdio_uart(pivend 0)


# pull in common dependencies
target_link_libraries(pivend pico_stdlib)

# create map/bin/hex file etc.
pico_add_extra_outputs(pivend)
