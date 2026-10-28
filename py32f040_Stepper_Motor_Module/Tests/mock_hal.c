/* ARM test harness links the real driver and UART parser against mocked IO. */
#include "main.h"
#include "motion.h"
#include "current_monitor.h"
#include <string.h>
void Test_CurrentBlock(void);
static uint32_t auto_adc,adc_ticks;
#include "module_uart.h"
static uint32_t tick, level, tx_size;
GPIO_TypeDef test_gpioa,test_gpiob;
TIM_TypeDef test_tim6;
EXTI_TypeDef test_exti;
uint32_t test_trace[10000],test_trace_count,test_pin_modes[5];
static uint32_t gpio_output;
static uint8_t *rx_destination;
static UART_HandleTypeDef *rx_uart;
uint8_t test_tx[4096];

uint32_t HAL_GetTick(void) { return tick; }
uint32_t HAL_GPIO_ReadPin(void *port, uint32_t pin) { (void)port; (void)pin; return level; }
void HAL_GPIO_Init(void *port, GPIO_InitTypeDef *g) {
 (void)port;if(g->Mode<5)test_pin_modes[g->Mode]|=g->Pin;
}
void HAL_NVIC_SetPriority(uint32_t irq, uint32_t priority, uint32_t sub) { (void)irq; (void)priority; (void)sub; }
void HAL_NVIC_EnableIRQ(uint32_t irq) { (void)irq; }
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *uart) { (void)uart; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size)
{ (void)size; rx_destination = data; rx_uart = uart; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart, uint8_t *data, uint16_t size, uint32_t timeout)
{
  (void)uart; (void)timeout;
  if (tx_size + size > sizeof(test_tx)) return HAL_ERROR;
  for (uint16_t i = 0; i < size; ++i) test_tx[tx_size++] = data[i];
  return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart) { (void)uart; return HAL_OK; }
void HAL_UART_IRQHandler(UART_HandleTypeDef *uart) { (void)uart; }
void APP_ErrorHandler(void) { for (;;) {} }

void Test_Init(uint32_t now, uint32_t raw)
{ tick = now; level = raw; tx_size = 0; test_gpiob.IDR=GPIO_PIN_7;
 MotorIO_InitSafe();Motion_Init();CurrentADC_Init();
 for(uint8_t i=0;i<10;i++) Test_CurrentBlock();
 auto_adc=1;ModuleUART_Init();MotorIO_StartTimer(); }
void Test_Time(uint32_t now) { tick = now; }
void Test_FeedByte(uint32_t byte)
{ *rx_destination = (uint8_t)byte; HAL_UART_RxCpltCallback(rx_uart); }
void Test_ProcessUART(void) { ModuleUART_Process(); }
void Test_Error(void) { HAL_UART_ErrorCallback(rx_uart); }
uint32_t Test_TxSize(void) { return tx_size; }
void Test_ClearTx(void) { tx_size = 0; }

void TIM6_LPTIM1_IRQHandler(void);
void EXTI4_15_IRQHandler(void);
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *h)
{ return h->Init.Prescaler==71 && h->Init.Period==49?HAL_OK:HAL_ERROR; }
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *h) { (void)h;return HAL_OK; }
/** Apply GPIO's write-only set/reset semantics, recording coil changes. */
static void Sync(void)
{
 uint32_t v=test_gpiob.BSRR,old=gpio_output;
 gpio_output=(gpio_output & ~(v>>16)) | (v&65535U);test_gpiob.BSRR=0;
 if((old&0x33U)!=(gpio_output&0x33U) && test_trace_count<10000U)
  test_trace[test_trace_count++]=gpio_output&0x73U;
}
void Test_Ticks(uint32_t count)
{
 Sync();
 while(count--) {
  test_tim6.SR=TIM_SR_UIF;TIM6_LPTIM1_IRQHandler();Sync();
  if(auto_adc && ++adc_ticks>=160U) { adc_ticks=0;Test_CurrentBlock(); }
 }
}
uint32_t Test_Output(void) { Sync();return gpio_output; }
uint32_t Test_Field(uint32_t index)
{
 MotionStatus s=Motion_GetStatus();
 switch(index) {
  case 0:return s.state;case 1:return (uint32_t)s.position;case 2:return s.remaining;
  case 3:return s.velocity_q16;case 4:return s.speed;case 5:return s.acceleration;
  case 6:return s.fault;case 7:return s.phase;default:return 0xffffffffU;
 }
}
void Test_FaultPin(uint32_t value) { test_gpiob.IDR=value?0:GPIO_PIN_7; }
void Test_FaultIRQ(void) { test_exti.PR=GPIO_PIN_7;EXTI4_15_IRQHandler();test_exti.PR=0;Sync(); }

