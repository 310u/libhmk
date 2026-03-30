#include <math.h>

#include "eeconfig.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "input_routing.h"
#include "joystick.h"
#include "joystick_math.h"
#include "keycodes.h"
#include "lib/usqrt.h"
#include "wear_leveling.h"

#if defined(JOYSTICK_ENABLED)

#if defined(__has_include)
#if __has_include("at32f402_405.h")
#include "at32f402_405.h"
#define JOYSTICK_GPIO_BACKEND_AT32 1
#elif __has_include("stm32f4xx_hal.h")
#include "stm32f4xx_hal.h"
#define JOYSTICK_GPIO_BACKEND_STM32 1
#endif
#endif

#if !defined(JOYSTICK_GPIO_BACKEND_AT32) && !defined(JOYSTICK_GPIO_BACKEND_STM32)
#error "Unsupported GPIO backend for joystick"
#endif

#ifndef JOYSTICK_X_ADC_INDEX
#error "JOYSTICK_X_ADC_INDEX not defined in board_def.h"
#endif

#ifndef JOYSTICK_Y_ADC_INDEX
#error "JOYSTICK_Y_ADC_INDEX not defined in board_def.h"
#endif

#ifndef JOYSTICK_SW_PIN
#error "JOYSTICK_SW_PIN not defined in board_def.h"
#endif

#ifndef JOYSTICK_SW_PORT
#error "JOYSTICK_SW_PORT not defined in board_def.h"
#endif

#ifndef JOYSTICK_SMOOTHING_SLOW_EXPONENT
#define JOYSTICK_SMOOTHING_SLOW_EXPONENT 4u
#endif

#ifndef JOYSTICK_SMOOTHING_FAST_EXPONENT
#define JOYSTICK_SMOOTHING_FAST_EXPONENT 2u
#endif

#ifndef JOYSTICK_SMOOTHING_FAST_DELTA
#define JOYSTICK_SMOOTHING_FAST_DELTA 24u
#endif

#ifndef JOYSTICK_MOUSE_REPORT_INTERVAL_MS
#define JOYSTICK_MOUSE_REPORT_INTERVAL_MS 1u
#endif

#ifndef JOYSTICK_SCROLL_REPORT_INTERVAL_MS
#define JOYSTICK_SCROLL_REPORT_INTERVAL_MS 8u
#endif

#ifndef JOYSTICK_CURSOR_THRESHOLD
#define JOYSTICK_CURSOR_THRESHOLD 48u
#endif

#define JOYSTICK_MOUSE_FP_SHIFT 8
#define JOYSTICK_MOUSE_FP_ONE (1L << JOYSTICK_MOUSE_FP_SHIFT)
#define JOYSTICK_MOUSE_DIVISOR 50L
#define JOYSTICK_SCROLL_DIVISOR 250L
#define JOYSTICK_VECTOR_MAX 181u
#define JOYSTICK_OUTPUT_FP_SHIFT 8
#define JOYSTICK_OUTPUT_FP_ONE (1L << JOYSTICK_OUTPUT_FP_SHIFT)

static joystick_state_t current_state = {0};
static joystick_config_t config_cache = {0};
static uint16_t filtered_x = 0;
static uint16_t filtered_y = 0;

// Debounce state for push switch
static bool sw_raw = false;
static uint32_t sw_last_change_tick = 0;
static bool sw_debounced = false;
static uint32_t last_mouse_tick = 0;
static int32_t mouse_accum_x = 0;
static int32_t mouse_accum_y = 0;
static int32_t scroll_accum_x = 0;
static int32_t scroll_accum_y = 0;
static bool mouse_switch_reported = false;
static uint8_t cursor_key_mask_reported = 0;

enum {
  JOYSTICK_CURSOR_LEFT = 1u << 0,
  JOYSTICK_CURSOR_RIGHT = 1u << 1,
  JOYSTICK_CURSOR_UP = 1u << 2,
  JOYSTICK_CURSOR_DOWN = 1u << 3,
};

static bool joystick_sw_sends_mouse_button(void) {
#if defined(JOYSTICK_SW_KEY_INDEX)
  // Boards that expose the push switch as a normal key should let the keymap
  // decide its behavior instead of also forcing a mouse click.
  return false;
#else
  return true;
#endif
}

static bool joystick_mode_is_cursor(uint8_t mode) {
  return mode == JOYSTICK_MODE_CURSOR_4 || mode == JOYSTICK_MODE_CURSOR_8;
}

