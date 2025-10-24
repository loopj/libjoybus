# Contributing

## Implementing a Backend

New backends should follow the structure of existing backends in `src/backend/`.

Each backend is expected to implement a `joybus_backend_init(...)` function, and provide implementations for each of the functions in the `joybus_api` struct:

```c
static const struct joybus_api mybackend_api = {
  .enable            = joybus_mybackend_enable,
  .disable           = joybus_mybackend_disable,
  .transfer          = joybus_mybackend_transfer,
  .target_register   = joybus_mybackend_target_register,
  .target_unregister = joybus_mybackend_target_unregister,
};

int joybus_mybackend_init(struct joybus *bus, ...)
{
  bus->api    = &mybackend_api;

  // Rest of initialization code...

  return 0;
}
```

Since backends need to clock in and out pulses on the bus with microsecond precision, bit-banging is typically not feasible. Using dedicated hardware peripherals which can capture and generate signals with minimal CPU intervention is recommended.

Some examples of approaches:

- Use a timer peripheral in capture mode to fill a buffer with edge timings. Once enough data has been collected, measure pulse widths in software.
- Use a timer peripheral in PWM mode to clock out pulses.
- Use a serial communication peripheral (e.g. SPI, USART) to clock out pulses after encoding them into the suitable line coding.
- Some platforms (e.g. RP2040) even have programmable IO peripherals which can "bit-bang" the protocol completely in hardware.

The turnaround times between transmitting and receiving data are also critical. An OEM controller responds to commands from a host within 2-3 µs, so your backend must be able to switch from transmit to receive mode immediately in order to capture the response.

Additionally, when in target mode, the backend must be able to prepare and send responses to host commands within 100 µs, ideally much faster.

### Backend Checklist

When implementing a new backend, the following requirements are considered the bare minimum:

- [ ] Backend must implement all functions in `joybus_api`
- [ ] Transfers must fail with `JOYBUS_ERR_DISABLED` if backend is not enabled
- [ ] Transfers must fail with `JOYBUS_ERR_BUSY` if another transfer is in progress
- [ ] Transfers must fail with `JOYBUS_ERR_TIMEOUT` if target does not begin replying within 100 µs
- [ ] Transfers must fail with `JOYBUS_ERR_TIMEOUT` if each subsequent byte is not received within 100 µs
- [ ] Backend must enforce a minimum delay of 20 µs between each transfer
- [ ] Target must begin responding to any command within 100 µs
- [ ] When target read mode is enabled, the bus must be idle before reading data to ensure we are not in the middle of a transfer
- [ ] If an error occurs during a target read, wait for the bus to be idle before starting to read the next command

Ideally, your backend should also meet the following criteria:

- [ ] Target must begin responding to any command within 10 µs
- [ ] Backend supports 4 simultaneous buses

## Running tests

Most of the higher level functionality of `libjoybus` can be tested using the
`loopback` backend. This backend simulates Joybus communication by connecting a
host and target together in software.

Build the test suite

```bash
cmake -Bbuild -DJOYBUS_BACKEND_LOOPBACK=1 && cmake --build build
```

Run the tests

```bash
ctest --test-dir build --output-on-failure
```
