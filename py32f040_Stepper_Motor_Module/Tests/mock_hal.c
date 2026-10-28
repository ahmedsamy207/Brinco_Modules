/* ARM test harness links the real driver and UART parser against mocked IO. */
#include "main.h"
#include "motion.h"
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
 MotorIO_InitSafe();Motion_Init();ModuleUART_Init();MotorIO_StartTimer(); }
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