static bool joystick_mode_is_gamepad(uint8_t mode) {
  return mode == JOYSTICK_MODE_XINPUT_LS || mode == JOYSTICK_MODE_XINPUT_RS;
}

static uint8_t joystick_abs_i8(int8_t value) {
  return value < 0 ? (uint8_t)(-value) : (uint8_t)value;
}

static void joystick_cursor_set_key(bool active, uint8_t keycode) {
  if (active) {
    input_keyboard_press(keycode);
  } else {
    input_keyboard_release(keycode);
  }
}

static void joystick_cursor_update_keys(uint8_t next_mask) {
  const uint8_t changed = cursor_key_mask_reported ^ next_mask;

  if (changed & JOYSTICK_CURSOR_LEFT) {
    joystick_cursor_set_key((next_mask & JOYSTICK_CURSOR_LEFT) != 0u, KC_LEFT);
  }
  if (changed & JOYSTICK_CURSOR_RIGHT) {
    joystick_cursor_set_key((next_mask & JOYSTICK_CURSOR_RIGHT) != 0u, KC_RIGHT);
  }
  if (changed & JOYSTICK_CURSOR_UP) {
    joystick_cursor_set_key((next_mask & JOYSTICK_CURSOR_UP) != 0u, KC_UP);
  }
  if (changed & JOYSTICK_CURSOR_DOWN) {
    joystick_cursor_set_key((next_mask & JOYSTICK_CURSOR_DOWN) != 0u, KC_DOWN);
  }

  cursor_key_mask_reported = next_mask;
}

static uint8_t joystick_cursor_compute_mask(int8_t x, int8_t y, bool allow_diagonal) {
  const uint8_t abs_x = joystick_abs_i8(x);
  const uint8_t abs_y = joystick_abs_i8(y);

  if (allow_diagonal) {
    uint8_t mask = 0u;

    if (abs_x >= JOYSTICK_CURSOR_THRESHOLD) {
      mask |= x < 0 ? JOYSTICK_CURSOR_LEFT : JOYSTICK_CURSOR_RIGHT;
    }
    if (abs_y >= JOYSTICK_CURSOR_THRESHOLD) {
      mask |= y > 0 ? JOYSTICK_CURSOR_UP : JOYSTICK_CURSOR_DOWN;
    }

    return mask;
  }

  if (abs_x < JOYSTICK_CURSOR_THRESHOLD && abs_y < JOYSTICK_CURSOR_THRESHOLD) {
    return 0u;
  }

  if (abs_x >= abs_y) {
    return x < 0 ? JOYSTICK_CURSOR_LEFT : JOYSTICK_CURSOR_RIGHT;
  }

  return y > 0 ? JOYSTICK_CURSOR_UP : JOYSTICK_CURSOR_DOWN;
}

static joystick_config_t joystick_default_config(void) {
  joystick_config_t def;
  joystick_init_default_config(&def);
  def.mouse_acceleration = JOYSTICK_MOUSE_ACCELERATION_DEFAULT;
  return def;
}

static uint8_t joystick_sanitize_mouse_speed(uint8_t raw) {
  return raw == 0u ? JOYSTICK_MOUSE_SPEED_DEFAULT : raw;
}

static uint8_t joystick_effective_mouse_acceleration(uint8_t raw) {
  return raw == 0u ? JOYSTICK_MOUSE_ACCELERATION_DEFAULT : raw;
}

static joystick_mouse_preset_t joystick_make_mouse_preset(uint8_t mouse_speed,
                                                          uint8_t mouse_acceleration) {
  return (joystick_mouse_preset_t){
      .mouse_speed = joystick_sanitize_mouse_speed(mouse_speed),
      .mouse_acceleration =
          joystick_effective_mouse_acceleration(mouse_acceleration),
  };
}

static bool joystick_config_has_explicit_mouse_presets(
    const joystick_config_t *config) {
  for (uint8_t i = 0; i < JOYSTICK_MOUSE_PRESET_COUNT; i++) {
    if (config->mouse_presets[i].mouse_speed != 0u ||
        config->mouse_presets[i].mouse_acceleration != 0u) {
      return true;
    }
  }

  return false;
}

