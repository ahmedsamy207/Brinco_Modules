# Main board firmware reference

Reviewed 2026-10-22 against `Schematic_Main_Board.pdf` as added in commit
`00176b3`. The PDF has six pages; each title block says Sheet_1 / 1/1, so
page numbers below mean PDF page positions. Recheck this reference against
future schematic revisions before using pin assignments.

## Architecture and page guide

| PDF page | Circuit |
|---|---|
| 1 | ESP32-S3-WROOM-1 (N8R8), level shifter, TFT and I2S headers |
| 2 | USB-C connector and USBLC6 protection |
| 3 | IP5306 battery/charging/5 V supply, 3.3 V regulator, camera regulators |
| 4 | LSM9DS1 and LSM6DS3 + LIS3MDL IMU circuits on the ESP I2C bus |
| 5 | Camera FPC, DVP signals, and camera supply decoupling |
| 6 | PY32F040C1BT6, four module UART headers, SPI expansion, buttons, LEDs, SWD |

The connection topology supports an ESP32 host and a PY32 peripheral hub:

`ESP32 SPI (ESP2_*) <-> level shifter <-> PY32 SPI1 (PY1_*)`

`PY32 USART1..4 <-> four external module UART connectors`

The ESP32 separately connects to TFT SPI, I2S audio, I2C sensors/camera
control, camera DVP, and native USB. This is a hardware topology, not an
existing SPI bridge protocol; that protocol still needs firmware design.

## PY32 UART module ports (PDF page 6)

TX/RX net names refer to the main board. AF mappings below are cross-checked
against the local `PY32F040_Datasheet_V1.1.pdf`, Tables 3-3 and 3-4.

| Header | Main board interface | Main TX GPIO / package pin | Main RX GPIO / package pin | AF |
|---|---|---|---|---|
| H4 | USART1, PY_TX1 / PY_RX1 | PA9 / 30 | PA10 / 31 | AF1 |
| H2 | USART2, PY_TX2 / PY_RX2 | PA2 / 12 | PA3 / 13 | AF1 |
| H3 | USART3, PY_TX3 / PY_RX3 | PB10 / 21 | PB11 / 22 | AF4 |
| H1 | USART4, PY_TX4 / PY_RX4 | PA0 / 10 | PA1 / 11 | AF4 |

Header numbers do not follow UART numbers. Do not infer a peripheral from
the connector number. A module's internal USART number is independent of
the main board USART to which its cable connects.

Each main board UART connector is numbered:

| Pin | Main board signal |
|---|---|
| 1 | 5 V |
| 2 | Main TX -> module RX |
| 3 | Main RX <- module TX |
| 4 | GND |

The ultrasonic and IR module schematics number their connectors in the
opposite order: 1 GND, 2 main RX/module TX, 3 main TX/module RX, 4 5 V.
This reversal is intentional: the designer confirmed that the modules use
reverse-side JST cables, as illustrated in the supplied cable reference image.
The intended electrical correspondence is main 1 -> module 4, 2 -> 3,
3 -> 2, 4 -> 1, connecting 5 V to 5 V, GND to GND, and each TX to the
opposite MCU's RX. This is the specified cable arrangement, not a schematic
numbering discrepancy. Use reverse-side cables for this connection.

The main board PY32 and these two module MCUs are shown powered from 5 V.
Their UART links do not pass through the ESP32's 3.3 V level shifter.
The schematic does not define baud rates, command syntax, module discovery,
or fixed assignments of module types to headers.

## PY32 SPI, clock, and local controls (PDF page 6)

| Net/function | GPIO | Package pin |
|---|---|---:|
| PY1_CS | PA4 | 14 |
| PY1_SCK | PA5 | 15 |
| PY1_MISO | PA6 | 16 |
| PY1_MOSI | PA7 | 17 |
| PY2_CS | PB9 | 46 |
| PY2_SCK | PB8 | 45 |
| PY2_MISO | PB6 | 42 |
| PY2_MOSI | PB7 | 43 |
| LED1 | PB15 | 28 |
| LED2 | PB14 | 27 |
| BTN1 | PB13 | 26 |
| BTN2 | PB12 | 25 |
| SWDIO | PA13 | 34 |
| SWCLK | PA14 | 37 |
| NRST | PF2 | 7 |

