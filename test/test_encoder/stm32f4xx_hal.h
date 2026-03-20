#pragma once

#include <stdint.h>

typedef struct {
  uint32_t id;
} GPIO_TypeDef;

extern GPIO_TypeDef mock_gpioa;
extern GPIO_TypeDef mock_gpiob;
extern GPIO_TypeDef mock_gpioc;

#define GPIOA (&mock_gpioa)
#define GPIOB (&mock_gpiob)
#define GPIOC (&mock_gpioc)

#define GPIO_PIN_0 0x0001u
#define GPIO_PIN_1 0x0002u

typedef enum {
  GPIO_PIN_RESET = 0u,
  GPIO_PIN_SET = 1u,
} GPIO_PinState;

typedef struct {
  uint32_t Pin;
  uint32_t Mode;
  uint32_t Pull;
  uint32_t Speed;
} GPIO_InitTypeDef;

#define GPIO_MODE_INPUT 0u
#define GPIO_PULLUP 1u
#define GPIO_PULLDOWN 2u
#define GPIO_NOPULL 3u
#define GPIO_SPEED_FREQ_VERY_HIGH 0u

#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
