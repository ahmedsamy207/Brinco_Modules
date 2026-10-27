/** @file color_bus.c
 *  @brief VEML6040 word transfers, repeated-start reads and mixed-rail GPIOs.
 *  Requires external I2C pull-ups and a voltage-compatible bus arrangement.
 *  The corrected _New schematic supplies BSS138 translators and pull-ups on
 *  both voltage domains; actual rework and rise times still need verification.
 */
#include "main.h"
#include "color_bus.h"
#include "color_config.h"
static I2C_HandleTypeDef bus;

uint8_t ColorBus_Reset(void)
{
  __HAL_RCC_I2C_FORCE_RESET();
  __HAL_RCC_I2C_RELEASE_RESET();
  /* HAL software state must be reset along with peripheral registers. */
  bus = (I2C_HandleTypeDef){0};
  bus.Instance = I2C;
  bus.Init.ClockSpeed = COLOR_I2C_HZ;
  bus.Init.DutyCycle = I2C_DUTYCYCLE_2;
  bus.Init.OwnAddress1 = 0U;
  bus.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  bus.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  return HAL_I2C_Init(&bus) == HAL_OK;
}

void ColorBus_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_I2C_CLK_ENABLE();
  /* PA6 releases to the external 3.3 V lamp-gate pull-up. I2C uses external
   * pull-ups: 5 V on the MCU side, 3.3 V on the sensor side of the BSS138s.
   * Keep internal pulls disabled; validate external values on the board. */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, COLOR_DEFAULT_LED ? GPIO_PIN_SET : GPIO_PIN_RESET);
  gpio.Pin = GPIO_PIN_6;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  gpio.Mode = GPIO_MODE_AF_OD;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF12_I2C;
  HAL_GPIO_Init(GPIOA, &gpio);
}

uint8_t ColorBus_WriteWord(uint8_t reg, uint16_t value)
{
  uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8U)};
  return HAL_I2C_Mem_Write(&bus, COLOR_I2C_ADDRESS << 1U, reg,
                           I2C_MEMADD_SIZE_8BIT, bytes, 2U, COLOR_I2C_TIMEOUT_MS) == HAL_OK;
}

uint8_t ColorBus_ReadWord(uint8_t reg, uint16_t *value)
{
  uint8_t bytes[2];
  /* HAL memory-read sends the register pointer, repeated START, then two
   * data bytes without a STOP between pointer and read (datasheet p7). */
  if (HAL_I2C_Mem_Read(&bus, COLOR_I2C_ADDRESS << 1U, reg, I2C_MEMADD_SIZE_8BIT,
                      bytes, 2U, COLOR_I2C_TIMEOUT_MS) != HAL_OK) return 0U;
  *value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
  return 1U;
}

void ColorBus_SetLED(uint8_t on)
{ HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, on ? GPIO_PIN_SET : GPIO_PIN_RESET); }
