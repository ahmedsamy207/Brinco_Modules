# Main board

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose

Central controller: ESP32-S3 handles application features, camera, display/audio,
USB and onboard I2C devices. A PY32F040 hub serves four module UART ports and
the downstream expansion SPI connection.

## Confirmed hardware

- ESP32-S3-WROOM-1 N8R8 on 3.3 V; PY32F040C1BT6 on 5 V.
- ESP-to-PY32 SPI passes through the main board level shifter into PY32 SPI1.
- PY32 SPI2 reaches the six-pin EXP header. PY32 has a 24 MHz crystal.
- Four independent UART headers; reverse-side JST module cables are intentional.
- I2S/PDM audio, TFT SPI, DVP camera, native USB and I2C devices connect to ESP.

Source: [main board schematic](../../Modules%20Schematics/Schematic_Main_Board.pdf).
Detailed pin maps, connector numbering, supply rails and observations:
[Main Board Reference](../../Modules%20Schematics/Main_Board_Reference.md).

## Firmware and protocol status

No main-board application firmware has been implemented in this repository.
The F040 template is a starting point, not a hub implementation. ESP/PY32 SPI
messages, module discovery and expansion routing remain to be defined.
Existing UART module commands are documented on the ultrasonic and IR pages.

## Open decisions and next work

Define application responsibilities, SPI bridge framing and routing, enumeration,
timeouts and failure handling. Confirm the populated IMU option/BOM and exact
level-shifter part behavior. Preserve PSRAM and boot-strapping pin constraints.
Audio features and display command interfaces need product requirements.

## Validation

Hardware pending. Later verify power/startup, cable mapping, SPI/UART timing,
all port assignments, expansion forwarding and simultaneous peripheral use.
