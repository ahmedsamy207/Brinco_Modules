/** @file color_bus.h @brief Foreground-only bounded sensor bus and lamp access. */
#ifndef BRINCO_COLOR_BUS_H
#define BRINCO_COLOR_BUS_H
#include <stdint.h>
/** @brief Configure PA2/PA3 AF12 open-drain, no pulls, and PA6 open-drain. */
void ColorBus_Init(void);
/** @brief Reset/reinitialize I2C1, not physical nine-clock bus recovery. */
uint8_t ColorBus_Reset(void);
/** @brief Register words are little-endian; return 1 only on full success. */
uint8_t ColorBus_WriteWord(uint8_t reg, uint16_t value);
uint8_t ColorBus_ReadWord(uint8_t reg, uint16_t *value);
/** @brief On releases PA6 to 3.3 V; off sinks it. Never drive a 5 V HIGH. */
void ColorBus_SetLED(uint8_t on);
#endif
