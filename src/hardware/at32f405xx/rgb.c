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

#include "at32f402_405.h"

#if !defined(RGB_DATA_PIN) || !defined(RGB_DATA_PORT)
#error "RGB_DATA_PIN and RGB_DATA_PORT must be defined for RGB support"
#endif

#if !defined(RGB_DATA_PIN_SOURCE) || !defined(RGB_DATA_PIN_MUX)
#error "RGB_DATA_PIN_SOURCE and RGB_DATA_PIN_MUX must be defined for RGB support"
#endif

#if !defined(RGB_TIMER) || !defined(RGB_TIMER_CLOCK) || !defined(RGB_TIMER_CHANNEL)
#error "RGB timer configuration macros must be defined for RGB support"
#endif

#if !defined(RGB_TIMER_DMA_REQUEST) || !defined(RGB_TIMER_DMAMUX_REQUEST)
#error "RGB DMA request configuration macros must be defined for RGB support"
#endif

#if !defined(RGB_DMA_CHANNEL) || !defined(RGB_DMA_MUX_CHANNEL)
#error "RGB DMA channel configuration macros must be defined for RGB support"
#endif

#if !defined(RGB_DMA_TRANSFER_FLAG) || !defined(RGB_DMA_CLEAR_FLAG)
#error "RGB DMA flag macros must be defined for RGB support"
#endif

#if defined(NUM_LEDS)
#else
#define NUM_LEDS NUM_KEYS
#endif

#if defined(RGB_USE_BITBANG_DRIVER)
#define RGB_BITBANG_PERIOD_TICKS                                                \
  ((uint32_t)(((uint64_t)F_CPU * 1250ULL + 999999999ULL) / 1000000000ULL))
#define RGB_BITBANG_HIGH_0_TICKS                                                \
  ((uint32_t)(((uint64_t)F_CPU * 350ULL + 999999999ULL) / 1000000000ULL))
#define RGB_BITBANG_HIGH_1_TICKS                                                \
  ((uint32_t)(((uint64_t)F_CPU * 700ULL + 999999999ULL) / 1000000000ULL))
#if !defined(RGB_BITBANG_FRAME_REPEATS)
#define RGB_BITBANG_FRAME_REPEATS 1u
#endif
#else
#define RGB_PWM_PERIOD_TICKS                                                     \
  ((uint32_t)(((uint64_t)F_CPU * 1250ULL + 999999999ULL) / 1000000000ULL))
#define RGB_PWM_HIGH_0_TICKS                                                     \
  ((uint32_t)(((uint64_t)F_CPU * 300ULL + 999999999ULL) / 1000000000ULL))
#define RGB_PWM_HIGH_1_TICKS                                                     \
  ((uint32_t)(((uint64_t)F_CPU * 600ULL + 999999999ULL) / 1000000000ULL))
#define RGB_FRAME_FLUSH_TICKS (RGB_PWM_PERIOD_TICKS * 3u)
#if !defined(RGB_DMA_FRAME_REPEATS)
#define RGB_DMA_FRAME_REPEATS 1u
#endif
#endif
#if !defined(RGB_RESET_TIME_NS)
#define RGB_RESET_TIME_NS 100000ULL
#endif
#define RGB_RESET_TIME_TICKS                                                     \
  ((uint32_t)(((uint64_t)F_CPU * RGB_RESET_TIME_NS + 999999999ULL) /             \
              1000000000ULL))

#if !defined(RGB_USE_BITBANG_DRIVER)
#define RGB_DMA_BUFFER_LEN (NUM_LEDS * 24u)

_Static_assert(RGB_DMA_BUFFER_LEN <= 65535u, "RGB DMA buffer exceeds DMA limit");

static uint32_t rgb_dma_buffer[RGB_DMA_BUFFER_LEN];
#endif
static bool rgb_driver_initialized = false;

static void rgb_enable_gpio_clock(void) {
  if (RGB_DATA_PORT == GPIOA) {
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
  } else if (RGB_DATA_PORT == GPIOB) {
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
  } else if (RGB_DATA_PORT == GPIOC) {
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
  }
}

