/** @file sound_meter.c
 *  @brief Relative peak-to-peak level; no calibrated SPL or audio stream.
 *  The DMA ISR owns publication. Foreground copies under a short IRQ mask.
 *  Min/max removes DC offset from amplitude but does not remove electrical noise.
 */
#include "main.h"
#include "sound_meter.h"
static volatile SoundReading reading;
static uint32_t boot_ms;
static uint8_t settled;

void SoundMeter_Init(void)
{
  SoundReading empty = {0};
  reading = empty;
  boot_ms = HAL_GetTick();
  settled = 0U;
}

void SoundMeter_Fault(void) { reading.fault = 1U; reading.valid = 0U; }

void SoundMeter_Publish(const uint16_t *samples, uint8_t raw_d0, uint8_t active_seen)
{
  SoundReading next = {0};
  uint32_t sum = 0U, now = HAL_GetTick();
  if (reading.fault) return;
  next.minimum = SOUND_ADC_MAX;
  for (uint32_t i = 0; i < SOUND_WINDOW_SAMPLES; ++i) {
    uint16_t value = samples[i];
    if (value > SOUND_ADC_MAX) { SoundMeter_Fault(); return; }
    if (value < next.minimum) next.minimum = value;
    if (value > next.maximum) next.maximum = value;
    if (value <= SOUND_CLIP_MARGIN || value >= SOUND_ADC_MAX-SOUND_CLIP_MARGIN) next.clipped = 1U;
    sum += value; /* At most 80 * 4095; safely fits uint32_t. */
  }
  next.last = samples[SOUND_WINDOW_SAMPLES-1U];
  next.mean = (uint16_t)((sum + SOUND_WINDOW_SAMPLES/2U) / SOUND_WINDOW_SAMPLES);
  next.peak_to_peak = next.maximum-next.minimum;
  next.level = (uint16_t)(((uint32_t)next.peak_to_peak*1000U + SOUND_ADC_MAX/2U) / SOUND_ADC_MAX);
  next.raw_d0 = raw_d0 != 0U;
  next.detected = active_seen != 0U;
  next.updated_ms = now;
  next.sequence = reading.sequence + 1U;
  /* Latch completion so a later 32-bit tick rollover cannot restart the guard. */
  if ((uint32_t)(now-boot_ms) >= SOUND_STARTUP_MS+SOUND_WINDOW_MS) settled = 1U;
  next.valid = settled;
  reading = next;
}

SoundReading SoundMeter_GetReading(void)
{
  uint32_t mask = __get_PRIMASK();
  SoundReading snapshot;
  __disable_irq();
  snapshot = reading;
  __set_PRIMASK(mask);
  if ((uint32_t)(HAL_GetTick()-snapshot.updated_ms) > SOUND_STALE_MS) snapshot.valid = 0U;
  return snapshot;
}
