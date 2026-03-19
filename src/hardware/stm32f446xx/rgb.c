/*
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "hardware/rgb_api.h"

#if defined(RGB_ENABLED)

#include "hardware/hardware.h"

#include "stm32f4xx_hal.h"

#if !defined(RGB_DATA_PIN) || !defined(RGB_DATA_PORT)
#error "RGB_DATA_PIN and RGB_DATA_PORT must be defined for RGB support"
#endif

#if defined(NUM_LEDS)
#else
#define NUM_LEDS NUM_KEYS
#endif

#define RGB_BITBANG_PERIOD_TICKS                                                \
  ((uint32_t)(((uint64_t)F_CPU * 1250ULL + 999999999ULL) / 1000000000ULL))
#define RGB_BITBANG_HIGH_0_TICKS                                                \
  ((uint32_t)(((uint64_t)F_CPU * 350ULL + 999999999ULL) / 1000000000ULL))
#define RGB_BITBANG_HIGH_1_TICKS                                                \
  ((uint32_t)(((uint64_t)F_CPU * 700ULL + 999999999ULL) / 1000000000ULL))

#if !defined(RGB_BITBANG_FRAME_REPEATS)
#define RGB_BITBANG_FRAME_REPEATS 1u
#endif

#if !defined(RGB_RESET_TIME_NS)
#define RGB_RESET_TIME_NS 100000ULL
#endif

#define RGB_RESET_TIME_TICKS                                                    \
  ((uint32_t)(((uint64_t)F_CPU * RGB_RESET_TIME_NS + 999999999ULL) /            \
              1000000000ULL))

static bool rgb_driver_initialized = false;

static void rgb_enable_gpio_clock(void) {
#if defined(GPIOA)
  if (RGB_DATA_PORT == GPIOA) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOB)
  if (RGB_DATA_PORT == GPIOB) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOC)
  if (RGB_DATA_PORT == GPIOC) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOD)
  if (RGB_DATA_PORT == GPIOD) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOE)
  if (RGB_DATA_PORT == GPIOE) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOF)
  if (RGB_DATA_PORT == GPIOF) {
    __HAL_RCC_GPIOF_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOG)
  if (RGB_DATA_PORT == GPIOG) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOH)
  if (RGB_DATA_PORT == GPIOH) {
    __HAL_RCC_GPIOH_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOI)
  if (RGB_DATA_PORT == GPIOI) {
    __HAL_RCC_GPIOI_CLK_ENABLE();
    return;
  }
#endif
}

static void rgb_driver_gpio_init(void) {
  GPIO_InitTypeDef gpio_init_struct = {0};

  rgb_enable_gpio_clock();
  gpio_init_struct.Pin = RGB_DATA_PIN;
  gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init_struct.Pull = GPIO_NOPULL;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(RGB_DATA_PORT, &gpio_init_struct);
  RGB_DATA_PORT->BSRR = (uint32_t)RGB_DATA_PIN << 16;
}

static inline void rgb_data_high(void) { RGB_DATA_PORT->BSRR = RGB_DATA_PIN; }

static inline void rgb_data_low(void) {
  RGB_DATA_PORT->BSRR = (uint32_t)RGB_DATA_PIN << 16;
}

static inline void rgb_wait_until(uint32_t start, uint32_t ticks) {
  while ((uint32_t)(board_cycle_count() - start) < ticks) {
  }
}

static void rgb_wait_reset_period(void) {
  uint32_t start = board_cycle_count();
  while ((uint32_t)(board_cycle_count() - start) < RGB_RESET_TIME_TICKS) {
  }
}

static inline void rgb_write_bit(uint32_t high_ticks) {
  uint32_t start = board_cycle_count();
  rgb_data_high();
  rgb_wait_until(start, high_ticks);
  rgb_data_low();
  rgb_wait_until(start, RGB_BITBANG_PERIOD_TICKS);
}

static void rgb_driver_write_frame(const uint8_t *grb_data, uint16_t byte_count) {
  for (uint16_t byte_index = 0; byte_index < byte_count; byte_index++) {
    uint8_t value = grb_data[byte_index];
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
      rgb_write_bit((value & mask) != 0u ? RGB_BITBANG_HIGH_1_TICKS
                                         : RGB_BITBANG_HIGH_0_TICKS);
    }
  }
}

void rgb_driver_init(void) {
  if (rgb_driver_initialized) {
    return;
  }

  rgb_driver_gpio_init();
  rgb_driver_initialized = true;
}

void rgb_driver_task(void) {}

void rgb_driver_write(const uint8_t *grb_data, uint16_t byte_count) {
  uint32_t primask;

  if (!rgb_driver_initialized) {
    rgb_driver_init();
  }

  rgb_data_low();
  rgb_wait_reset_period();

  if (byte_count == 0u) {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();

  for (uint32_t repeat = 0; repeat < RGB_BITBANG_FRAME_REPEATS; repeat++) {
    rgb_driver_write_frame(grb_data, byte_count);
    rgb_data_low();
    rgb_wait_reset_period();
  }

  rgb_data_low();
  if (primask == 0u) {
    __enable_irq();
  }
  rgb_wait_reset_period();
}

#endif
