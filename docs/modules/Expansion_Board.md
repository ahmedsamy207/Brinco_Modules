# UART expansion board

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Add eight UART module ports using two PY32 hubs. One expansion board plus the
main board provides twelve UART ports. Supports the intended cascaded expansion
topology, including connection through the I2S board's SPI pass-through.

## Confirmed hardware

- Two PY32F040C1BT6 MCUs, each on 5 V with a separate 24 MHz crystal and SWD.
- EXP_IN reaches PY1 SPI1; PY1 SPI2 connects internally to PY2 SPI1.
- PY2 SPI2 reaches EXP_OUT for another downstream stage.
- Four UARTs per MCU. H1/H2/H3/H4 are PY1 USART2/3/4/1 respectively;
  H5/H6/H7/H8 are PY2 USART1/4/2/3 respectively.

Source: [expansion schematic](../../Modules%20Schematics/Schematic_Expansion_Board_UART.pdf).
Detailed pins, six-pin IN/OUT mapping and topology:
[Expansion Reference](../../Modules%20Schematics/Expansion_Reference.md).

## Firmware and protocol status

Not implemented. Copy the F040 template when implementation starts. Both MCU
roles need upstream SPI reception, local UART service and downstream forwarding.
This is an active chain of SPI segments, not passive parallel SPI wiring.

## Open decisions and next work

Define hub/hop and port addressing, boot-time discovery or fixed configuration,
buffering, response latency, SPI mode/rate, disconnect handling and chain limits.
Decide whether one configurable firmware image can serve both expansion MCUs.
No routing packet format or enumeration algorithm has been approved yet.

## Validation

Hardware pending. Test each UART locally, both inter-MCU SPI directions, multiple
hops, downstream disconnect/recovery and continued service of unaffected ports.