static __attribute__((unused)) void rgb_wait_reset_period(void) {
  uint32_t start = board_cycle_count();
  while ((uint32_t)(board_cycle_count() - start) < RGB_RESET_TIME_TICKS) {
  }
}

#if defined(RGB_USE_BITBANG_DRIVER)
static void rgb_driver_gpio_init(void) {
  gpio_init_type gpio_init_struct;

  rgb_enable_gpio_clock();
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = RGB_DATA_PIN;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(RGB_DATA_PORT, &gpio_init_struct);
  gpio_bits_reset(RGB_DATA_PORT, RGB_DATA_PIN);
}

static inline void rgb_data_high(void) { RGB_DATA_PORT->scr = RGB_DATA_PIN; }

static inline void rgb_data_low(void) { RGB_DATA_PORT->clr = RGB_DATA_PIN; }

static inline void rgb_wait_until(uint32_t start, uint32_t ticks) {
  while ((uint32_t)(board_cycle_count() - start) < ticks) {
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
      rgb_write_bit((value & mask) ? RGB_BITBANG_HIGH_1_TICKS
                                   : RGB_BITBANG_HIGH_0_TICKS);
    }
  }
}
#else
static void rgb_driver_gpio_output_init(void) {
  gpio_init_type gpio_init_struct;

  rgb_enable_gpio_clock();
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = RGB_DATA_PIN;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(RGB_DATA_PORT, &gpio_init_struct);
  gpio_bits_reset(RGB_DATA_PORT, RGB_DATA_PIN);
}

static void rgb_driver_gpio_mux_init(void) {
  gpio_init_type gpio_init_struct;

  rgb_enable_gpio_clock();
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = RGB_DATA_PIN;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(RGB_DATA_PORT, &gpio_init_struct);
  gpio_pin_mux_config(RGB_DATA_PORT, RGB_DATA_PIN_SOURCE, RGB_DATA_PIN_MUX);
}

static void rgb_driver_gpio_init(void) {
  rgb_driver_gpio_output_init();
}

static void rgb_driver_dma_init(void) {
  dma_init_type dma_init_struct;

  crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);
  dmamux_enable(DMA1, TRUE);

  dma_reset(RGB_DMA_CHANNEL);
  dma_default_para_init(&dma_init_struct);
  dma_init_struct.peripheral_base_addr = (uint32_t)&RGB_TIMER->c3dt;
  dma_init_struct.memory_base_addr = (uint32_t)rgb_dma_buffer;
  dma_init_struct.direction = DMA_DIR_MEMORY_TO_PERIPHERAL;
  dma_init_struct.buffer_size = 1;
  dma_init_struct.peripheral_inc_enable = FALSE;
  dma_init_struct.memory_inc_enable = TRUE;
  dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_WORD;
  dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_WORD;
  dma_init_struct.loop_mode_enable = FALSE;
  dma_init_struct.priority = DMA_PRIORITY_VERY_HIGH;
  dma_init(RGB_DMA_CHANNEL, &dma_init_struct);
  dmamux_init(RGB_DMA_MUX_CHANNEL, RGB_TIMER_DMAMUX_REQUEST);
}

static void rgb_driver_timer_init(void) {
  tmr_output_config_type tmr_output_struct;

  crm_periph_clock_enable(RGB_TIMER_CLOCK, TRUE);
  tmr_reset(RGB_TIMER);

  tmr_base_init(RGB_TIMER, RGB_PWM_PERIOD_TICKS - 1u, 0);
  tmr_cnt_dir_set(RGB_TIMER, TMR_COUNT_UP);
  tmr_internal_clock_set(RGB_TIMER);
  tmr_period_buffer_enable(RGB_TIMER, TRUE);

  tmr_output_default_para_init(&tmr_output_struct);
  tmr_output_struct.oc_mode = TMR_OUTPUT_CONTROL_PWM_MODE_A;
  tmr_output_struct.oc_output_state = TRUE;
  tmr_output_struct.oc_polarity = TMR_OUTPUT_ACTIVE_HIGH;
  tmr_output_channel_config(RGB_TIMER, RGB_TIMER_CHANNEL, &tmr_output_struct);
  tmr_output_channel_buffer_enable(RGB_TIMER, RGB_TIMER_CHANNEL, TRUE);
  tmr_channel_dma_select(RGB_TIMER, TMR_DMA_REQUEST_BY_OVERFLOW);
  tmr_channel_value_set(RGB_TIMER, RGB_TIMER_CHANNEL, 0);
  tmr_event_sw_trigger(RGB_TIMER, TMR_OVERFLOW_SWTRIG);

  tmr_dma_request_enable(RGB_TIMER, RGB_TIMER_DMA_REQUEST, TRUE);
  tmr_output_enable(RGB_TIMER, TRUE);
}
#endif

