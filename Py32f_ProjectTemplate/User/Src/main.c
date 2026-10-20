#include "main.h"

void SystemClock_Config(void);
void APP_ErrorHandler(void);

__attribute__((weak)) void _init(void)  {}
__attribute__((weak)) void _fini(void) {}

/**
	* @brief	Main program.
	* @retval int
	*/
int main(void)
{
	HAL_Init();
	SystemClock_Config();

	while (1)
	{
		HAL_Delay(1000);
	}
}

/**
	* @brief System Clock Configuration
	* @retval None
	*/
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_24MHz;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL3;      // 24MHz × 3 = 72MHz
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		APP_ErrorHandler();
	}

	/* Enable LPTIM clock if required */
	__HAL_RCC_LPTIM_CLK_ENABLE();

	/* Configure clocks: HSI as SYSCLK, AHB and APB1 with no division */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
		APP_ErrorHandler();
	}
}

/**
	* @brief	This function is executed in case of error occurrence.
	* @param	None
	* @retval None
	*/
void APP_ErrorHandler(void)
{
	/* infinite loop */
	while (1)
	{
	}
}
