/* Minimal test HAL. Production builds use User/Inc/main.h and the real HAL.
 * These stubs model GPIO/time/UART calls, not peripheral register behavior. */
#ifndef STEPPER_TEST_MAIN_H
#define STEPPER_TEST_MAIN_H
#include <stdint.h>
#include <stddef.h>
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef struct {
  void *Instance;
  struct { uint32_t BaudRate, WordLength, StopBits, Parity, Mode, HwFlowCtl, OverSampling; } Init;
} UART_HandleTypeDef;
#define GPIOA (&test_gpioa)
#define GPIOB (&test_gpiob)
#define USART3 ((void *)2)
#define GPIO_PIN_2 4U
#define GPIO_PIN_4 16U
#define GPIO_PIN_5 32U
#define GPIO_PIN_SET 1U
#define GPIO_MODE_INPUT 0U
#define GPIO_MODE_AF_PP 1U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_HIGH 1U
#define GPIO_AF4_USART3 4U
#define UART_WORDLENGTH_8B 0U
#define UART_STOPBITS_1 0U
#define UART_PARITY_NONE 0U
#define UART_MODE_TX_RX 3U
#define UART_HWCONTROL_NONE 0U
#define UART_OVERSAMPLING_16 0U
#define USART3_4_IRQn 29U
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_USART3_CLK_ENABLE() ((void)0)
#define __HAL_UART_CLEAR_OREFLAG(uart) ((void)(uart))
static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t mask) { (void)mask; }
static inline void __DMB(void) {}
uint32_t HAL_GetTick(void);
uint32_t HAL_GPIO_ReadPin(void *, uint32_t);
void HAL_GPIO_Init(void *, GPIO_InitTypeDef *);
void HAL_NVIC_SetPriority(uint32_t, uint32_t, uint32_t);
void HAL_NVIC_EnableIRQ(uint32_t);
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
void HAL_UART_IRQHandler(UART_HandleTypeDef *);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *);
void APP_ErrorHandler(void);

typedef struct { uint32_t BSRR,IDR; } GPIO_TypeDef;
extern GPIO_TypeDef test_gpioa,test_gpiob;
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define GPIO_PIN_3 8U
#define GPIO_PIN_6 64U
#define GPIO_PIN_7 128U
#define GPIO_PIN_8 256U
#define GPIO_PIN_9 512U
#define GPIO_PIN_10 1024U
#define GPIO_PIN_11 2048U
#define GPIO_MODE_OUTPUT_PP 2U
#define GPIO_MODE_IT_FALLING 3U
#define GPIO_MODE_ANALOG 4U
#define GPIO_PULLDOWN 2U
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM6_CLK_ENABLE() ((void)0)
#define EXTI4_15_IRQn 7U
#define TIM6_LPTIM1_IRQn 17U
#define TIM_COUNTERMODE_UP 0U
#define TIM_AUTORELOAD_PRELOAD_DISABLE 0U
#define TIM_SR_UIF 1U
#define TIM_FLAG_UPDATE 1U
#define __HAL_TIM_CLEAR_FLAG(h,f) ((h)->Instance->SR &= ~(f))
typedef struct { uint32_t SR; } TIM_TypeDef;
typedef struct {
 TIM_TypeDef *Instance;
 struct { uint32_t Prescaler,Period,CounterMode,AutoReloadPreload; } Init;
} TIM_HandleTypeDef;
extern TIM_TypeDef test_tim6;
#define TIM6 (&test_tim6)
typedef struct { uint32_t PR; } EXTI_TypeDef;
extern EXTI_TypeDef test_exti;
#define EXTI (&test_exti)
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *);
#endif
