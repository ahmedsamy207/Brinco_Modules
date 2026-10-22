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
  /* The board's 24 MHz crystal feeds the PLL directly: 24 x 3 = 72 MHz. */
  oscillator.PLL.PLLState = RCC_PLL_ON;
  oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  oscillator.PLL.PLLMUL = RCC_PLL_MUL3;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) APP_ErrorHandler();

  clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clocks.APB1CLKDivider = RCC_HCLK_DIV1;
  /* Configure flash wait states before increasing the core clock. */
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_2) != HAL_OK) {
    APP_ErrorHandler();
  }
  /* HSE_VALUE must agree across HAL and CMSIS SystemCoreClockUpdate(). */
  SystemCoreClockUpdate();
  if (SystemCoreClock != 72000000U) APP_ErrorHandler();
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
