/**
 * @file ir_array.c
 * @brief ADC sampling, hysteresis, and packed state for the TCRT5000L array.
 *
 * Physical order S1..S5 maps to PA5, PA6, PA7, PA1, PA0. Three-sensor builds
 * retain S2..S4 and compact their states into bits 0..2. All functions run
 * in the foreground; the UART interrupt never accesses the ADC or reading.
 * High/low refers to sensor voltage, not a calibrated black/white decision.
 */
#include "ir_array.h"

static ADC_HandleTypeDef adc;
static IRArrayReading reading;
static uint32_t last_sample_ms;
/* The first successful scan (also after an ADC error) seeds hysteresis from
 * the centre thresholds rather than retaining untrustworthy previous states. */
static uint8_t initialized_states;
/* Physical sensor order, deliberately different from ADC channel order. */
static const uint32_t channels[5] = {
  ADC_CHANNEL_5, ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_1, ADC_CHANNEL_0
};
static const uint16_t thresholds[5] = {
  IR_THRESHOLD_S1, IR_THRESHOLD_S2, IR_THRESHOLD_S3,
  IR_THRESHOLD_S4, IR_THRESHOLD_S5
};

/** @brief Configure active analog pins and calibrate the ADC after clock setup.
 *  @note Three-sensor builds leave the removed end-sensor pins unconfigured.
 *        Calibration corrects ADC offset; it does not calibrate reflectance.
 */
void IRArray_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_1;
#if IR_SENSOR_COUNT == 5
  gpio.Pin |= GPIO_PIN_5 | GPIO_PIN_0;
#endif
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  adc.Instance = ADC1;
  adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  adc.Init.Resolution = ADC_RESOLUTION_12B;
  adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  adc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  adc.Init.LowPowerAutoWait = DISABLE;
  adc.Init.ContinuousConvMode = DISABLE;
  adc.Init.DiscontinuousConvMode = DISABLE;
  adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  adc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  /* Long sampling time for the 10 kohm phototransistor pull-up networks. */
  adc.Init.SamplingTimeCommon = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_Init(&adc) != HAL_OK ||
      HAL_ADC_Calibration_Start(&adc) != HAL_OK) APP_ErrorHandler();
  last_sample_ms = HAL_GetTick();
}

/** @brief Convert one input and remove it from the hardware scan list.
 *  @param channel HAL ADC channel constant for the physical sensor.
 *  @param value Receives the right-aligned 12-bit sample on success only.
 *  @return HAL_OK on success; failure prevents publishing the entire scan.
 *  @note Polling is bounded to 1 ms per conversion; interrupts remain enabled.
 */
static HAL_StatusTypeDef ReadChannel(uint32_t channel, uint16_t *value)
{
  ADC_ChannelConfTypeDef config = {0};
  HAL_StatusTypeDef status;
  config.Channel = channel;
  config.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&adc, &config) != HAL_OK) return HAL_ERROR;
  status = HAL_ADC_Start(&adc);
  if (status == HAL_OK) status = HAL_ADC_PollForConversion(&adc, 1U);
  if (status == HAL_OK) *value = (uint16_t)HAL_ADC_GetValue(&adc);
  if (HAL_ADC_Stop(&adc) != HAL_OK) return HAL_ERROR;
  /* Remove this channel so the next start converts exactly one input,
   * rather than accumulating channels in the ADC's hardware sequencer. */
  config.Rank = ADC_RANK_NONE;
  if (HAL_ADC_ConfigChannel(&adc, &config) != HAL_OK) return HAL_ERROR;
  return status;
}

/** @brief Sample all active sensors when the foreground scan period expires.
 *  @note This is best-effort scheduling, not a timer-guaranteed 1 kHz stream.
 *        Work in a local reading so consumers never receive half of a scan.
 */
void IRArray_Process(void)
{
  IRArrayReading next = {0};
  uint32_t now = HAL_GetTick();
  uint8_t i;
  if ((uint32_t)(now - last_sample_ms) < IR_SAMPLE_PERIOD_MS) return;
  last_sample_ms = now;
  next.states = reading.states;
  for (i = 0U; i < IR_SENSOR_COUNT; ++i) {
    uint8_t physical = i;
    uint8_t bit = (uint8_t)(1U << i);
#if IR_SENSOR_COUNT == 3
    physical = i + 1U; /* Only the retained physical sensors S2, S3, S4. */
#endif
    if (ReadChannel(channels[physical], &next.raw[i]) != HAL_OK) {
      reading.valid = 0U;
      initialized_states = 0U;
      return;
    }
    /* Use the centre threshold once, then a Schmitt-style hysteresis band.
     * Within the band no branch changes the bit, so the old state is kept. */
    if (!initialized_states) {
      if (next.raw[i] >= thresholds[physical]) next.states |= bit;
      else next.states &= (uint8_t)~bit;
    } else if (next.raw[i] >= thresholds[physical] + IR_HYSTERESIS) {
      next.states |= bit;
    } else if (next.raw[i] <= thresholds[physical] - IR_HYSTERESIS) {
      next.states &= (uint8_t)~bit;
    }
  }
  next.valid = 1U;
  next.updated_ms = HAL_GetTick();
  reading = next; /* Publish only after the entire sensor set succeeds. */
  initialized_states = 1U;
}

/** @brief Return a foreground snapshot, invalidating data older than IR_STALE_MS.
 *  @note Raw values can remain available for inspection after a failed scan;
 *        callers must check valid before interpreting them as current data.
 */
IRArrayReading IRArray_GetReading(void)
{
  IRArrayReading snapshot = reading;
  if ((uint32_t)(HAL_GetTick() - snapshot.updated_ms) > IR_STALE_MS) snapshot.valid = 0U;
  return snapshot;
}

/** @brief Encode the latest classification as exactly one UART payload byte.
 *  @return bit 7: five-sensor flag; bit 6: valid; bit 5: zero; low bits: states.
 *          An invalid scan preserves only the sensor-count flag, so it cannot
 *          be confused with a valid scan in which every sensor reads low.
 */
uint8_t IRArray_GetPacket(void)
{
  IRArrayReading snapshot = IRArray_GetReading();
  uint8_t packet = IR_SENSOR_COUNT == 5 ? IR_PACKET_FIVE_SENSORS : 0U;
  if (snapshot.valid) {
    packet |= IR_PACKET_VALID | (snapshot.states & ((1U << IR_SENSOR_COUNT) - 1U));
  }
  return packet;
}
