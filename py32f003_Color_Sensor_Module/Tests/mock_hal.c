/* ARM test harness links the real driver and UART parser against mocked IO. */
#include "main.h"
#include "color_sensor.h"
#include "color_bus.h"
#include "module_uart.h"
static uint32_t tick, level, tx_size;
static uint8_t *rx_destination;
static UART_HandleTypeDef *rx_uart;
uint8_t test_tx[4096];

uint32_t HAL_GetTick(void) { return tick; }
uint32_t HAL_GPIO_ReadPin(void *port, uint32_t pin) { (void)port; (void)pin; return level; }
static uint32_t contract_errors;
void HAL_GPIO_Init(void *port, GPIO_InitTypeDef *gpio)
{
  (void)port;
  if (gpio->Pin==(GPIO_PIN_2|GPIO_PIN_3) &&
      (gpio->Mode!=GPIO_MODE_AF_OD || gpio->Pull!=GPIO_NOPULL || gpio->Alternate!=GPIO_AF12_I2C)) ++contract_errors;
  if (gpio->Pin==GPIO_PIN_6 && (gpio->Mode!=GPIO_MODE_OUTPUT_OD || gpio->Pull!=GPIO_NOPULL)) ++contract_errors;
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


static uint16_t conf, channels[4];
static uint32_t operations, resets, lamp, fail_at, bad_conf, init_failure, triggers;
uint32_t test_ops[512];
void Test_BusReset(void) { ++resets; }
void HAL_GPIO_WritePin(void *port,uint32_t pin,uint32_t state)
{ (void)port; if(pin==GPIO_PIN_6) lamp=state; }
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *bus)
{
  if (bus->Instance!=I2C || bus->Init.ClockSpeed!=100000U) ++contract_errors;
  return init_failure ? HAL_ERROR : HAL_OK;
}
static uint8_t Contract(uint16_t address,uint16_t reg,uint16_t memsize,uint16_t size,uint32_t timeout)
{
  ++operations;
  if (address!=0x20U || memsize!=I2C_MEMADD_SIZE_8BIT || size!=2U || timeout!=5U ||
      (reg!=0U && (reg<8U || reg>11U))) ++contract_errors;
  return operations!=fail_at;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *bus,uint16_t address,uint16_t reg,
                                  uint16_t memsize,uint8_t *data,uint16_t size,uint32_t timeout)
{
  (void)bus;
  uint16_t value=data[0] | ((uint16_t)data[1]<<8U);
  uint8_t ok=Contract(address,reg,memsize,size,timeout);
  if (operations<=512U) test_ops[operations-1U]=((uint32_t)reg<<16U) | value;
  if (!ok) return HAL_TIMEOUT;
  if (reg==0U) { conf=value & ~4U; if(value & 4U) ++triggers; }
  return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus,uint16_t address,uint16_t reg,
                                 uint16_t memsize,uint8_t *data,uint16_t size,uint32_t timeout)
{
  (void)bus;
  uint8_t ok=Contract(address,reg,memsize,size,timeout);
  uint16_t value=reg==0U ? (conf ^ bad_conf) : channels[reg-8U];
  if (operations<=512U) test_ops[operations-1U]=0x80000000U | ((uint32_t)reg<<16U) | value;
  data[0]=(uint8_t)value; data[1]=(uint8_t)(value>>8U); /* Dirty bytes on error test non-publication. */
  return ok ? HAL_OK : HAL_ERROR;
}
void Test_Init(uint32_t now,uint32_t unused)
{
  (void)unused;tick=now;conf=1U;operations=resets=lamp=fail_at=bad_conf=init_failure=triggers=contract_errors=0U;
  tx_size=0;for(uint8_t i=0;i<4;++i) channels[i]=0;
  ColorSensor_Init();ModuleUART_Init();
}
void Test_Step(uint32_t now) { tick=now;ColorSensor_Process(); }
void Test_Time(uint32_t now) { tick=now; }
void Test_Channel(uint32_t index,uint32_t value) { if(index<4U) channels[index]=(uint16_t)value; }
void Test_FailAt(uint32_t operation) { fail_at=operation; }
void Test_BadConf(uint32_t mask) { bad_conf=mask; }
void Test_InitFailure(uint32_t fail) { init_failure=fail; }
uint32_t Test_SetIT(uint32_t ms) { return ColorSensor_SetIntegration((uint16_t)ms); }
uint32_t Test_SetLED(uint32_t on) { return ColorSensor_SetLED((uint8_t)on); }
uint32_t Test_Field(uint32_t field)
{
  ColorReading v=ColorSensor_GetReading();
  switch(field) {
    case 0:case 1:case 2:case 3:return v.channels[field];
    case 4:return v.integration_ms;case 5:return v.led;case 6:return v.valid;
    case 7:return v.error;case 8:return v.saturation;case 9:return v.updated_ms;
    case 10:return v.sequence;case 11:return operations;case 12:return resets;
    case 13:return lamp;case 14:return contract_errors;case 15:return triggers;
    default:return 0xffffffffU;
  }
}
void Test_FeedByte(uint32_t byte)
{ *rx_destination=(uint8_t)byte;HAL_UART_RxCpltCallback(rx_uart); }
void Test_ProcessUART(void) { ModuleUART_Process(); }
void Test_Error(void) { HAL_UART_ErrorCallback(rx_uart); }
uint32_t Test_TxSize(void) { return tx_size; }
void Test_ClearTx(void) { tx_size=0; }
