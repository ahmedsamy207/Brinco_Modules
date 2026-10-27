/** @file downstream.c
 * SPI2 master, PB8 SCK/PB6 MISO/PB7 MOSI AF8, PB9 GPIO chip select.
 * 72MHz/256=281250Hz, mode0. Interrupt-driven transfer leaves upstream SPI
 * and all UARTs responsive. Foreground enforces >=5ms CS high and >=1ms setup.
 */
#include "main.h"
#include "hub.h"
#include <string.h>
static SPI_HandleTypeDef spi;
static uint8_t tx[HUB_FRAME],rx[HUB_FRAME];
static volatile uint8_t finished;
static volatile uint16_t received, sent;
static uint32_t cr1_config, cr2_config;
/* Shared with IRQ:0 idle,1 CS setup,2 shifting,3 complete,4 failed. */
static volatile uint8_t state;
static uint32_t since,ended;
static void End(uint8_t outcome)
{
  SPI2->CR2 &= ~(SPI_CR2_RXNEIE|SPI_CR2_TXEIE|SPI_CR2_ERRIE);
  SPI2->CR1 &= ~SPI_CR1_SPE;
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_SET);
  ended=HAL_GetTick();state=outcome;
}
void Downstream_Init(void)
{
  GPIO_InitTypeDef g={0};__HAL_RCC_GPIOB_CLK_ENABLE();__HAL_RCC_SPI2_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_SET);
  g.Pin=GPIO_PIN_9;g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_PULLUP;g.Speed=GPIO_SPEED_FREQ_HIGH;HAL_GPIO_Init(GPIOB,&g);
  g.Pin=GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8;g.Mode=GPIO_MODE_AF_PP;g.Alternate=GPIO_AF8_SPI2;g.Pull=GPIO_PULLDOWN;HAL_GPIO_Init(GPIOB,&g);
  spi.Instance=SPI2;spi.Init.Mode=SPI_MODE_MASTER;spi.Init.Direction=SPI_DIRECTION_2LINES;
  spi.Init.DataSize=SPI_DATASIZE_8BIT;spi.Init.CLKPolarity=SPI_POLARITY_LOW;
  spi.Init.CLKPhase=SPI_PHASE_1EDGE;spi.Init.NSS=SPI_NSS_SOFT;
  spi.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_256;spi.Init.FirstBit=SPI_FIRSTBIT_MSB;
  spi.Init.CRCCalculation=SPI_CRCCALCULATION_DISABLE;spi.Init.CRCPolynomial=7;
  if(HAL_SPI_Init(&spi)!=HAL_OK) APP_ErrorHandler();
  cr1_config=SPI2->CR1 & ~SPI_CR1_SPE;
  cr2_config=SPI2->CR2 & ~(SPI_CR2_RXNEIE|SPI_CR2_TXEIE|SPI_CR2_ERRIE);
  HAL_NVIC_SetPriority(SPI2_IRQn,1,0);HAL_NVIC_EnableIRQ(SPI2_IRQn);ended=HAL_GetTick();
}
uint8_t Downstream_Start(const uint8_t *p)
{
  /* Tick timestamps truncate to milliseconds: add one tick to guarantee
   * at least5ms high even when CS changed just before the next SysTick. */
  if(state || (uint32_t)(HAL_GetTick()-ended)<6U) return 0;
  __HAL_RCC_SPI2_FORCE_RESET();__HAL_RCC_SPI2_RELEASE_RESET();
  SPI2->CR1=cr1_config;SPI2->CR2=cr2_config;
  memcpy(tx,p,HUB_FRAME);finished=0;received=0;sent=0;since=HAL_GetTick();state=1;
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_9,GPIO_PIN_RESET);return 1;
}
void Downstream_Process(void)
{
  uint32_t now=HAL_GetTick();
  /* Two tick boundaries guarantee a full1ms physical CS setup interval. */
  if(state==1 && (uint32_t)(now-since)>=2U) {
    state=2;
    SPI2->CR1 |= SPI_CR1_SPE;
    SPI2->CR2 |= SPI_CR2_RXNEIE|SPI_CR2_TXEIE|SPI_CR2_ERRIE;
  }
  if(state==2) {
    /* IRQ posts completion. Wait for physical last-bit idle in foreground. */
    if(finished && !(SPI2->SR&SPI_SR_BSY)) End(finished==1?3:4);
    else if((uint32_t)(now-since)>HUB_SPI_TIMEOUT_MS) {
      End(4);
    }
  }
}
uint8_t Downstream_Result(uint8_t *p)
{
  uint8_t result=state;
  if(result<3) return 0;
  if(result==3) memcpy(p,rx,HUB_FRAME);
  state=0;return result==3?1:2;
}
/** Cancel a timed-out route, including its unfinished local SPI transaction. */
void Downstream_Cancel(void)
{
  uint32_t m=__get_PRIMASK();__disable_irq();End(0);__set_PRIMASK(m);
}
/** No polling loops in ISR, including error paths. One byte is bounded work. */
void Downstream_IRQHandler(void)
{
  uint32_t flags=SPI2->SR;
  if(state!=2) return;
  if(flags&(SPI_SR_OVR|SPI_SR_MODF)) {
    SPI2->CR2 &= ~(SPI_CR2_RXNEIE|SPI_CR2_TXEIE|SPI_CR2_ERRIE);
    finished=2;return;
  }
  if(flags&SPI_SR_RXNE) {
    uint8_t value=*(volatile uint8_t *)&SPI2->DR;
    if(received<HUB_FRAME) rx[received++]=value;
    if(received==HUB_FRAME) {
      SPI2->CR2 &= ~(SPI_CR2_RXNEIE|SPI_CR2_TXEIE|SPI_CR2_ERRIE);
      finished=1;return;
    }
  }
  if((SPI2->SR&SPI_SR_TXE) && (SPI2->CR2&SPI_CR2_TXEIE)) {
    if(sent<HUB_FRAME) *(volatile uint8_t *)&SPI2->DR=tx[sent++];
    else SPI2->CR2 &= ~SPI_CR2_TXEIE;
  }
}
