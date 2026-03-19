#pragma once

#include <stdint.h>

typedef struct {
  volatile uint32_t BSRR;
} GPIO_TypeDef;

typedef struct {
  uint32_t Pin;
  uint32_t Mode;
  uint32_t Pull;
  uint32_t Speed;
} GPIO_InitTypeDef;

#define GPIO_MODE_OUTPUT_PP 1u
#define GPIO_NOPULL 0u
#define GPIO_SPEED_FREQ_VERY_HIGH 3u

#define GPIO_PIN_8 (1u << 8)

extern GPIO_TypeDef mock_gpioa;
#define GPIOA (&mock_gpioa)

extern uint32_t mock_gpio_clock_mask;
#define __HAL_RCC_GPIOA_CLK_ENABLE() (mock_gpio_clock_mask |= 1u << 0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOD_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOE_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOF_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOG_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOH_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOI_CLK_ENABLE() ((void)0)

extern uint32_t mock_primask_value;
extern uint32_t mock_disable_irq_count;
extern uint32_t mock_enable_irq_count;

static inline uint32_t __get_PRIMASK(void) { return mock_primask_value; }

static inline void __disable_irq(void) { mock_disable_irq_count++; }

static inline void __enable_irq(void) { mock_enable_irq_count++; }

void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init);