static uint16_t joystick_abs_diff_u16(uint16_t a, uint16_t b) {
  return a >= b ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static uint16_t joystick_ema(uint16_t old_val, uint16_t new_val,
                             uint8_t exponent) {
  if (old_val == 0 || exponent == 0u)
    return new_val;

  const uint32_t weight = (1u << exponent) - 1u;
  return (uint16_t)(((uint32_t)old_val * weight + new_val) >> exponent);
}

static uint16_t joystick_filter_adc(uint16_t filtered_val, uint16_t raw_val) {
  if (joystick_mode_is_gamepad(config_cache.mode)) {
    return raw_val;
  }

  uint8_t exponent = JOYSTICK_SMOOTHING_SLOW_EXPONENT;

  if (joystick_abs_diff_u16(filtered_val, raw_val) >= JOYSTICK_SMOOTHING_FAST_DELTA)
    exponent = JOYSTICK_SMOOTHING_FAST_EXPONENT;

  return joystick_ema(filtered_val, raw_val, exponent);
}

static int32_t joystick_apply_calibration_fp(
    uint16_t raw_val, joystick_axis_calibration_t *cal) {
  // 0 = min, 2048 = center, 4095 = max
  int32_t val = raw_val;
  int32_t center = cal->center;
  int32_t min = cal->min;
  int32_t max = cal->max;

  // Failsafe in case of uncalibrated EEPROM
  if (center == 0 || max == 0) {
    center = 2048;
    min = 0;
    max = 4095;
  }

  int32_t dist_from_center = val - center;

  int32_t result = 0;
  if (dist_from_center > 0) {
    // Positive side
    int32_t range = max - center;
    if (range <= 0)
      range = 1; // Failsafe

    result =
        (dist_from_center * 127 * JOYSTICK_OUTPUT_FP_ONE) / range;
    if (result > (127 * JOYSTICK_OUTPUT_FP_ONE))
      result = 127 * JOYSTICK_OUTPUT_FP_ONE;
  } else {
    // Negative side
    int32_t range = center - min;
    if (range <= 0)
      range = 1; // Failsafe

    result =
        (dist_from_center * 128 * JOYSTICK_OUTPUT_FP_ONE) / range;
    if (result < (-128 * JOYSTICK_OUTPUT_FP_ONE))
      result = -128 * JOYSTICK_OUTPUT_FP_ONE;
  }

  return result;
}

static uint16_t joystick_vector_length(int8_t x, int8_t y) {
  const int32_t x32 = x;
  const int32_t y32 = y;
  return usqrt16((uint16_t)(x32 * x32 + y32 * y32));
}

static int8_t joystick_clamp_i16_to_i8(int16_t value) {
  if (value > INT8_MAX) {
    return INT8_MAX;
  }
  if (value < INT8_MIN) {
    return INT8_MIN;
  }
  return (int8_t)value;
}

static int8_t joystick_fp_to_i8(int32_t value_fp) {
  return joystick_clamp_i16_to_i8(
      (int16_t)lroundf((float)value_fp / (float)JOYSTICK_OUTPUT_FP_ONE));
}

static int32_t joystick_vector_delta_fp(uint16_t magnitude, uint8_t speed,
                                        uint8_t acceleration, int32_t divisor) {
  const int64_t max_sq =
      (int64_t)JOYSTICK_VECTOR_MAX * (int64_t)JOYSTICK_VECTOR_MAX;
  const int64_t mag_sq = (int64_t)magnitude * (int64_t)magnitude;
  const int64_t curve_term = ((int64_t)(255u - acceleration) * max_sq +
                              (int64_t)acceleration * mag_sq) /
                             255LL;
  int64_t numerator = (int64_t)magnitude * curve_term;
  numerator *= (int64_t)speed;
  numerator *= (int64_t)JOYSTICK_MOUSE_FP_ONE;

  int64_t denominator = 16384LL * (int64_t)divisor;
  int64_t delta_fp = numerator / denominator;

  if (delta_fp > INT32_MAX) {
    return INT32_MAX;
  }
  return (int32_t)delta_fp;
}

static int8_t joystick_consume_mouse_accum(int32_t *accum) {
  int32_t whole = *accum / JOYSTICK_MOUSE_FP_ONE;

  if (whole > INT8_MAX) {
    whole = INT8_MAX;
  } else if (whole < INT8_MIN) {
    whole = INT8_MIN;
  }

  *accum -= whole * JOYSTICK_MOUSE_FP_ONE;
  return (int8_t)whole;
}

joystick_config_t joystick_normalize_config(joystick_config_t config) {
  config.mouse_speed = joystick_sanitize_mouse_speed(config.mouse_speed);
  config.mouse_acceleration =
      joystick_effective_mouse_acceleration(config.mouse_acceleration);

  if (config.active_mouse_preset >= JOYSTICK_MOUSE_PRESET_COUNT) {
    config.active_mouse_preset = 0u;
  }

  const joystick_mouse_preset_t fallback =
      joystick_make_mouse_preset(config.mouse_speed, config.mouse_acceleration);
  const bool has_explicit_presets =
      joystick_config_has_explicit_mouse_presets(&config);

  if (!has_explicit_presets) {
    joystick_fill_default_mouse_presets(config.mouse_presets,
                                        fallback.mouse_speed,
                                        fallback.mouse_acceleration);
  } else {
    for (uint8_t i = 0; i < JOYSTICK_MOUSE_PRESET_COUNT; i++) {
      if (config.mouse_presets[i].mouse_speed == 0u) {
        config.mouse_presets[i].mouse_speed = fallback.mouse_speed;
      }
      if (config.mouse_presets[i].mouse_acceleration == 0u) {
        config.mouse_presets[i].mouse_acceleration = fallback.mouse_acceleration;
      }
    }
  }

  joystick_mouse_preset_t *active_preset =
      &config.mouse_presets[config.active_mouse_preset];
  if (config.mouse_speed != active_preset->mouse_speed ||
      config.mouse_acceleration != active_preset->mouse_acceleration) {
    *active_preset = fallback;
  }

  config.mouse_speed = active_preset->mouse_speed;
  config.mouse_acceleration = active_preset->mouse_acceleration;

  return config;
}

void joystick_select_mouse_preset(joystick_config_t *config,
                                  uint8_t preset_index) {
  if (config == NULL) {
    return;
  }

  *config = joystick_normalize_config(*config);
  config->active_mouse_preset = preset_index % JOYSTICK_MOUSE_PRESET_COUNT;
  config->mouse_speed = config->mouse_presets[config->active_mouse_preset].mouse_speed;
  config->mouse_acceleration =
      config->mouse_presets[config->active_mouse_preset].mouse_acceleration;
}

static bool joystick_switch_pressed(void);

static void joystick_release_mouse_buttons(void) {
  if (mouse_switch_reported) {
    hid_mouse_move(0, 0, 0);
    mouse_switch_reported = false;
  }
}

static void joystick_release_cursor_keys(void) {
  if (cursor_key_mask_reported != 0u) {
    joystick_cursor_update_keys(0u);
  }
}

static void joystick_reset_signal_state(void) {
  current_state.raw_x = 0;
  current_state.raw_y = 0;
  current_state.out_x = 0;
  current_state.out_y = 0;
  current_state.sw = false;
  current_state.calibrated_x = 0;
  current_state.calibrated_y = 0;
  current_state.corrected_x = 0;
  current_state.corrected_y = 0;
  filtered_x = 0;
  filtered_y = 0;
  sw_raw = false;
  sw_debounced = false;
  sw_last_change_tick = 0;
}

static void joystick_reset_output_state(void) {
  last_mouse_tick = timer_read();
  mouse_accum_x = 0;
  mouse_accum_y = 0;
  scroll_accum_x = 0;
  scroll_accum_y = 0;
  mouse_switch_reported = false;
  cursor_key_mask_reported = 0;
}

static void joystick_update_switch_state(void) {
  const bool sw_current = joystick_switch_pressed();
  if (sw_current != sw_raw) {
    sw_raw = sw_current;
    sw_last_change_tick = timer_read();
  }

  if (config_cache.sw_debounce_ms == 0 ||
      timer_elapsed(sw_last_change_tick) >=
          (uint32_t)config_cache.sw_debounce_ms) {
    sw_debounced = sw_raw;
  }

  current_state.sw = sw_debounced;
}

static void joystick_update_signal_state(void) {
  const uint16_t x_raw = analog_read_raw(JOYSTICK_X_ADC_INDEX);
  const uint16_t y_raw = analog_read_raw(JOYSTICK_Y_ADC_INDEX);
  int32_t calibrated_x_fp = 0;
  int32_t calibrated_y_fp = 0;
  int32_t corrected_x_fp = 0;
  int32_t corrected_y_fp = 0;
  int32_t out_x_fp = 0;
  int32_t out_y_fp = 0;

  current_state.raw_x = x_raw;
  current_state.raw_y = y_raw;
  filtered_x = joystick_filter_adc(filtered_x, x_raw);
  filtered_y = joystick_filter_adc(filtered_y, y_raw);

  joystick_update_switch_state();

  calibrated_x_fp = joystick_apply_calibration_fp(filtered_x, &config_cache.x);
  calibrated_y_fp = joystick_apply_calibration_fp(filtered_y, &config_cache.y);
  current_state.calibrated_x = joystick_fp_to_i8(calibrated_x_fp);
  current_state.calibrated_y = joystick_fp_to_i8(calibrated_y_fp);

  corrected_x_fp = calibrated_x_fp;
  corrected_y_fp = calibrated_y_fp;
  joystick_apply_circular_correction_fp(config_cache.radial_boundaries,
                                        &corrected_x_fp, &corrected_y_fp);
  current_state.corrected_x = joystick_fp_to_i8(corrected_x_fp);
  current_state.corrected_y = joystick_fp_to_i8(corrected_y_fp);

  out_x_fp = corrected_x_fp;
  out_y_fp = corrected_y_fp;
  joystick_apply_radial_deadzone_fp(&out_x_fp, &out_y_fp, config_cache.deadzone);
  current_state.out_x = joystick_fp_to_i8(out_x_fp);
  current_state.out_y = joystick_fp_to_i8(out_y_fp);
}

static void joystick_apply_sniper_scaling(int32_t *dx_fp, int32_t *dy_fp) {
  if (is_sniper_active) {
    *dx_fp = (*dx_fp * eeconfig->options.sniper_mode_multiplier) / 255;
    *dy_fp = (*dy_fp * eeconfig->options.sniper_mode_multiplier) / 255;
  }
}

static void joystick_compute_pointer_delta(int32_t *dx_fp, int32_t *dy_fp,
                                           uint8_t acceleration,
                                           int32_t divisor) {
  const uint16_t magnitude =
      joystick_vector_length(current_state.out_x, current_state.out_y);
  *dx_fp = 0;
  *dy_fp = 0;

  if (magnitude == 0u) {
    return;
  }

  const int32_t delta_fp = joystick_vector_delta_fp(
      magnitude, config_cache.mouse_speed, acceleration, divisor);
  *dx_fp = ((int32_t)current_state.out_x * delta_fp) / (int32_t)magnitude;
  *dy_fp = ((int32_t)current_state.out_y * delta_fp) / (int32_t)magnitude;
  joystick_apply_sniper_scaling(dx_fp, dy_fp);
}

static bool joystick_pointer_output_active(bool sw_mouse_button) {
  return current_state.out_x != 0 || current_state.out_y != 0 ||
         sw_mouse_button || mouse_switch_reported;
}

static void joystick_task_mouse_mode(uint32_t tick) {
  if (timer_elapsed(last_mouse_tick) < JOYSTICK_MOUSE_REPORT_INTERVAL_MS) {
    return;
  }

  const bool sw_mouse_button = joystick_sw_sends_mouse_button() && current_state.sw;
  if (joystick_pointer_output_active(sw_mouse_button)) {
    const uint8_t acceleration =
        joystick_effective_mouse_acceleration(config_cache.mouse_acceleration);
    int32_t dx_fp = 0;
    int32_t dy_fp = 0;
    joystick_compute_pointer_delta(&dx_fp, &dy_fp, acceleration,
                                   JOYSTICK_MOUSE_DIVISOR);

    mouse_accum_x += dx_fp;
    mouse_accum_y -= dy_fp;

    const int8_t dx = joystick_consume_mouse_accum(&mouse_accum_x);
    const int8_t dy = joystick_consume_mouse_accum(&mouse_accum_y);

    uint8_t buttons = 0;
    if (sw_mouse_button)
      buttons |= 1u;

    hid_mouse_move(dx, dy, buttons);
    mouse_switch_reported = buttons != 0u;
  }

  last_mouse_tick = tick;
}

static void joystick_task_scroll_mode(uint32_t tick) {
  if (timer_elapsed(last_mouse_tick) < JOYSTICK_SCROLL_REPORT_INTERVAL_MS) {
    return;
  }

  const bool sw_mouse_button = joystick_sw_sends_mouse_button() && current_state.sw;
  if (joystick_pointer_output_active(sw_mouse_button)) {
    int32_t dx_fp = 0;
    int32_t dy_fp = 0;
    joystick_compute_pointer_delta(&dx_fp, &dy_fp,
                                   JOYSTICK_MOUSE_ACCELERATION_DEFAULT,
                                   JOYSTICK_SCROLL_DIVISOR);

    scroll_accum_x += dx_fp;
    scroll_accum_y += dy_fp;

    const int8_t pan = joystick_consume_mouse_accum(&scroll_accum_x);
    const int8_t wheel = joystick_consume_mouse_accum(&scroll_accum_y);

    uint8_t buttons = 0;
    if (sw_mouse_button)
      buttons |= 1u;

    hid_mouse_scroll(wheel, pan, buttons);
    mouse_switch_reported = buttons != 0u;
  }

  last_mouse_tick = tick;
}

static void joystick_task_cursor_mode(void) {
  const uint8_t next_mask =
      joystick_cursor_compute_mask(current_state.out_x, current_state.out_y,
                                   config_cache.mode == JOYSTICK_MODE_CURSOR_8);
  joystick_cursor_update_keys(next_mask);
}

static void joystick_init_switch_pin(void) {
#if defined(JOYSTICK_GPIO_BACKEND_AT32)
  gpio_init_type gpio_init_struct;

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = JOYSTICK_SW_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(JOYSTICK_SW_PORT, &gpio_init_struct);
#elif defined(JOYSTICK_GPIO_BACKEND_STM32)
  GPIO_InitTypeDef gpio_init_struct = {0};

  gpio_init_struct.Pin = JOYSTICK_SW_PIN;
  gpio_init_struct.Mode = GPIO_MODE_INPUT;
  gpio_init_struct.Pull = GPIO_PULLUP;
  gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(JOYSTICK_SW_PORT, &gpio_init_struct);
#endif
}

static bool joystick_switch_pressed(void) {
#if defined(JOYSTICK_GPIO_BACKEND_AT32)
  return gpio_input_data_bit_read(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN) == RESET;
#elif defined(JOYSTICK_GPIO_BACKEND_STM32)
  return HAL_GPIO_ReadPin(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN) == GPIO_PIN_RESET;
#endif
}

void joystick_init(void) {
  joystick_config_t persisted_config;
  joystick_init_switch_pin();

  // Initial load of config cache
  if (eeconfig != NULL) {
    memcpy(&persisted_config, &CURRENT_PROFILE.joystick_config,
           sizeof(persisted_config));
    config_cache = joystick_normalize_config(persisted_config);
  } else {
    config_cache = joystick_default_config();
  }
  joystick_reset_signal_state();
  joystick_reset_output_state();
}

// Global state getters/setters
joystick_config_t joystick_get_config(void) { return config_cache; }

void joystick_apply_config(joystick_config_t config) {
  const uint8_t prev_mode = config_cache.mode;
  config_cache = joystick_normalize_config(config);

  if ((prev_mode == JOYSTICK_MODE_MOUSE || prev_mode == JOYSTICK_MODE_SCROLL) &&
      prev_mode != config_cache.mode && mouse_switch_reported) {
    joystick_release_mouse_buttons();
  }

  if (joystick_mode_is_cursor(prev_mode) && prev_mode != config_cache.mode &&
      cursor_key_mask_reported != 0u) {
    joystick_release_cursor_keys();
  }
}

void joystick_set_config(joystick_config_t config) {
  config = joystick_normalize_config(config);
  if (eeconfig != NULL) {
    uint32_t addr = offsetof(eeconfig_t, profiles) +
                    eeconfig->current_profile * sizeof(eeconfig_profile_t) +
                    offsetof(eeconfig_profile_t, joystick_config);
    (void)wear_leveling_write(addr, &config, sizeof(config));
  }
  joystick_apply_config(config);
}

joystick_state_t joystick_get_state(void) { return current_state; }

void joystick_task(void) {
  joystick_update_signal_state();

  if (config_cache.mode == JOYSTICK_MODE_MOUSE) {
    joystick_task_mouse_mode(timer_read());
  } else if (config_cache.mode == JOYSTICK_MODE_SCROLL) {
    joystick_task_scroll_mode(timer_read());
  } else if (joystick_mode_is_cursor(config_cache.mode)) {
    joystick_task_cursor_mode();
  } else {
    joystick_release_cursor_keys();
  }
}

#endif // JOYSTICK_ENABLED
