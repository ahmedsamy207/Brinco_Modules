/**
  ******************************************************************************
  * @file    main.c
  * @author  Brinco
  * @brief   IR array module application.
  ******************************************************************************
  * @attention
  *
  * Entry point for the IR array module. Sensor acquisition and UART command
  * handling share a cooperative foreground loop; interrupt handlers only
  * capture timing or receive bytes. Do not add long delays to this loop.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ir_array.h"
#include "module_uart.h"

/* Private define ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/**
  * @brief  Main program.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, initialise the SysTick */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize acquisition before accepting host commands. */
  IRArray_Init();
  ModuleUART_Init();

  while (1)
  {
    /* Refresh sensor state before replying to the next queued command. */
    IRArray_Process();
    ModuleUART_Process();
  }
}

/**
  * @brief System Clock Configuration
  * @note  Runs from the fitted external crystal at the project clock target.
  * @retval None
  */
/** Clock policy: use the fitted crystal; halt if HSE/PLL startup fails.
 *  HSI is only the reset/bootstrap source, never an application fallback.
 *  Keep APB undivided so timer clocks equal the peripheral clock.
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef oscillator = {0};
  RCC_ClkInitTypeDef clocks = {0};

  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  oscillator.HSEState = RCC_HSE_ON;
  oscillator.HSEFreq = RCC_HSE_16_32MHz;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) APP_ErrorHandler();

  clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  clocks.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clocks.APB1CLKDivider = RCC_HCLK_DIV1;
  /* Configure flash wait states before increasing the core clock. */
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_1) != HAL_OK) {
    APP_ErrorHandler();
  }
  /* HSE_VALUE must agree across HAL and CMSIS SystemCoreClockUpdate(). */
  SystemCoreClockUpdate();
  if (SystemCoreClock != 32000000U) APP_ErrorHandler();
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void APP_ErrorHandler(void)
{
  /* Fatal initialization/configuration error: halt for debugger inspection. */
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     for example: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE****/
