// Number of Joybus channels supported
#define GCCA_JOYBUS_CHANNELS          4

// GPIO pins for each Joybus channel
#define GCCA_JOYBUS_GPIO_CH0          12
#define GCCA_JOYBUS_GPIO_CH1          13
#define GCCA_JOYBUS_GPIO_CH2          2
#define GCCA_JOYBUS_GPIO_CH3          3

// Enables rumble support
#define GCCA_RUMBLE_POWER_AVAILABLE   1

// Poll the Joybus every 1ms (1kHz)
#define GCCA_POLL_INTERVAL            1

// Tell USB hosts that they should poll every 1ms (1kHz)
// NOTE: This differs from the OEM adapter's poll interval of 8ms
#define GCCA_USB_ENDPOINT_INTERVAL    1