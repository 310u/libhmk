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

#include "encoder.h"

#include "input_routing.h"
#include "keycodes.h"

#if ENCODER_NUM == 0

void encoder_init(void) {}

void encoder_task(void) {}

#else

#if defined(__has_include)
#if __has_include("at32f402_405.h")
#include "at32f402_405.h"
#define ENCODER_GPIO_BACKEND_AT32 1
#elif __has_include("stm32f4xx_hal.h")
#include "stm32f4xx_hal.h"
#define ENCODER_GPIO_BACKEND_STM32 1
#endif
#endif

#if !defined(ENCODER_GPIO_BACKEND_AT32) && !defined(ENCODER_GPIO_BACKEND_STM32)
#error "Unsupported GPIO backend for encoder"
#endif

#if defined(ENCODER_GPIO_BACKEND_AT32)
static gpio_type *encoder_a_ports[] = ENCODER_A_PORTS;
static const uint16_t encoder_a_pins[] = ENCODER_A_PINS;
static gpio_type *encoder_b_ports[] = ENCODER_B_PORTS;
static const uint16_t encoder_b_pins[] = ENCODER_B_PINS;
#elif defined(ENCODER_GPIO_BACKEND_STM32)
static GPIO_TypeDef *encoder_a_ports[] = ENCODER_A_PORTS;
static const uint16_t encoder_a_pins[] = ENCODER_A_PINS;
static GPIO_TypeDef *encoder_b_ports[] = ENCODER_B_PORTS;
static const uint16_t encoder_b_pins[] = ENCODER_B_PINS;
#endif

#if defined(ENCODER_CW_KEYS)
static const uint8_t encoder_cw_keys[] = ENCODER_CW_KEYS;
static const uint8_t encoder_ccw_keys[] = ENCODER_CCW_KEYS;
#else
static const uint8_t encoder_cw_keycodes[] = ENCODER_CW_KEYCODES;
static const uint8_t encoder_ccw_keycodes[] = ENCODER_CCW_KEYCODES;
#endif
static uint8_t encoder_states[ENCODER_NUM];
static int8_t encoder_accum[ENCODER_NUM];
static uint8_t encoder_queue[ENCODER_QUEUE_SIZE];
static uint8_t encoder_queue_head;
static uint8_t encoder_queue_size;
static bool encoder_release_pending;
static uint8_t encoder_release_keycode;

static const int8_t encoder_transition_table[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
};

_Static_assert(M_ARRAY_SIZE(encoder_a_ports) == ENCODER_NUM,
               "Invalid number of encoder A ports");
_Static_assert(M_ARRAY_SIZE(encoder_a_pins) == ENCODER_NUM,
               "Invalid number of encoder A pins");
_Static_assert(M_ARRAY_SIZE(encoder_b_ports) == ENCODER_NUM,
               "Invalid number of encoder B ports");
_Static_assert(M_ARRAY_SIZE(encoder_b_pins) == ENCODER_NUM,
               "Invalid number of encoder B pins");
#if defined(ENCODER_CW_KEYS)
_Static_assert(M_ARRAY_SIZE(encoder_cw_keys) == ENCODER_NUM,
               "Invalid number of encoder clockwise keys");
_Static_assert(M_ARRAY_SIZE(encoder_ccw_keys) == ENCODER_NUM,
               "Invalid number of encoder counterclockwise keys");
#else
_Static_assert(M_ARRAY_SIZE(encoder_cw_keycodes) == ENCODER_NUM,
               "Invalid number of encoder clockwise keycodes");
_Static_assert(M_ARRAY_SIZE(encoder_ccw_keycodes) == ENCODER_NUM,
               "Invalid number of encoder counterclockwise keycodes");
#endif

#if defined(ENCODER_GPIO_BACKEND_AT32)
static void encoder_enable_gpio_clock(gpio_type *port) {
  if (port == NULL) {
    return;
  }
#if defined(GPIOA)
  if (port == GPIOA) {
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOB)
  if (port == GPIOB) {
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOC)
  if (port == GPIOC) {
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOD)
  if (port == GPIOD) {
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOE)
  if (port == GPIOE) {
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOF)
  if (port == GPIOF) {
    crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
}

static void encoder_init_input(gpio_type *port, uint16_t pin) {
  gpio_init_type gpio_init_struct;

  encoder_enable_gpio_clock(port);

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = pin;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
#if defined(ENCODER_INPUT_PULLDOWN)
  gpio_init_struct.gpio_pull = GPIO_PULL_DOWN;
#elif defined(ENCODER_INPUT_NOPULL)
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
#else
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
#endif
  gpio_init(port, &gpio_init_struct);
}

static bool encoder_read_input(gpio_type *port, uint16_t pin) {
#if defined(ENCODER_INPUT_ACTIVE_HIGH)
  return gpio_input_data_bit_read(port, pin) != RESET;
#else
  return gpio_input_data_bit_read(port, pin) == RESET;
#endif
}
#elif defined(ENCODER_GPIO_BACKEND_STM32)
static void encoder_enable_gpio_clock(GPIO_TypeDef *port) {
  if (port == NULL) {
    return;
  }
#if defined(GPIOA)
  if (port == GPIOA) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOB)
  if (port == GPIOB) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOC)
  if (port == GPIOC) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOD)
  if (port == GPIOD) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOE)
  if (port == GPIOE) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOF)
  if (port == GPIOF) {
    __HAL_RCC_GPIOF_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOG)
  if (port == GPIOG) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOH)
  if (port == GPIOH) {
    __HAL_RCC_GPIOH_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOI)
  if (port == GPIOI) {
    __HAL_RCC_GPIOI_CLK_ENABLE();
    return;
  }
