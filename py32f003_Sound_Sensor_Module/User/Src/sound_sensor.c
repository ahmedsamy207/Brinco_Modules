/** @file sound_sensor.c
 *  @brief PA2/ADC2 sampled by TIM3 TRGO into circular DMA; PA3/D0 edge latch.
 *  32 MHz timer / 32 / 250 = 4 kHz. ADC /4 = 8 MHz, long sample time for
 *  microphone bias impedance. Each DMA half contains one 20 ms window.
 *  No UART work or HAL polling occurs in acquisition callbacks.
 */
#include "main.h"
#include "sound_sensor.h"
#include "sound_meter.h"
static ADC_HandleTypeDef adc;
static DMA_HandleTypeDef dma;
static TIM_HandleTypeDef timer;
static uint16_t samples[2U*SOUND_WINDOW_SAMPLES];
static volatile uint8_t active_seen;
static uint32_t started_ms;

static uint8_t IsActive(uint8_t raw)
{
  return SOUND_DETECT_ACTIVE_LOW ? !raw : raw;
}

/** @brief Consume the edge latch atomically; edges afterward belong to next window. */
static void Publish(const uint16_t *half)
{
  uint32_t mask = __get_PRIMASK();
  uint8_t raw, seen;
  __disable_irq();
  raw = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET;
  seen = active_seen | IsActive(raw);
  /* Carry a level active at this boundary into the next window as well. */
  active_seen = IsActive(raw);
  __set_PRIMASK(mask);
  SoundMeter_Publish(half, raw, seen);
}

void SoundSensor_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  ADC_ChannelConfTypeDef channel = {0};
  TIM_MasterConfigTypeDef master = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_ADC_CLK_ENABLE();
  __HAL_RCC_DMA_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  if (HAL_RCC_GetPCLK1Freq() != 32000000U) APP_ErrorHandler();
  SoundMeter_Init();
  started_ms = HAL_GetTick();
  gpio.Pin = GPIO_PIN_2;
  gpio.Mode = GPIO_MODE_ANALOG;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = GPIO_PIN_3;
  gpio.Mode = SOUND_DETECT_ACTIVE_LOW ? GPIO_MODE_IT_FALLING : GPIO_MODE_IT_RISING;
  gpio.Pull = GPIO_NOPULL; /* Schematic R4 is the external D0 pull-up. */
  HAL_GPIO_Init(GPIOA, &gpio);
  active_seen = IsActive(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_SET);
  __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
  HAL_NVIC_SetPriority(EXTI2_3_IRQn, 0U, 0U);
  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);

  dma.Instance = DMA1_Channel1;
  dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
  dma.Init.PeriphInc = DMA_PINC_DISABLE;
  dma.Init.MemInc = DMA_MINC_ENABLE;
  dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  dma.Init.Mode = DMA_CIRCULAR;
  dma.Init.Priority = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&dma) != HAL_OK) APP_ErrorHandler();
  HAL_DMA_ChannelMap(&dma, DMA_CHANNEL_MAP_ADC);
  __HAL_LINKDMA(&adc, DMA_Handle, dma);
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  HAL_NVIC_SetPriority(ADC_COMP_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(ADC_COMP_IRQn);
  adc.Instance = ADC1;
  adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  adc.Init.Resolution = ADC_RESOLUTION_12B;
  adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  adc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  adc.Init.LowPowerAutoWait = DISABLE;
  adc.Init.ContinuousConvMode = DISABLE;
  adc.Init.DiscontinuousConvMode = DISABLE;
  adc.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  adc.Init.DMAContinuousRequests = ENABLE;
  adc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  adc.Init.SamplingTimeCommon = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_Init(&adc) != HAL_OK || HAL_ADC_Calibration_Start(&adc) != HAL_OK) APP_ErrorHandler();
  channel.Channel = ADC_CHANNEL_2;
  channel.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&adc, &channel) != HAL_OK) APP_ErrorHandler();

  timer.Instance = TIM3;
  timer.Init.Prescaler = 31U;
  timer.Init.Period = 249U;
  timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&timer) != HAL_OK) APP_ErrorHandler();
  master.MasterOutputTrigger = TIM_TRGO_UPDATE;
  master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&timer, &master) != HAL_OK) APP_ErrorHandler();
  if (HAL_ADC_Start_DMA(&adc, (uint32_t *)samples, 2U*SOUND_WINDOW_SAMPLES) != HAL_OK ||
      HAL_TIM_Base_Start(&timer) != HAL_OK) APP_ErrorHandler();
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *handle)
{ if (handle == &adc) Publish(samples); }
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *handle)
{ if (handle == &adc) Publish(samples+SOUND_WINDOW_SAMPLES); }
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *handle)
{
  if (handle != &adc) return;
  /* Stop new triggers immediately; defer any HAL cleanup to reset. */
  __HAL_TIM_DISABLE(&timer);
  SoundMeter_Fault();
}
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
  /* Only the configured active edge triggers EXTI. Latch even if the pulse
   * has ended before this ISR reads GPIO; pending edges can still coalesce. */
  if (pin == GPIO_PIN_3) active_seen = 1U;
}
void SoundSensor_DMAIRQ(void)
{
  uint32_t flags = DMA1->ISR;
  /* If both boundaries are pending, DMA may already be overwriting the half
   * we would read. Reject the acquisition rather than publish mixed windows.
   * Prioritize a transfer error too; the vendor handler checks HT before TE. */
  if ((flags & DMA_FLAG_TE1) != 0U ||
      (flags & (DMA_FLAG_HT1 | DMA_FLAG_TC1)) == (DMA_FLAG_HT1 | DMA_FLAG_TC1)) {
    __HAL_TIM_DISABLE(&timer);
    __HAL_DMA_DISABLE(&dma);
    __HAL_DMA_DISABLE_IT(&dma, DMA_IT_HT | DMA_IT_TC | DMA_IT_TE);
    __HAL_DMA_CLEAR_FLAG(&dma, DMA_FLAG_GL1);
    SoundMeter_Fault();
    return;
  }
  HAL_DMA_IRQHandler(&dma);
}
void SoundSensor_ADCIRQ(void) { HAL_ADC_IRQHandler(&adc); }
void SoundSensor_DetectIRQ(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3); }
void SoundSensor_Process(void)
{
  uint32_t mask = __get_PRIMASK();
  /* Recheck freshness with publication excluded, avoiding a false timeout
   * if a new DMA result arrives between taking the snapshot and faulting. */
  __disable_irq();
  SoundReading value = SoundMeter_GetReading();
  uint32_t now = HAL_GetTick();
  if (!value.fault && (uint32_t)(now-started_ms) > SOUND_STARTUP_MS+SOUND_STALE_MS &&
      (uint32_t)(now-value.updated_ms) > SOUND_STALE_MS) {
    __HAL_TIM_DISABLE(&timer);
    SoundMeter_Fault();
  }
  __set_PRIMASK(mask);
}
