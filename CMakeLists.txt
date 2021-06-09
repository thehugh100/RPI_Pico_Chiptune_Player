add_executable(blink
        chiptune.cpp
        )

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(blink pico_stdlib hardware_pwm)

pico_enable_stdio_usb(blink 1)
pico_enable_stdio_uart(blink 0)


pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)

pico_set_boot_stage2(blink slower_boot2)

# create map/bin/hex file etc.
pico_add_extra_outputs(blink)

# add url via pico_set_program_url
example_auto_set_url(blink)
