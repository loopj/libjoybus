# Generate PIO headers
pico_generate_pio_header(joybus ${CMAKE_CURRENT_LIST_DIR}/joybus_host.pio)
pico_generate_pio_header(joybus ${CMAKE_CURRENT_LIST_DIR}/joybus_target.pio)

# Add platform-specific source files
target_sources(joybus INTERFACE joybus.c)

# Link with the necessary pico-sdk libraries
target_link_libraries(
  joybus
  INTERFACE
    pico_base_headers
    hardware_clocks
    hardware_dma
    hardware_gpio
    hardware_irq
    hardware_pio
    hardware_timer
)
