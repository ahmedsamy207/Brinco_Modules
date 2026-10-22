/**
  ******************************************************************************
  * @file    main.c
  * @author  Brinco
  * @brief   Main program body - PY32F003 project template.
  ******************************************************************************
  * @attention
  *
  * This file is the starting point for a new PY32F003 module. Everything else
  * (startup file, HAL, linker script, debug configuration) is selected by
  * MCU_TYPE in the Makefile.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ultrasonic.h"
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
  Ultrasonic_Init();
  ModuleUART_Init();

  /* Initialise your peripherals here:
   *   MX_GPIO_Init();
   *   MX_USART1_UART_Init();
   */

  while (1)
  {
    Ultrasonic_Process();
    ModuleUART_Process();
  }
}

/**
  * @brief System Clock Configuration
  * @note  PY32F003 has no PLL: the maximum system clock is the 24 MHz HSI.
  *       Change HSICalibrationValue to 4/8/16/22.12 MHz if you need less.
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* Configure the oscillator: enable HSI at 24 MHz with no division */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Enable LPTIM clock if required */
  __HAL_RCC_LPTIM_CLK_ENABLE();

  /* Configure clocks: HSI as SYSCLK, AHB and APB1 with no division */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void APP_ErrorHandler(void)
{
  /* infinite loop */
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