#endif
}

static void encoder_init_input(GPIO_TypeDef *port, uint16_t pin) {
  GPIO_InitTypeDef gpio_init = {0};

  encoder_enable_gpio_clock(port);

  gpio_init.Pin = pin;
  gpio_init.Mode = GPIO_MODE_INPUT;
#if defined(ENCODER_INPUT_PULLDOWN)
  gpio_init.Pull = GPIO_PULLDOWN;
#elif defined(ENCODER_INPUT_NOPULL)
  gpio_init.Pull = GPIO_NOPULL;
#else
  gpio_init.Pull = GPIO_PULLUP;
#endif
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(port, &gpio_init);
}

static bool encoder_read_input(GPIO_TypeDef *port, uint16_t pin) {
  const GPIO_PinState pin_state = HAL_GPIO_ReadPin(port, pin);

#if defined(ENCODER_INPUT_ACTIVE_HIGH)
  return pin_state == GPIO_PIN_SET;
#else
  return pin_state == GPIO_PIN_RESET;
#endif
}
#endif

static uint8_t encoder_queue_next_index(uint8_t index) {
  index++;
  if (index >= ENCODER_QUEUE_SIZE) {
    index = 0;
  }
  return index;
}

static void encoder_queue_push(uint8_t keycode) {
#if !defined(ENCODER_CW_KEYS)
  if (keycode == KC_NO) {
    return;
  }
#endif

  if (encoder_queue_size == ENCODER_QUEUE_SIZE) {
    encoder_queue_head = encoder_queue_next_index(encoder_queue_head);
    encoder_queue_size--;
  }

  uint8_t tail = encoder_queue_head;
  for (uint8_t i = 0; i < encoder_queue_size; i++) {
    tail = encoder_queue_next_index(tail);
  }

  encoder_queue[tail] = keycode;
  encoder_queue_size++;
}

static bool encoder_queue_pop(uint8_t *keycode) {
  if (encoder_queue_size == 0u) {
    return false;
  }

  *keycode = encoder_queue[encoder_queue_head];
  encoder_queue_head = encoder_queue_next_index(encoder_queue_head);
  encoder_queue_size--;
  return true;
}

static void encoder_output_press(uint8_t output) {
#if defined(ENCODER_CW_KEYS)
  (void)input_key_press(output);
#else
  input_keycode_press(output);
#endif
}

static void encoder_output_release(uint8_t output) {
#if defined(ENCODER_CW_KEYS)
  (void)input_key_release(output);
#else
  input_keycode_release(output);
#endif
}

static void encoder_start_next_tap_if_idle(void) {
  if (encoder_release_pending) {
    return;
  }

  uint8_t keycode = KC_NO;
  if (encoder_queue_pop(&keycode)) {
    encoder_output_press(keycode);
    encoder_release_pending = true;
    encoder_release_keycode = keycode;
  }
}

static uint8_t encoder_read_state(uint8_t index) {
  uint8_t state = 0u;

  if (encoder_read_input(encoder_a_ports[index], encoder_a_pins[index])) {
    state |= 0x01u;
  }
  if (encoder_read_input(encoder_b_ports[index], encoder_b_pins[index])) {
    state |= 0x02u;
  }

  return state;
}

void encoder_init(void) {
  memset(encoder_accum, 0, sizeof(encoder_accum));
  memset(encoder_queue, 0, sizeof(encoder_queue));
  encoder_queue_head = 0;
  encoder_queue_size = 0;
  encoder_release_pending = false;
  encoder_release_keycode = KC_NO;

  for (uint8_t i = 0; i < ENCODER_NUM; i++) {
    encoder_init_input(encoder_a_ports[i], encoder_a_pins[i]);
    encoder_init_input(encoder_b_ports[i], encoder_b_pins[i]);
    encoder_states[i] = encoder_read_state(i);
  }
}

void encoder_task(void) {
  if (encoder_release_pending) {
    encoder_output_release(encoder_release_keycode);
    encoder_release_pending = false;
    encoder_release_keycode = KC_NO;
  }

  for (uint8_t i = 0; i < ENCODER_NUM; i++) {
    const uint8_t current_state = encoder_read_state(i);
    const uint8_t transition = (uint8_t)((encoder_states[i] << 2) | current_state);
    const int8_t delta = encoder_transition_table[transition];

    encoder_states[i] = current_state;
    if (delta == 0) {
      continue;
    }

    encoder_accum[i] = (int8_t)(encoder_accum[i] + delta);

    while (encoder_accum[i] >= ENCODER_STEPS_PER_DETENT) {
#if defined(ENCODER_CW_KEYS)
      encoder_queue_push(encoder_cw_keys[i]);
#else
      encoder_queue_push(encoder_cw_keycodes[i]);
#endif
      encoder_accum[i] = (int8_t)(encoder_accum[i] - ENCODER_STEPS_PER_DETENT);
    }

    while (encoder_accum[i] <= -ENCODER_STEPS_PER_DETENT) {
#if defined(ENCODER_CW_KEYS)
      encoder_queue_push(encoder_ccw_keys[i]);
#else
      encoder_queue_push(encoder_ccw_keycodes[i]);
#endif
      encoder_accum[i] = (int8_t)(encoder_accum[i] + ENCODER_STEPS_PER_DETENT);
    }
  }

  // A task may release the previous detent above and start the next queued tap
  // below, keeping encoder latency bounded without collapsing a tap into an
  // immediate press+release pair.
  encoder_start_next_tap_if_idle();
}

#endif
