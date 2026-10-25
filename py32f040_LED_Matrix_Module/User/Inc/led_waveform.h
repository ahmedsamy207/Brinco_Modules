/** @file led_waveform.h @brief Pure RGB-to-GRB PWM encoding, independent of HAL. */
#ifndef BRINCO_LED_WAVEFORM_H
#define BRINCO_LED_WAVEFORM_H
#include <stdint.h>
#include "matrix_config.h"
/** @brief Encode 48 RGB pixels, MSB first on the wire in GRB order.
 *  @param rgb Exactly MATRIX_RGB_BYTES bytes in chain-index RGB order.
 *  @param brightness Linear scale, 0=black, 255=unchanged; source is preserved.
 *  @param output At least MATRIX_WAVEFORM_WORDS halfwords for DMA compare values.
 */
void LEDWaveform_Encode(const uint8_t *rgb, uint8_t brightness, uint16_t *output);
#endif
