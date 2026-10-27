/** @file ports.c
 * Four 115200 8N1 UARTs. IRQs receive a byte into a bounded ring; foreground
 * queues whole commands and reads bounded chunks. RX overflow drops newest.
 * TX uses a separate holding byte, so CLEAR cannot invalidate an active buffer.
 */
#include "main.h"
#include "hub.h"
#include <string.h>
#define RING 256U
static UART_HandleTypeDef uart[4];
static uint8_t rx[4][RING],tx[4][RING],incoming[4],outgoing[4];
static volatile uint16_t rh[4],rt[4],th[4],tt[4],errors[4];
static volatile uint8_t active[4];
/* Monotonic16-bit indices preserve full256-byte capacity across wraparound. */
static uint32_t Lock(void) { uint32_t m=__get_PRIMASK();__disable_irq();return m; }
static void Flag(uint8_t p,uint16_t bit) { errors[p]|=bit; }
void Ports_Init(void)
{
  GPIO_InitTypeDef g={0};
  USART_TypeDef *instances[4]={USART1,USART2,USART3,USART4};
  __HAL_RCC_GPIOA_CLK_ENABLE();__HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();__HAL_RCC_USART2_CLK_ENABLE();
  __HAL_RCC_USART3_CLK_ENABLE();__HAL_RCC_USART4_CLK_ENABLE();
  g.Mode=GPIO_MODE_AF_PP;g.Pull=GPIO_PULLUP;g.Speed=GPIO_SPEED_FREQ_HIGH;
  g.Pin=GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_2|GPIO_PIN_3;g.Alternate=GPIO_AF1_USART1;HAL_GPIO_Init(GPIOA,&g);
  g.Pin=GPIO_PIN_0|GPIO_PIN_1;g.Alternate=GPIO_AF4_USART4;HAL_GPIO_Init(GPIOA,&g);
  g.Pin=GPIO_PIN_10|GPIO_PIN_11;g.Alternate=GPIO_AF4_USART3;HAL_GPIO_Init(GPIOB,&g);
  for(uint8_t p=0;p<4;p++) {
    uart[p].Instance=instances[p];uart[p].Init.BaudRate=115200;
    uart[p].Init.WordLength=UART_WORDLENGTH_8B;uart[p].Init.StopBits=UART_STOPBITS_1;
    uart[p].Init.Parity=UART_PARITY_NONE;uart[p].Init.Mode=UART_MODE_TX_RX;
    uart[p].Init.HwFlowCtl=UART_HWCONTROL_NONE;uart[p].Init.OverSampling=UART_OVERSAMPLING_16;
    if(HAL_UART_Init(&uart[p])!=HAL_OK) APP_ErrorHandler();
    if(HAL_UART_Receive_IT(&uart[p],&incoming[p],1)!=HAL_OK) APP_ErrorHandler();
  }
  HAL_NVIC_SetPriority(USART1_IRQn,2,0);HAL_NVIC_EnableIRQ(USART1_IRQn);
  HAL_NVIC_SetPriority(USART2_IRQn,2,0);HAL_NVIC_EnableIRQ(USART2_IRQn);
  HAL_NVIC_SetPriority(USART3_4_IRQn,2,0);HAL_NVIC_EnableIRQ(USART3_4_IRQn);
}
uint8_t Ports_Write(uint8_t p,const uint8_t *data,uint8_t n)
{
  /* Only foreground changes head. Consumer IRQ may free space meanwhile. */
  if(n>RING-(uint16_t)(th[p]-tt[p])) return 0;
  for(uint8_t i=0;i<n;i++) tx[p][(th[p]+i)&255U]=data[i];
  __DMB();th[p]+=n;return 1;
}
uint8_t Ports_Read(uint8_t p,uint8_t *data,uint8_t n)
{
  uint8_t i=0;
  while(i<n && rt[p]!=rh[p]) { data[i++]=rx[p][rt[p]&255U];rt[p]++; }
  return i;
}
void Ports_Status(uint8_t p,uint8_t *data)
{
  uint32_t m=Lock();uint16_t values[3]={(uint16_t)(rh[p]-rt[p]),(uint16_t)(th[p]-tt[p]+active[p]),errors[p]};
  __set_PRIMASK(m);
  for(uint8_t i=0;i<3;i++) { data[2*i]=(uint8_t)values[i];data[2*i+1]=(uint8_t)(values[i]>>8); }
}
void Ports_Clear(uint8_t p)
{
  /* Clear sticky errors only; never silently discard queued module commands. */
  uint32_t m=Lock();errors[p]=0;__set_PRIMASK(m);
}
void Ports_Process(void)
{
  for(uint8_t p=0;p<4;p++) {
    uint32_t m=Lock();
    if(!active[p] && tt[p]!=th[p]) {
      outgoing[p]=tx[p][tt[p]&255U];active[p]=1;
      if(HAL_UART_Transmit_IT(&uart[p],&outgoing[p],1)==HAL_OK) tt[p]++;
      else { active[p]=0;Flag(p,4); }
    }
    /* HAL aborts RX on overrun: retry here as well as in its error callback. */
    if(uart[p].RxState==HAL_UART_STATE_READY && HAL_UART_Receive_IT(&uart[p],&incoming[p],1)!=HAL_OK) Flag(p,2);
    __set_PRIMASK(m);
  }
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
  for(uint8_t p=0;p<4;p++) if(h==&uart[p]) {
    if((uint16_t)(rh[p]-rt[p])<RING) { rx[p][rh[p]&255U]=incoming[p];__DMB();rh[p]++; }
    else Flag(p,1);
    if(HAL_UART_Receive_IT(h,&incoming[p],1)!=HAL_OK) Flag(p,2);
  }
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h)
{
  for(uint8_t p=0;p<4;p++) if(h==&uart[p]) active[p]=0;
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
  for(uint8_t p=0;p<4;p++) if(h==&uart[p]) Flag(p,2);
}
void Ports_IRQHandler(uint8_t p) { HAL_UART_IRQHandler(&uart[p]); }
