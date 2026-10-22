# I2S / PDM audio board

Last reviewed: 2026-10-22. [All modules](README.md)

## Purpose agreed with the project owner

Audio expansion: speaker playback from ESP32 I2S and microphone capture into
ESP32 PDM. Six-pin Expansion In/Out allow the SPI expansion chain to pass
through this board. Voice commands, recording and other application features
are not yet specified.

## Confirmed hardware

- MAX98357AETE+T speaker amplifier on 5 V, connected to an 8 ohm/1 W speaker.
- MSM261DDB019 digital microphone on 3.3 V; its L/R pin is grounded.
- Speaker signals: SPK_LRC, SPK_CLK and SPK_DIN.
- Microphone signals: MIC_CLK and MIC_Data.
- There is no onboard MCU and no SPI slave consuming a chip select.
- EXP_IN/EXP_OUT pass the same CS, SCK, MISO, MOSI, 5 V and GND nets.

The audio connector must also be connected: SPI pass-through alone does not
carry audio signals or the microphone's 3.3 V supply.

| Audio signal | Main ESP GPIO | Main audio header pin | Audio board header pin |
|---|---:|---:|---:|
| SPK_LRC | 38 | 1 | 2 |
| 3.3 V | - | 2 | 1 |
| SPK_CLK | 39 | 3 | 4 |
| 5 V | - | 4 | 3 |
| SPK_DIN | 40 | 5 | 6 |
| MIC_Data | 42 | 6 | 5 |
| GND | - | 7 | 8 |
| MIC_CLK | 41 | 8 | 7 |

Source: [audio board schematic](../../Modules%20Schematics/Schematic_I2S_EXP_Module.pdf)
and [main board schematic, page 1](../../Modules%20Schematics/Schematic_Main_Board.pdf).
See [Expansion Reference](../../Modules%20Schematics/Expansion_Reference.md)
for the separate six-pin pass-through wiring and cascading topology.

## Firmware and protocol status

No standalone MCU firmware is needed on this board. No ESP audio application
or drivers have been implemented in this repository. There is no module UART
protocol or proposed PY32 project for it.

## Open decisions and next work

Agree playback/capture features, sample rates, channel/sample formats, buffering
and simultaneous operation requirements. Check the exact amplifier's gain/
shutdown straps and the microphone's PDM clock limits against their datasheets.
Implement and validate the corresponding ESP32 peripheral/DMA paths and
underflow/overflow behavior. Do not treat PDM microphone data as ordinary
I2S PCM without the appropriate ESP capture/conversion configuration.

## Validation

Hardware pending. Verify audio cable mapping, supply rails, speaker waveform/
playback, microphone capture, clipping/noise, streaming interruptions and
simultaneous SPI expansion traffic. SPI pass-through has no firmware routing
logic of its own.