PY1 is the SPI1 link to the level shifter/ESP. PY2 goes to the six-pin EXP
header: 1 MISO, 2 CS, 3 5 V, 4 SCK, 5 GND, 6 MOSI. This pin order was
confirmed by a magnified review of the connector symbol. Do not conflate the
expansion connector with the four UART module connectors.

The PY32 crystal is **24 MHz**, unlike the **32 MHz** crystals in the two
module schematics reviewed so far. A main board clock configuration must
use its own oscillator value rather than copying a module's HSE value.
BOOT0/PF8 is grounded. Buttons switch their inputs to GND, so firmware
needs an appropriate pull-up configuration. LED cathodes return to GND
through the shown common ground node; the GPIO drives through 1 kohm resistors.

## ESP32 interfaces (PDF page 1)

| Interface | ESP32 signals |
|---|---|
| SPI to PY32 via U12 | CS GPIO1, SCK GPIO2, MISO GPIO43/TXD0, MOSI GPIO44/RXD0 |
| SPI to TFT via U12 | CS GPIO14, SCK GPIO21, MISO GPIO47, MOSI GPIO48 |
| Level-shifter enable | SPI_EN on GPIO3, with a 10 kohm pull-down |
| Shared I2C bus | SDA GPIO4, SCL GPIO5, 4.7 kohm pull-ups to 3.3 V |
| Speaker audio | LRC GPIO38, CLK GPIO39, DIN GPIO40 |
| Microphone audio | CLK GPIO41, Data GPIO42 |
| Native USB | D- GPIO19, D+ GPIO20 |
| Boot/reset | IO0 and EN jumpers/buttons to GND, pull-ups to 3.3 V |

U12 is labelled LKIB1219SS and connects 3.3 V VCCA to 5 V VCCB. Its first
four channels bridge ESP2_* to PY1_*; the other four bridge ESP1_* to TFT_*.
The exact translator part's electrical behavior and OE requirements must be
verified from its datasheet before implementing initialization/clock limits.
GPIO3 is a strapping pin, and GPIO43/44 are also UART0 pins: ESP startup and
debug configuration must account for these shared uses. The drawing explicitly
reserves GPIO35/36/37 for PSRAM on the N8R8 variant.

The I2S header is connected directly to the ESP audio signals and provides
both 3.3 V and 5 V. It is not routed through the PY32 UART hub.

## Camera, IMUs, and power

Camera DVP assignment: VSYNC GPIO6, HREF GPIO7, PCLK GPIO8, XMCLK GPIO16,
Y9 GPIO15, Y8 GPIO17, Y7 GPIO18, Y6 GPIO9, Y5 GPIO10, Y4 GPIO11,
Y3 GPIO12, Y2 GPIO13. The FPC uses the shared I2C nets. CAM_RESET is shown
pulled up to 2.8 V; no MCU-controlled reset net is shown on page 1.

Page 4 shows both an LSM9DS1 and an LSM6DS3TR-C + LIS3MDLTR arrangement.
The schematic alone does not establish which parts will be populated.
Resolve the BOM and device addresses before implementing IMU discovery.

USB VBUS feeds the IP5306 power circuit. BAT is the battery connection;
the IP5306 circuit supplies the 5 V rail. AP2112K produces 3.3 V for the
ESP and other circuits. Camera regulators produce 1.5 V and 2.8 V, with a
filtered AVDD_2V8 branch. Charging and power switching are shown as hardware
circuits; the drawing does not expose a charger-control firmware interface.

## Firmware implications

The expansion chain is documented in [Expansion_Reference.md](Expansion_Reference.md).
The I2S board passes the expansion SPI nets through; the expansion board
contains two additional PY32 hubs which require active forwarding firmware.

- Continue using each module's own schematic for its MCU pins and clock.
- Keep UART command handling independent of the future ESP/PY32 SPI bridge.
- All four UART links are separate point-to-point ports; no shared-bus
  addressing is implied by the drawing.
- Retain the documented module baud/protocol until a deliberate system-wide
  change is agreed. The schematic cannot establish those software settings.
- Verify cable pin numbering and board revisions during hardware bring-up.
- This reference records schematic connectivity, not tested hardware behavior
  or a complete electrical-design validation.
