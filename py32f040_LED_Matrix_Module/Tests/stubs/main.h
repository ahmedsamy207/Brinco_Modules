/* Test-only register/HAL model. Does not model actual SPI FIFOs or bus timing. */
#ifndef MATRIX_TEST_MAIN_H
#define MATRIX_TEST_MAIN_H
#include <stdint.h>
typedef enum { HAL_OK=0, HAL_ERROR=1 } HAL_StatusTypeDef;
typedef struct { uint32_t CR1,CR2,SR,DR; } SPI_TypeDef;
typedef struct { uint32_t RTSR,FTSR,SWIER,PR,EXTICR[4],IMR; } EXTI_TypeDef;
typedef struct { uint32_t Pin,Mode,Pull,Speed,Alternate; } GPIO_InitTypeDef;
typedef struct {
  SPI_TypeDef *Instance;
  struct {
    uint32_t Mode,Direction,DataSize,CLKPolarity,CLKPhase,NSS,FirstBit;
    uint32_t CRCCalculation,CRCPolynomial,BaudRatePrescaler;
  } Init;
} SPI_HandleTypeDef;
extern SPI_TypeDef test_spi;
extern EXTI_TypeDef test_exti;
#define SPI1 (&test_spi)
#define EXTI (&test_exti)
#define GPIOA ((void *)1)
#define GPIOB ((void *)2)
#define GPIO_PIN_3 8U
#define GPIO_PIN_4 16U
#define GPIO_PIN_5 32U
#define GPIO_PIN_15 32768U
#define GPIO_PIN_SET 1U
#define GPIO_PIN_RESET 0U
#define GPIO_MODE_AF_PP 1U
#define GPIO_PULLUP 1U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_HIGH 1U
#define GPIO_AF0_SPI1 0U
#define SPI_MODE_SLAVE 0U
#define SPI_DIRECTION_2LINES 0U
#define SPI_DATASIZE_8BIT 0U
#define SPI_POLARITY_LOW 0U
#define SPI_PHASE_1EDGE 0U
#define SPI_NSS_HARD_INPUT 0U
#define SPI_FIRSTBIT_MSB 0U
#define SPI_CRCCALCULATION_DISABLE 0U
#define SPI_BAUDRATEPRESCALER_2 0U
#define SPI_CR1_SPE 64U
#define SPI_CR2_RXNEIE 64U
#define SPI_CR2_TXEIE 128U
#define SPI_CR2_ERRIE 32U
#define SPI_SR_RXNE 1U
#define SPI_SR_TXE 2U
#define SPI_SR_MODF 32U
#define SPI_SR_OVR 64U
#define SPI1_IRQn 25U
#define EXTI4_15_IRQn 7U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_SPI1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_SPI1_FORCE_RESET() Test_SPIReset()
#define __HAL_RCC_SPI1_RELEASE_RESET() ((void)0)
static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t mask) { (void)mask; }
void Test_SPIReset(void);
uint32_t HAL_GetTick(void);
uint32_t HAL_GPIO_ReadPin(void *,uint32_t);
void HAL_GPIO_Init(void *,GPIO_InitTypeDef *);
void HAL_NVIC_SetPriority(uint32_t,uint32_t,uint32_t);
void HAL_NVIC_EnableIRQ(uint32_t);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *);
void APP_ErrorHandler(void);
#endif
