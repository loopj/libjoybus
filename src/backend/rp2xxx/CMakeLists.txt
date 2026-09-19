# Add backend-specific source files
target_sources(joybus INTERFACE ${JOYBUS_RP2XXX_SOURCES})

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
