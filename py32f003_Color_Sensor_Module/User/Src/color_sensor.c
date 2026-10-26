/** @file color_sensor.c
 *  @brief Active-force RGBW state machine with atomic publication and retry.
 *  One triggered exposure completes before its four registers are read, so
 *  channels cannot cross continuous-conversion boundaries. No next trigger
 *  is sent until all four words succeed. Reserved bits are always zero.
 */
#include "main.h"
#include "color_sensor.h"
#include "color_bus.h"
#include "color_config.h"
typedef enum { SETTLE, SHUTDOWN, ENABLE_SENSOR, VERIFY, TRIGGER, WAIT, READ_CHANNEL, RETRY } Phase;
static Phase phase;
static ColorReading reading;
static uint16_t pending[4];
static uint8_t it_code, channel;
static uint32_t phase_ms;
static const uint16_t integration_times[] = {40U,80U,160U,320U,640U,1280U};

/** @brief Engineering margin over nominal IT; no hardware data-ready bit exists.
 *  @note Validate conversion timing on boards: 1.25*IT + 10 ms is a margin,
 *        not a manufacturer-guaranteed maximum conversion time.
 */
static uint32_t ExposureWait(void)
{ return reading.integration_ms + reading.integration_ms/4U + 10U; }

/** @brief Invalidate immediately so readings never retain mismatched settings. */
static void Restart(void)
{
  reading.valid = 0U;
  phase = SETTLE;
  phase_ms = HAL_GetTick();
}

static void Failed(void)
{
  reading.valid = 0U;
  reading.error = 1U;
  phase = RETRY;
  phase_ms = HAL_GetTick();
}

void ColorSensor_Init(void)
{
  reading = (ColorReading){0};
  reading.integration_ms = COLOR_DEFAULT_IT_MS;
  reading.led = COLOR_DEFAULT_LED;
  for (it_code = 0U; it_code < 6U; ++it_code) {
    if (integration_times[it_code] == reading.integration_ms) break;
  }
  if (it_code == 6U || COLOR_DEFAULT_LED > 1U) APP_ErrorHandler();
  ColorBus_Init();
  Restart();
}

uint8_t ColorSensor_SetIntegration(uint16_t milliseconds)
{
  for (uint8_t i = 0U; i < 6U; ++i) {
    if (integration_times[i] == milliseconds) {
      if (reading.integration_ms != milliseconds) {
        reading.integration_ms = milliseconds;
        it_code = i;
        Restart();
      }
      return 1U;
    }
  }
  return 0U;
}

uint8_t ColorSensor_SetLED(uint8_t on)
{
  if (on > 1U) return 0U;
  if (reading.led != on) {
    reading.led = on;
    ColorBus_SetLED(on);
    Restart();
  }
  return 1U;
}

void ColorSensor_Process(void)
{
  uint32_t now = HAL_GetTick();
  uint16_t conf = (uint16_t)(it_code << 4U) | 0x02U; /* AF=1, SD/TRIG=0. */
  uint16_t value;
  switch (phase) {
    case SETTLE:
      if ((uint32_t)(now-phase_ms) < COLOR_SETTLE_MS) return;
      if (!ColorBus_Reset()) { Failed(); return; }
      phase = SHUTDOWN;
      return;
    case SHUTDOWN:
      if (!ColorBus_WriteWord(COLOR_CONF_REG, conf | 0x01U)) { Failed(); return; }
      phase = ENABLE_SENSOR;
      return;
    case ENABLE_SENSOR:
      if (!ColorBus_WriteWord(COLOR_CONF_REG, conf)) { Failed(); return; }
      phase = VERIFY;
      return;
    case VERIFY:
      if (!ColorBus_ReadWord(COLOR_CONF_REG, &value) || value != conf) { Failed(); return; }
      phase = TRIGGER;
      return;
    case TRIGGER:
      if (!ColorBus_WriteWord(COLOR_CONF_REG, conf | 0x04U)) { Failed(); return; }
      phase_ms = HAL_GetTick();
      phase = WAIT;
      return;
    case WAIT:
      if ((uint32_t)(now-phase_ms) < ExposureWait()) return;
      channel = 0U;
      phase = READ_CHANNEL;
      return;
    case READ_CHANNEL:
      if (!ColorBus_ReadWord(COLOR_RED_REG+channel, &pending[channel])) { Failed(); return; }
      if (++channel == 4U) {
        uint8_t saturation = 0U;
        for (uint8_t i = 0U; i < 4U; ++i) {
          reading.channels[i] = pending[i];
          if (pending[i] >= COLOR_SATURATION_COUNT) saturation |= (uint8_t)(1U << i);
        }
        reading.saturation = saturation;
        reading.updated_ms = HAL_GetTick();
        ++reading.sequence;
        reading.valid = 1U;
        reading.error = 0U;
        phase = TRIGGER;
      }
      return;
    case RETRY:
      if ((uint32_t)(now-phase_ms) >= COLOR_RETRY_MS) Restart();
      return;
    default: Failed(); return;
  }
}

ColorReading ColorSensor_GetReading(void)
{
  ColorReading snapshot = reading;
  /* Includes the margin and normal transfer/service overhead; delayed
   * foreground service must not leave an old sample looking fresh forever. */
  if ((uint32_t)(HAL_GetTick()-snapshot.updated_ms) > ExposureWait()+100U) snapshot.valid = 0U;
  return snapshot;
}
