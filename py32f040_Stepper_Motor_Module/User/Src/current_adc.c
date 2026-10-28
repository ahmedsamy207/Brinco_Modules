/** @file current_adc.c
 * OPA1 PA9(+),PA10(-),PA8(out)->PA2/ADC2.
 * OPA2 PA7(+),PA6(-),PA5(out)->PA4/ADC4. All pins stay analog; only the OPA
 * output switches drive PA8/5, never GPIO outputs on their tied ADC pins.
 * ADC1 PCLK/8=9MHz, continuous scan A,B,VREFINT into circular DMA channel1.
 * Long, unequal sampling times make the scan nominally asynchronous to the
 * driver's50kHz chopping. Aliasing/real waveform coverage still needs bench QA.
 */
#include "main.h"
#include "motion.h"
#include "current_monitor.h"
static OPA_HandleTypeDef opa[2];
static ADC_HandleTypeDef adc;
static DMA_HandleTypeDef dma;
static volatile uint16_t buffer[2U*CURRENT_WORDS] __attribute__((aligned(4)));
static uint8_t failed;
static void StopFailed(void)
{
 __HAL_DMA_DISABLE(&dma);
 __HAL_DMA_DISABLE_IT(&dma,DMA_IT_HT|DMA_IT_TC|DMA_IT_TE);
 __HAL_ADC_DISABLE_IT(&adc,ADC_IT_OVR);
 __HAL_ADC_DISABLE(&adc);failed=1;CurrentMonitor_Fail();
}
void CurrentADC_Init(void)
{
 ADC_ChannelConfTypeDef channel={0};
 __HAL_RCC_OPA_CLK_ENABLE();__HAL_RCC_ADC_CLK_ENABLE();__HAL_RCC_DMA_CLK_ENABLE();
 __HAL_RCC_SYSCFG_CLK_ENABLE();CurrentMonitor_Init();failed=0;
 /* MotorIO_InitSafe already placed all shared analog nets in analog mode. */
 for(uint8_t i=0;i<2;i++) {
  opa[i].Instance=OPA;opa[i].Init.Part=i==0?OPA1:OPA2;
  if(HAL_OPA_Init(&opa[i])!=HAL_OK || HAL_OPA_Start(&opa[i])!=HAL_OK) APP_ErrorHandler();
 }
 __HAL_RCC_ADC_CONFIG(RCC_ADCCLKSOURCE_PCLK_DIV8);
 adc.Instance=ADC1;adc.Init.Resolution=ADC_RESOLUTION_12B;adc.Init.DataAlign=ADC_DATAALIGN_RIGHT;
 adc.Init.ScanConvMode=ADC_SCAN_ENABLE;adc.Init.ContinuousConvMode=ENABLE;
 adc.Init.NbrOfConversion=3;adc.Init.DiscontinuousConvMode=DISABLE;adc.Init.ExternalTrigConv=ADC_SOFTWARE_START;
 if(HAL_ADC_Init(&adc)!=HAL_OK || HAL_ADCEx_Calibration_Start(&adc)!=HAL_OK) APP_ErrorHandler();
 if(HAL_ADC_ConfigVrefBuf(&adc,ADC_VREFBUF_VCCA)!=HAL_OK) APP_ErrorHandler();
 channel.SamplingTime=ADC_SAMPLETIME_134CYCLES_5;
 channel.Channel=ADC_CHANNEL_2;channel.Rank=ADC_REGULAR_RANK_1;
 if(HAL_ADC_ConfigChannel(&adc,&channel)!=HAL_OK) APP_ErrorHandler();
 channel.Channel=ADC_CHANNEL_4;channel.Rank=ADC_REGULAR_RANK_2;
 if(HAL_ADC_ConfigChannel(&adc,&channel)!=HAL_OK) APP_ErrorHandler();
 channel.Channel=ADC_CHANNEL_VREFINT;channel.Rank=ADC_REGULAR_RANK_3;
 channel.SamplingTime=ADC_SAMPLETIME_239CYCLES_5;
 if(HAL_ADC_ConfigChannel(&adc,&channel)!=HAL_OK) APP_ErrorHandler();
 HAL_Delay(1); /* >OPA20us and VREFINT15us startup; bridges still disabled. */
 dma.Instance=DMA1_Channel1;dma.Init.Direction=DMA_PERIPH_TO_MEMORY;
 dma.Init.PeriphInc=DMA_PINC_DISABLE;dma.Init.MemInc=DMA_MINC_ENABLE;
 dma.Init.PeriphDataAlignment=DMA_PDATAALIGN_HALFWORD;dma.Init.MemDataAlignment=DMA_MDATAALIGN_HALFWORD;
 dma.Init.Mode=DMA_CIRCULAR;dma.Init.Priority=DMA_PRIORITY_HIGH;
 if(HAL_DMA_Init(&dma)!=HAL_OK) APP_ErrorHandler();
 HAL_DMA_ChannelMap(&dma,DMA_CHANNEL_MAP_ADC1);__HAL_LINKDMA(&adc,DMA_Handle,dma);
 /* Motor tick and UART preempt block analysis; analysis does not mask them. */
 HAL_NVIC_SetPriority(DMA1_Channel1_IRQn,3,0);HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
 HAL_NVIC_SetPriority(ADC_COMP_IRQn,2,0);HAL_NVIC_EnableIRQ(ADC_COMP_IRQn);
 __HAL_ADC_ENABLE_IT(&adc,ADC_IT_OVR); /* Vendor DMA start does not enable overrun IRQ. */
 if(HAL_ADC_Start_DMA(&adc,(uint32_t *)buffer,2U*CURRENT_WORDS)!=HAL_OK) APP_ErrorHandler();
}
/** Reject boundaries crossed while analyzing. UART may preempt but cannot see
 * an unvalidated half: statistics live on the stack until the final check. */
static void Publish(uint8_t half)
{
 CurrentBlock block;
 uint32_t n=__HAL_DMA_GET_COUNTER(&dma);
 if(failed || (half==0?(n==0 || n>CURRENT_WORDS):(n<=CURRENT_WORDS || n>2U*CURRENT_WORDS))) {
  StopFailed();return;
 }
 if(!CurrentMonitor_Analyze(buffer+half*CURRENT_WORDS,&block)) { StopFailed();return; }
 uint32_t mask=__get_PRIMASK();__disable_irq();
 n=__HAL_DMA_GET_COUNTER(&dma);
 uint32_t crossed=half==0?DMA_FLAG_TC1:DMA_FLAG_HT1;
 if((DMA1->ISR&(crossed|DMA_FLAG_TE1)) ||
    (half==0?(n==0 || n>CURRENT_WORDS):(n<=CURRENT_WORDS || n>2U*CURRENT_WORDS))) StopFailed();
 else CurrentMonitor_Commit(&block);
 __set_PRIMASK(mask);
}
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h) { if(h==&adc) Publish(0); }
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h) { if(h==&adc) Publish(1); }
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *h) { if(h==&adc) StopFailed(); }
void CurrentADC_DMAIRQ(void)
{
 uint32_t flags=DMA1->ISR;
 if((flags&DMA_FLAG_TE1) || (flags&(DMA_FLAG_HT1|DMA_FLAG_TC1))==(DMA_FLAG_HT1|DMA_FLAG_TC1)) {
  StopFailed();__HAL_DMA_CLEAR_FLAG(&dma,DMA_FLAG_GL1);return;
 }
 HAL_DMA_IRQHandler(&dma);
}
void CurrentADC_ADCIRQ(void) { HAL_ADC_IRQHandler(&adc); }