void rgb_driver_init(void) {
  if (rgb_driver_initialized) {
    return;
  }

  rgb_driver_gpio_init();
#if !defined(RGB_USE_BITBANG_DRIVER)
  rgb_driver_dma_init();
  rgb_driver_timer_init();
#endif
  rgb_driver_initialized = true;
}

void rgb_driver_write(const uint8_t *grb_data, uint16_t byte_count) {
#if defined(RGB_USE_BITBANG_DRIVER)
  uint32_t primask;

  if (!rgb_driver_initialized) {
    rgb_driver_init();
  }

  rgb_data_low();
  rgb_wait_reset_period();

  if (byte_count == 0) {
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
#else
  uint16_t duty_index = 0;
  uint16_t total_slots;

  if (!rgb_driver_initialized) {
    rgb_driver_init();
  }

  if (byte_count == 0) {
    tmr_channel_value_set(RGB_TIMER, RGB_TIMER_CHANNEL, 0);
    tmr_event_sw_trigger(RGB_TIMER, TMR_OVERFLOW_SWTRIG);
    rgb_wait_reset_period();
    return;
  }

  for (uint16_t byte_index = 0; byte_index < byte_count; byte_index++) {
    uint8_t value = grb_data[byte_index];
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
      rgb_dma_buffer[duty_index++] =
          (value & mask) ? RGB_PWM_HIGH_1_TICKS : RGB_PWM_HIGH_0_TICKS;
    }
  }

  total_slots = duty_index;

  rgb_driver_gpio_output_init();
  rgb_wait_reset_period();

  for (uint32_t repeat = 0; repeat < RGB_DMA_FRAME_REPEATS; repeat++) {
    dma_channel_enable(RGB_DMA_CHANNEL, FALSE);
    dma_flag_clear(RGB_DMA_CLEAR_FLAG);

    tmr_counter_enable(RGB_TIMER, FALSE);
    tmr_counter_value_set(RGB_TIMER, 0);
    tmr_channel_value_set(RGB_TIMER, RGB_TIMER_CHANNEL, 0);
    tmr_event_sw_trigger(RGB_TIMER, TMR_OVERFLOW_SWTRIG);

    rgb_driver_gpio_mux_init();

    if (total_slots > 0u) {
      RGB_DMA_CHANNEL->maddr = (uint32_t)rgb_dma_buffer;
      dma_data_number_set(RGB_DMA_CHANNEL, total_slots);
      dma_channel_enable(RGB_DMA_CHANNEL, TRUE);
    }

    tmr_counter_enable(RGB_TIMER, TRUE);

    if (total_slots > 0u) {
      while (dma_interrupt_flag_get(RGB_DMA_TRANSFER_FLAG) == RESET) {
      }
    }

    {
      uint32_t start = board_cycle_count();
      while ((uint32_t)(board_cycle_count() - start) < RGB_FRAME_FLUSH_TICKS) {
      }
    }

    dma_channel_enable(RGB_DMA_CHANNEL, FALSE);
    dma_flag_clear(RGB_DMA_CLEAR_FLAG);

    tmr_counter_enable(RGB_TIMER, FALSE);
    tmr_counter_value_set(RGB_TIMER, 0);
    tmr_channel_value_set(RGB_TIMER, RGB_TIMER_CHANNEL, 0);
    tmr_event_sw_trigger(RGB_TIMER, TMR_OVERFLOW_SWTRIG);

    rgb_driver_gpio_output_init();
    rgb_wait_reset_period();
  }
#endif
}

#endif
