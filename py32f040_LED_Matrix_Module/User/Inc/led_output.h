/** @file led_output.h @brief Nonblocking TIM15_CH2 / DMA output on PB15 AF1. */
#ifndef BRINCO_LED_OUTPUT_H
#define BRINCO_LED_OUTPUT_H
#include <stdint.h>
void LEDOutput_Init(void);
/** @brief Start one complete RGB frame; return 1 only if DMA started.
 *  Copies/encodes input before returning. Busy/fault returns 0 without restart.
 */
uint8_t LEDOutput_Start(const uint8_t *rgb, uint8_t brightness);
uint8_t LEDOutput_IsBusy(void);
uint8_t LEDOutput_HasError(void);
/** @brief Foreground DMA watchdog. A DMA failure latches until MCU reset. */
void LEDOutput_Process(void);
void LEDOutput_DMAIRQ(void);
#endif
