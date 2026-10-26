/** @file sound_meter.h @brief Window statistics shared by acquisition and UART. */
#ifndef BRINCO_SOUND_METER_H
#define BRINCO_SOUND_METER_H
#include <stdint.h>
#include "sound_config.h"
typedef struct {
  uint16_t last, minimum, maximum, mean, peak_to_peak; /* 12-bit ADC counts. */
  uint16_t level; /* Rounded peak-to-peak / 4095 * 1000, not dB SPL. */
  uint8_t raw_d0; /* GPIO voltage at window end, independent of polarity. */
  uint8_t detected; /* Active edge/level observed during this window. */
  uint8_t clipped; /* At least one sample within five counts of an ADC rail. */
  uint8_t valid, fault; /* Settled/fresh data; fault is sticky until reset. */
  uint32_t updated_ms, sequence; /* Completion tick and wrapping window counter. */
} SoundReading;
/** @brief Foreground initialization before enabling acquisition IRQs. */
void SoundMeter_Init(void);
/** @brief DMA IRQ: publish exactly one 80-sample window, bounded integer work.
 *  @param active_seen Nonzero if D0 was active at any captured edge/window end.
 *  @note Samples must be 12-bit; invalid values latch an acquisition fault.
 */
void SoundMeter_Publish(const uint16_t *samples, uint8_t raw_d0, uint8_t active_seen);
/** @brief IRQ-safe sticky acquisition failure; reset MCU to recover. */
void SoundMeter_Fault(void);
/** @brief Foreground atomic snapshot; clears valid if older than 100 ms. */
SoundReading SoundMeter_GetReading(void);
#endif
