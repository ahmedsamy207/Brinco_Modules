/** @file spi_transport.c
 *  @brief Bounded SPI byte service; no HAL polling waits inside interrupts.
 *
 * PA15=NSS, PB3=SCK, PB4=MISO, PB5=MOSI, all AF0. Hardware NSS gates the
 * slave. RX IRQ drains bytes, TX IRQ feeds an immutable reply, and EXTI15
 * rising closes the transaction. Foreground validates exactly 160 bytes and
 * resets SPI before the next transaction, flushing any prefetched old bytes.
 * Host contract: mode 0/MSB first <=250 kHz, 10 us CS setup, >=5 ms CS high.
 */
#include "main.h"
#include "tft_protocol.h"
#include "spi_transport.h"

static SPI_HandleTypeDef spi;
static uint8_t rx[TFT_SPI_FRAME_BYTES], tx[TFT_SPI_FRAME_BYTES];
static volatile uint16_t rx_count, tx_count;
static volatile uint8_t frame_ends, bad_frame, armed;
static uint32_t cr1_config, cr2_config, selected_ms;
static uint8_t selected;

static void DisableSPI(void)
{
  SPI1->CR2 &= ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE | SPI_CR2_ERRIE);
  SPI1->CR1 &= ~SPI_CR1_SPE;
}

/** @brief Drain one received byte, bounding writes even on overlong frames. */
static void ReceiveByte(void)
{
  uint8_t value = *(volatile uint8_t *)&SPI1->DR;
  if (rx_count < TFT_SPI_FRAME_BYTES) rx[rx_count++] = value;
  else bad_frame = 1U;
}

static void Arm(void)
{
  uint32_t mask;
  TFTProtocol_BuildReply(tx);
  mask = __get_PRIMASK();
  __disable_irq();
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_SET && frame_ends == 0U) {
    __HAL_RCC_SPI1_FORCE_RESET();
    __HAL_RCC_SPI1_RELEASE_RESET();
    SPI1->CR1 = cr1_config;
    SPI1->CR2 = cr2_config;
    rx_count = 0U; tx_count = 1U; bad_frame = 0U;
    selected = 0U;
    armed = 1U;
    SPI1->CR1 |= SPI_CR1_SPE;
    *(volatile uint8_t *)&SPI1->DR = tx[0];
    SPI1->CR2 |= SPI_CR2_RXNEIE | SPI_CR2_TXEIE | SPI_CR2_ERRIE;
  }
  __set_PRIMASK(mask);
}

void SPITransport_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_SPI1_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_15;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF0_SPI1;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &gpio);
  spi.Instance = SPI1;
  spi.Init.Mode = SPI_MODE_SLAVE;
  spi.Init.Direction = SPI_DIRECTION_2LINES;
  spi.Init.DataSize = SPI_DATASIZE_8BIT;
  spi.Init.CLKPolarity = SPI_POLARITY_LOW;
  spi.Init.CLKPhase = SPI_PHASE_1EDGE;
  spi.Init.NSS = SPI_NSS_HARD_INPUT;
  spi.Init.FirstBit = SPI_FIRSTBIT_MSB;
  spi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  spi.Init.CRCPolynomial = 7U;
  spi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2; /* Ignored in slave mode. */
  if (HAL_SPI_Init(&spi) != HAL_OK) APP_ErrorHandler();
  cr1_config = SPI1->CR1 & ~SPI_CR1_SPE;
  cr2_config = SPI1->CR2 & ~(SPI_CR2_RXNEIE | SPI_CR2_TXEIE | SPI_CR2_ERRIE);

  /* EXTI mux fields are eight bits apart on this part. Route line 15 to PA. */
  EXTI->EXTICR[3] &= ~(0x0FUL << 24U);
  EXTI->FTSR &= ~GPIO_PIN_15;
  EXTI->RTSR |= GPIO_PIN_15;
  EXTI->PR = GPIO_PIN_15;
  EXTI->IMR |= GPIO_PIN_15;
  /* Last-byte RX is serviced before lower-priority CS-rising finalization. */
  HAL_NVIC_SetPriority(SPI1_IRQn, 0U, 0U);
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(SPI1_IRQn);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
  Arm();
}

void SPITransport_IRQHandler(void)
{
  uint32_t status = SPI1->SR;
  if (!armed) return;
  if ((status & (SPI_SR_OVR | SPI_SR_MODF)) != 0U) bad_frame = 1U;
  if ((status & SPI_SR_RXNE) != 0U) ReceiveByte();
  /* Clear overrun by DR then SR reads. A mode fault ends this transaction. */
  if ((status & SPI_SR_OVR) != 0U) {
    volatile uint32_t ignored = SPI1->DR;
    ignored = SPI1->SR;
    (void)ignored;
  }
  if ((status & SPI_SR_MODF) != 0U) { DisableSPI(); return; }
  if ((SPI1->SR & SPI_SR_TXE) != 0U && (SPI1->CR2 & SPI_CR2_TXEIE) != 0U) {
    if (tx_count < TFT_SPI_FRAME_BYTES) {
      *(volatile uint8_t *)&SPI1->DR = tx[tx_count++];
    } else SPI1->CR2 &= ~SPI_CR2_TXEIE;
  }
}

void SPITransport_NSS_IRQHandler(void)
{
  if ((EXTI->PR & GPIO_PIN_15) == 0U) return;
  EXTI->PR = GPIO_PIN_15;
  if (!armed) bad_frame = 1U; /* Do not reuse bytes from an earlier transaction. */
  /* Drain a final pending byte before freezing the input buffer. */
  if (armed && (SPI1->SR & SPI_SR_RXNE) != 0U) ReceiveByte();
  if ((SPI1->SR & (SPI_SR_OVR | SPI_SR_MODF)) != 0U) bad_frame = 1U;
  DisableSPI();
  armed = 0U;
  if (frame_ends < 255U) ++frame_ends;
}

void SPITransport_Process(void)
{
  uint32_t now = HAL_GetTick();
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15) == GPIO_PIN_RESET) {
    if (!selected) { selected = 1U; selected_ms = now; }
    else if (armed && (uint32_t)(now-selected_ms) > TFT_SPI_TIMEOUT_MS) {
      uint32_t mask = __get_PRIMASK();
      __disable_irq();
      DisableSPI(); armed=0U; bad_frame=1U;
      __set_PRIMASK(mask);
      TFTProtocol_TransportError();
    }
    return; /* Never publish or re-arm a response while CS is asserted. */
  }
  if (frame_ends) {
    uint8_t ends;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    ends=frame_ends; frame_ends=0U;
    __set_PRIMASK(mask);
    if (ends==1U && !bad_frame && rx_count==TFT_SPI_FRAME_BYTES) TFTProtocol_Handle(rx);
    else TFTProtocol_TransportError();
    selected=0U;
  }
  if (!armed) Arm();
}
