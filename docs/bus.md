# Bus and Backends

## Core

```{eval-rst}
.. c:autodoc:: include/joybus/bus.h
```

## Backends

A backend is initialized once, then used through the core API above via the ``JOYBUS()`` cast. Each backend exposes an instance structure that embeds {c:struct}`joybus` as its first member, and an initialization function.

### RP2xxx

Raspberry Pi RP2040 and RP2350.

```{eval-rst}
.. c:autostruct:: joybus_rp2xxx
   :file: include/joybus/backend/rp2xxx.h

.. c:autofunction:: joybus_rp2xxx_init
   :file: include/joybus/backend/rp2xxx.h
```

### Silicon Labs EFR32

Silicon Labs EFM32 and EFR32.

```{eval-rst}
.. c:autostruct:: joybus_gecko
   :file: include/joybus/backend/gecko.h

.. c:autofunction:: joybus_gecko_init
   :file: include/joybus/backend/gecko.h
```
