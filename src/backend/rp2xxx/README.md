# Gecko Backend for rp2xxx

## Usage

```c
// Store the bus instance somewhere
struct joybus_rp2xxx rp2xxx_bus;

// Initialize the rp2xxx joybus interface
// Parameters: (joybus_rp2xxx instance, GPIO, PIO instance)
joybus_rp2xxx_init(&rp2xxx_bus, 6, pio0);

// Cast to a generic joybus pointer, for ease of use with libjoybus functions
struct joybus *bus = JOYBUS(&rp2xxx_bus);

// Enable the bus
joybus_enable(bus);

// Use the bus
joybus_transfer(bus, ...);
```

## Checklist

- [x] Backend must implement all functions in `joybus_api`
- [x] Transfers must fail with `JOYBUS_ERR_DISABLED` if backend is not enabled
- [x] Transfers must fail with `JOYBUS_ERR_BUSY` if another transfer is in progress
- [x] Transfers must fail with `JOYBUS_ERR_TIMEOUT` if target does not begin replying within 100 µs
- [x] Transfers must fail with `JOYBUS_ERR_TIMEOUT` if each subsequent byte is not received within 100 µs
- [x] Backend must enforce a minimum delay of 20 µs between each transfer
- [x] Target must begin responding to any command within 100 µs
- [x] When target read mode is enabled, the bus must be idle before reading data to ensure we are not in the middle of a transfer
- [x] If an error occurs during a target read, wait for the bus to be idle before starting to read the next command

Ideally, your backend should also meet the following criteria:

- [ ] Target must begin responding to any command within 10 µs
- [x] Backend supports 4 simultaneous buses
- [ ] Zephyr implementation available
