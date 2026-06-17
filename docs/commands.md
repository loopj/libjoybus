# Commands

Joybus command functions to use when acting as a host on the bus.

Each of these functions maps to a low-level Joybus command, for example {c:func}`joybus_n64_read` maps to the N64 controller read command (`0x01`).

Each function has both a synchronous and asynchronous variant. The synchronous variant blocks until the command completes, while the asynchronous variant calls the provided callback from interrupt context when the command completes.

```{eval-rst}
.. c:autodoc:: include/joybus/host/common.h
.. c:autodoc:: include/joybus/host/n64.h
.. c:autodoc:: include/joybus/host/gcn.h
```
