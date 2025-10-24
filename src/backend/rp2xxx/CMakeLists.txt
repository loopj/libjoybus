# Generate PIO headers
pico_generate_pio_header(joybus ${CMAKE_CURRENT_LIST_DIR}/joybus_host.pio)
pico_generate_pio_header(joybus ${CMAKE_CURRENT_LIST_DIR}/joybus_target.pio)

# Add RP2XXX-specific source files
target_sources(joybus INTERFACE joybus.c)

# Make sure we link with the necessary Pico libraries
target_link_libraries(joybus INTERFACE pico_base_headers hardware_pio hardware_dma)
