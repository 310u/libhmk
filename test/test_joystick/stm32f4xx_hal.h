#pragma once

#include <stdint.h>

typedef struct {
  uint32_t Pin;
  uint32_t Mode;
  uint32_t Pull;
  uint32_t Speed;
} GPIO_InitTypeDef;

typedef struct GPIO_TypeDef GPIO_TypeDef;
struct GPIO_TypeDef {
  uint32_t unused;
};

typedef enum {
  GPIO_PIN_RESET = 0,
  GPIO_PIN_SET = 1,
} GPIO_PinState;

#define GPIO_MODE_INPUT 0u
#define GPIO_PULLUP 1u
#define GPIO_SPEED_FREQ_VERY_HIGH 3u

#define GPIO_PIN_0 0x0001u

extern GPIO_TypeDef *const GPIOA;

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
