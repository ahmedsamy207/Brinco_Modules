#include "module_uart.h"
#include "ultrasonic.h"
#include <string.h>

#define RX_CAPACITY 128U
#define LINE_CAPACITY 32U
#define LINE_TIMEOUT_MS 500U
#define STALE_MS 200U

static UART_HandleTypeDef uart;
static uint8_t rx_byte;
static uint8_t rx_queue[RX_CAPACITY];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint8_t rx_lost;
static volatile uint8_t rx_restart;
static char line[LINE_CAPACITY];
static uint8_t line_size;
static uint8_t discard_line;
static uint32_t last_byte_ms;

static char *AppendDecimal(char *destination, uint32_t value)
{
  char digits[10];
  uint8_t count = 0U;
  do {
    digits[count++] = (char)('0' + value % 10U);
    value /= 10U;
  } while (value != 0U);
  while (count != 0U) *destination++ = digits[--count];
  return destination;
}

static void Reply(const char *message)
{
  /* RX remains interrupt-driven during this bounded foreground transmission.
   * A host must wait for the reply before sending the next command. */
  (void)HAL_UART_Transmit(&uart, (uint8_t *)message, (uint16_t)strlen(message), 20U);
}

static void ExecuteCommand(void)
{
  char response[48];
  UltrasonicReading value;
  uint32_t age;
  line[line_size] = '\0';
  if (strcmp(line, "PING") == 0) {
    Reply("PONG\r\n");
  } else if (strcmp(line, "INFO") == 0) {
    Reply("INFO BRINCO ULTRASONIC 1\r\n");
  } else if (strcmp(line, "DIST") == 0) {
    value = Ultrasonic_GetReading();
    age = HAL_GetTick() - value.updated_ms;
    if (value.status == ULTRASONIC_NOT_READY) {
      Reply("ERR NOT_READY\r\n");
    } else if (age > STALE_MS) {
      Reply("ERR STALE\r\n");
    } else if (value.status == ULTRASONIC_TIMEOUT) {
      Reply("ERR NO_ECHO\r\n");
    } else if (value.status != ULTRASONIC_OK) {
      Reply("ERR INVALID_ECHO\r\n");
    } else {
      char *end;
      memcpy(response, "DIST ", 5U);
      end = AppendDecimal(response + 5U, value.distance_mm);
      *end++ = ' ';
      end = AppendDecimal(end, age);
      *end++ = '\r';
      *end++ = '\n';
      *end = '\0';
      Reply(response);
    }
  } else {
    Reply("ERR COMMAND\r\n");
  }
}

void ModuleUART_Init(void)
{
  GPIO_InitTypeDef gpio = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();
  gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF9_USART2;
  HAL_GPIO_Init(GPIOA, &gpio);

  uart.Instance = USART2;
  uart.Init.BaudRate = 115200U;
  uart.Init.WordLength = UART_WORDLENGTH_8B;
  uart.Init.StopBits = UART_STOPBITS_1;
  uart.Init.Parity = UART_PARITY_NONE;
  uart.Init.Mode = UART_MODE_TX_RX;
  uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&uart) != HAL_OK) APP_ErrorHandler();
  HAL_NVIC_SetPriority(USART2_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  if (HAL_UART_Receive_IT(&uart, &rx_byte, 1U) != HAL_OK) APP_ErrorHandler();
}

void ModuleUART_IRQHandler(void)
{
  HAL_UART_IRQHandler(&uart);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *handle)
{
  uint16_t next;
  if (handle != &uart) return;
  next = (rx_head + 1U) % RX_CAPACITY;
  if (next == rx_tail) {
    rx_lost = 1U;
  } else {
    rx_queue[rx_head] = rx_byte;
    __DMB();
    rx_head = next;
  }
  if (HAL_UART_Receive_IT(&uart, &rx_byte, 1U) != HAL_OK) rx_restart = 1U;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *handle)
{
  if (handle != &uart) return;
  rx_lost = 1U;
  rx_restart = 1U;
}

void ModuleUART_Process(void)
{
  uint32_t now = HAL_GetTick();
  if (rx_lost || rx_restart) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    /* A line with lost/corrupted bytes must never become a valid command. */
    rx_tail = rx_head;
    rx_lost = 0U;
    if (rx_restart) {
      (void)HAL_UART_AbortReceive(&uart);
      __HAL_UART_CLEAR_OREFLAG(&uart);
      rx_restart = HAL_UART_Receive_IT(&uart, &rx_byte, 1U) != HAL_OK;
    }
    __set_PRIMASK(mask);
    line_size = 0U;
    discard_line = 1U;
    Reply("ERR RX\r\n");
  }
  if (line_size != 0U && (uint32_t)(now - last_byte_ms) >= LINE_TIMEOUT_MS) {
    line_size = 0U;
    discard_line = 1U;
    Reply("ERR LINE_TIMEOUT\r\n");
  }
  while (rx_tail != rx_head) {
    uint8_t byte = rx_queue[rx_tail];
    rx_tail = (rx_tail + 1U) % RX_CAPACITY;
    last_byte_ms = now;
    if (byte == '\n') {
      if (discard_line) {
        discard_line = 0U;
      } else if (line_size != 0U) {
        ExecuteCommand();
      }
      line_size = 0U;
      /* Service the sensor before accepting another complete command. */
      return;
    }
    if (discard_line || byte == '\r') continue;
    if (byte < 0x20U || byte > 0x7eU || line_size >= LINE_CAPACITY - 1U) {
      discard_line = 1U;
      line_size = 0U;
      Reply("ERR LINE\r\n");
      return;
    }
    line[line_size++] = (char)byte;
  }
}
