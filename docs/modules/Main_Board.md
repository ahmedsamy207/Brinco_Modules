# Main board

Last reviewed: 2026-10-27. [All modules](README.md)

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
The [expansion firmware](../../py32f040_Expansion_Module/README.md) now defines
an initial hop/port SPI bridge protocol. Main-board ESP32/PY32 application
integration remains pending; the template alone does not provide it. Existing
sensor commands remain unchanged and are carried as raw UART payloads.

## Open decisions and next work

Integrate the expansion bridge protocol and define application responsibilities,
sensor discovery, response assembly and application failure handling. Confirm the populated IMU option/BOM and exact
level-shifter part behavior. Preserve PSRAM and boot-strapping pin constraints.
Audio features and display command interfaces need product requirements.

## Validation

Hardware pending. Later verify power/startup, cable mapping, SPI/UART timing,
all port assignments, expansion forwarding and simultaneous peripheral use.