/* ADC HAL model captures the production scan, then clocks complete stable DMA
 * halves. Values represent analog samples supplied by Python, not real current. */
DMA_Channel_TypeDef test_dma_channel;
DMA_TypeDef test_dma;
uint16_t test_samples[CURRENT_WORDS];
uint32_t test_opa_enabled,test_adc_channels[3],test_adc_times[3];
static ADC_HandleTypeDef *adc_handle;
static uint16_t *dma_buffer;
static uint32_t dma_active,adc_interrupt,half,inject_cross,counter_reads;
void HAL_Delay(uint32_t ms) { tick+=ms; }
HAL_StatusTypeDef HAL_OPA_Init(OPA_HandleTypeDef *h) { (void)h;return HAL_OK; }
HAL_StatusTypeDef HAL_OPA_Start(OPA_HandleTypeDef *h) { test_opa_enabled|=1U<<h->Init.Part;return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *h) { return h->Init.NbrOfConversion==3 && h->Init.ContinuousConvMode==ENABLE?HAL_OK:HAL_ERROR; }
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h) { (void)h;return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_ConfigVrefBuf(ADC_HandleTypeDef *h,uint32_t v) { (void)h;return v==ADC_VREFBUF_VCCA?HAL_OK:HAL_ERROR; }
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *h,ADC_ChannelConfTypeDef *c)
{ (void)h;test_adc_channels[c->Rank-1]=c->Channel;test_adc_times[c->Rank-1]=c->SamplingTime;return HAL_OK; }
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *h) { (void)h;return HAL_OK; }
void HAL_DMA_ChannelMap(DMA_HandleTypeDef *h,uint32_t map) { (void)h;(void)map; }
HAL_StatusTypeDef HAL_ADC_Start_DMA(ADC_HandleTypeDef *h,uint32_t *data,uint32_t n)
{
 if(n!=2U*CURRENT_WORDS) return HAL_ERROR;
 adc_handle=h;dma_buffer=(uint16_t *)data;dma_active=1;half=0;adc_ticks=0;
 for(uint16_t i=0;i<CURRENT_SCANS;i++) { test_samples[3*i]=82;test_samples[3*i+1]=82;test_samples[3*i+2]=983; }
 return HAL_OK;
}
void Test_StopDMA(void) { dma_active=0; }
void Test_ADCInterrupt(uint32_t enabled) { adc_interrupt=enabled; }
uint32_t Test_DMACounter(void)
{
 if(inject_cross && ++counter_reads==2U) {
  test_dma.ISR|=half==0?DMA_FLAG_TC1:DMA_FLAG_HT1;
  return half==0?2U*CURRENT_WORDS:CURRENT_WORDS;
 }
 return test_dma_channel.CNDTR;
}
void HAL_DMA_IRQHandler(DMA_HandleTypeDef *h)
{
 (void)h;uint32_t flags=test_dma.ISR;test_dma.ISR=0;
 if(flags&DMA_FLAG_HT1) HAL_ADC_ConvHalfCpltCallback(adc_handle);
 if(flags&DMA_FLAG_TC1) HAL_ADC_ConvCpltCallback(adc_handle);
}
void HAL_ADC_IRQHandler(ADC_HandleTypeDef *h) { if(adc_interrupt) HAL_ADC_ErrorCallback(h); }
void Test_CurrentBlock(void)
{
 if(!dma_active)return;
 memcpy(dma_buffer+half*CURRENT_WORDS,test_samples,sizeof test_samples);
 test_dma_channel.CNDTR=half==0?CURRENT_WORDS:2U*CURRENT_WORDS;
 test_dma.ISR=half==0?DMA_FLAG_HT1:DMA_FLAG_TC1;counter_reads=0;
 CurrentADC_DMAIRQ();half^=1U;
}
void Test_AutoADC(uint32_t enabled) { auto_adc=enabled; }
void Test_ResetSense(void) { CurrentADC_Init(); }
void Test_CrossBoundary(uint32_t enabled) { inject_cross=enabled; }
void Test_DMAFlags(uint32_t flags) { test_dma.ISR=flags;CurrentADC_DMAIRQ(); }
void Test_ADCError(void) { CurrentADC_ADCIRQ(); }
uint32_t Test_CurrentField(uint32_t field)
{
 CurrentReading r=CurrentMonitor_Get();
 switch(field) {
  case 0:return r.ready;case 1:return r.fault;case 2:return r.mean_ma[0];case 3:return r.mean_ma[1];
  case 4:return r.peak_ma[0];case 5:return r.peak_ma[1];case 6:return r.vcca_mv;
  case 7:return r.limit_ma;case 8:return r.sequence;case 9:return dma_active;default:return 0xffffffffU;
 }
}
