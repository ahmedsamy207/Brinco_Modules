/** @file led_waveform.c
 *  @brief Timing values satisfy both table and narrower note on datasheet p7.
 *  Zero high: 22/72 MHz. One high: 68/72 MHz. Reset is a literal zero duty,
 *  not the short high pulse representing a data bit whose numeric value is 0.
 */
#include "led_waveform.h"

void LEDWaveform_Encode(const uint8_t *rgb, uint8_t brightness, uint16_t *output)
{
  uint32_t position = 0U;
  static const uint8_t order[3] = {1U, 0U, 2U}; /* RGB storage -> GRB wire. */
  for (uint32_t i = 0U; i < MATRIX_RESET_CELLS; ++i) output[position++] = 0U;
  for (uint32_t pixel = 0U; pixel < MATRIX_LEDS; ++pixel) {
    for (uint32_t channel = 0U; channel < 3U; ++channel) {
      uint8_t value = (uint8_t)(((uint32_t)rgb[pixel * 3U + order[channel]] * brightness + 127U) / 255U);
      for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
        output[position++] = (value & mask) ? MATRIX_ONE_TICKS : MATRIX_ZERO_TICKS;
      }
    }
  }
  for (uint32_t i = 0U; i < MATRIX_RESET_CELLS; ++i) output[position++] = 0U;
}
