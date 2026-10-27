/** @file tft_io.h @brief Foreground-only ownership of LCD and shared touch GPIOs. */
#ifndef BRINCO_TFT_IO_H
#define BRINCO_TFT_IO_H
#include <stdint.h>
typedef struct { uint16_t x, y, z; uint8_t stable; } TFTRawTouch;
/** @brief Initialize MAR2406 ILI9341, 240x320 portrait RGB565, and touch ADC.
 *  @note Startup reset/sleep delays are blocking, before SPI is initialized.
 */
void TFTIO_Init(void);
void TFTIO_SetRotation(uint8_t rotation);
/** @brief Write one in-bounds row segment; pixels=NULL repeats colour.
 *  @note Every call closes LCD CS. No framebuffer or LCD readback is used.
 */
void TFTIO_Row(uint16_t x,uint16_t y,uint16_t count,uint16_t color,const uint16_t *pixels);
/** @brief Temporarily multiplex four panel electrodes, then always restore LCD.
 *  @return 1 on successful ADC acquisition; no contact is not an ADC failure.
 */
uint8_t TFTIO_ReadTouch(TFTRawTouch *raw);
#endif
