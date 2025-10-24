# Interactive Joybus Shell Example

Zephyr application which presents an interactive console for sending Joybus
commands to connected Joybus targets.

Uses the Zephyr [Shell](https://docs.zephyrproject.org/latest/services/shell/index.html) OS service to provide an interactive command line interface

```bash
# Initialize the Zephyr workspace
west init -l .
west update

# Build the project (replace xg22_ek2710a with your board)
west build -p -b xg22_ek2710a

# Flash the project
west flash
```
