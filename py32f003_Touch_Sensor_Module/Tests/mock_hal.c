/* ARM test harness links the real driver and UART parser against mocked IO. */
#include "main.h"
#include "touch_sensor.h"
#include "module_uart.h"
static uint32_t tick, level, tx_size;
static uint8_t *rx_destination;
static UART_HandleTypeDef *rx_uart;
uint8_t test_tx[4096];

uint32_t HAL_GetTick(void) { return tick; }
uint32_t HAL_GPIO_ReadPin(void *port, uint32_t pin) { (void)port; (void)pin; return level; }
void HAL_GPIO_Init(void *port, GPIO_InitTypeDef *gpio) { (void)port; (void)gpio; }
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
{ tick = now; level = raw; tx_size = 0; TouchSensor_Init(); ModuleUART_Init(); }
void Test_Step(uint32_t now, uint32_t raw)
{ tick = now; level = raw; TouchSensor_Process(); ModuleUART_Process(); }
void Test_Time(uint32_t now) { tick = now; }
uint32_t Test_State(void)
{
  TouchReading value = TouchSensor_GetReading();
  return value.raw_level | (value.touched << 1U) | (value.valid << 2U);
}
void Test_FeedByte(uint32_t byte)
{ *rx_destination = (uint8_t)byte; HAL_UART_RxCpltCallback(rx_uart); }
void Test_ProcessUART(void) { ModuleUART_Process(); }
void Test_Error(void) { HAL_UART_ErrorCallback(rx_uart); }
uint32_t Test_TxSize(void) { return tx_size; }
void Test_ClearTx(void) { tx_size = 0; }
