# UART expansion board

Last reviewed: 2026-10-27. [All modules](README.md)

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

Implemented in [py32f040_Expansion_Module](../../py32f040_Expansion_Module/README.md),
created from the F040 template. Flash the same binary into PY1 and PY2. Each
MCU runs from its 24 MHz crystal with PLL x3 at72 MHz and provides four
115200 8N1 UART ports plus SPI1 upstream slave/SPI2 downstream master.

The first bridge protocol uses64-byte CRC-protected envelopes, relative hop0..7
and logical USART port1..4. UART payloads are transparent, including the IR binary
byte. Host sends one request and polls FETCH for its cached reply. Strictly
increasing32-bit IDs prevent duplicate UART writes/consuming reads during the
powered session; timeouts never cause automatic command retransmission.

Each port has256-byte RX/TX rings and sticky error/overflow flags. WRITE, READ,
STATUS, CLEAR and INFO provide queue access and explicit reachability probing.
The [project README](../../py32f040_Expansion_Module/README.md) specifies timings,
connector-to-port mapping, error semantics, host helper, limits and reset behavior.

Main ESP32/main-board PY32 hub application integration remains pending. I2S
pass-through adds no active hop. Continuous lossless full-rate traffic from all
UARTs is not guaranteed; physical clock/cable/cascade limits must be measured.

## Validation

Firmware builds; ARM-emulated tests exercise the actual routing, UART queues,
and both SPI byte drivers, with independent MCU instances for cascaded links.
See project README and Tests/run_tests.py for rerun instructions and coverage.
No physical boards have been tested. Check every UART/header, inter-MCU SPI,
NSS/MISO behavior, simultaneous traffic, disconnect/reset/recovery and cable
signal integrity when hardware arrives.
