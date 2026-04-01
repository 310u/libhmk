#include <unity.h>

#include "eeconfig.h"
#include "joystick.h"
#include "keycodes.h"
#include "stm32f4xx_hal.h"

static eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;

static uint16_t analog_raw_values[2];
static uint32_t mock_time = 0;
static GPIO_TypeDef gpioa_instance = {0};
GPIO_TypeDef *const GPIOA = &gpioa_instance;
static GPIO_PinState mock_sw_pin_state = GPIO_PIN_SET;

static int8_t last_mouse_x = 0;
static int8_t last_mouse_y = 0;
static uint8_t last_mouse_buttons = 0;
static uint32_t mouse_move_count = 0;
static int8_t last_scroll_wheel = 0;
static int8_t last_scroll_pan = 0;
static uint8_t last_scroll_buttons = 0;
static uint32_t mouse_scroll_count = 0;
static uint8_t pressed_keycodes[8] = {0};
static uint8_t released_keycodes[8] = {0};
static uint8_t pressed_count = 0;
static uint8_t released_count = 0;

bool is_sniper_active = false;

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init) {
  (void)port;
  (void)init;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin) {
  (void)port;
  (void)pin;
  return mock_sw_pin_state;
}

uint16_t analog_read_raw(uint8_t index) { return analog_raw_values[index]; }

uint32_t timer_read(void) { return mock_time; }

void hid_keycode_add(uint8_t keycode) {
  if (pressed_count < M_ARRAY_SIZE(pressed_keycodes)) {
    pressed_keycodes[pressed_count++] = keycode;
  }
}

void hid_keycode_remove(uint8_t keycode) {
  if (released_count < M_ARRAY_SIZE(released_keycodes)) {
    released_keycodes[released_count++] = keycode;
  }
}

void hid_mouse_move(int8_t x, int8_t y, uint8_t buttons) {
  last_mouse_x = x;
  last_mouse_y = y;
  last_mouse_buttons = buttons;
  mouse_move_count++;
}

void hid_mouse_scroll(int8_t wheel, int8_t pan, uint8_t buttons) {
  last_scroll_wheel = wheel;
  last_scroll_pan = pan;
  last_scroll_buttons = buttons;
  mouse_scroll_count++;
}

void hid_clear_runtime_state(void) {}
void hid_send_reports(void) {}

bool wear_leveling_write(uint32_t addr, const void *buf, uint32_t len) {
  (void)addr;
  (void)buf;
  (void)len;
  return true;
}

static joystick_config_t joystick_test_config(uint8_t mode) {
  joystick_config_t config;
  joystick_init_default_config(&config);
  config.deadzone = 0;
  config.mode = mode;
  config.mouse_speed = 64;
  config.mouse_acceleration = 255;
  joystick_fill_default_mouse_presets(config.mouse_presets, config.mouse_speed,
                                      config.mouse_acceleration);
  return config;
}

static joystick_config_t joystick_user_regression_config(void) {
  joystick_config_t config = joystick_test_config(JOYSTICK_MODE_XINPUT_RS);

  config.x.min = 601;
  config.x.center = 2032;
  config.x.max = 3305;
  config.y.min = 413;
  config.y.center = 1981;
  config.y.max = 3513;
  config.deadzone = 10;

  const uint8_t boundaries[JOYSTICK_RADIAL_BOUNDARY_SECTORS] = {
      126, 129, 131, 130, 131, 131, 130, 128, 127, 127, 127,
      126, 125, 123, 121, 120, 127, 129, 129, 130, 130, 131,
      128, 128, 128, 128, 125, 126, 129, 129, 127, 125,
  };
  memcpy(config.radial_boundaries, boundaries, sizeof(boundaries));
  return config;
}

static void reset_reports(void) {
  last_mouse_x = 0;
  last_mouse_y = 0;
  last_mouse_buttons = 0;
  mouse_move_count = 0;
  last_scroll_wheel = 0;
  last_scroll_pan = 0;
  last_scroll_buttons = 0;
  mouse_scroll_count = 0;
  memset(pressed_keycodes, 0, sizeof(pressed_keycodes));
  memset(released_keycodes, 0, sizeof(released_keycodes));
  pressed_count = 0;
  released_count = 0;
}

void setUp(void) {
  memset(&mock_eeconfig, 0, sizeof(mock_eeconfig));
  analog_raw_values[0] = 2048;
  analog_raw_values[1] = 2048;
  mock_sw_pin_state = GPIO_PIN_SET;
  mock_time = 0;
  reset_reports();
  is_sniper_active = false;

  mock_eeconfig.current_profile = 0;
  mock_eeconfig.options.sniper_mode_multiplier = 128;
  mock_eeconfig.profiles[0].joystick_config =
      joystick_test_config(JOYSTICK_MODE_MOUSE);
  joystick_init();
}

void tearDown(void) {}

void test_joystick_mouse_mode_reports_motion_and_button(void) {
  analog_raw_values[0] = 4095;
  analog_raw_values[1] = 2048;

  mock_time = 0;
  joystick_task();

  mock_sw_pin_state = GPIO_PIN_RESET;
  mock_time = 10;
  joystick_task();

  mock_time = 20;
  joystick_task();

  TEST_ASSERT_EQUAL_UINT32(2, mouse_move_count);
  TEST_ASSERT_GREATER_THAN_INT8(0, last_mouse_x);
  TEST_ASSERT_EQUAL_INT8(0, last_mouse_y);
  TEST_ASSERT_EQUAL_UINT8(1, last_mouse_buttons);
}

void test_joystick_cursor_mode_registers_and_releases_arrow_keys(void) {
  joystick_config_t config = joystick_test_config(JOYSTICK_MODE_CURSOR_8);
  joystick_apply_config(config);

  analog_raw_values[0] = 4095;
  analog_raw_values[1] = 4095;
  mock_time = 1;
  joystick_task();

  TEST_ASSERT_EQUAL_UINT8(2, pressed_count);
  TEST_ASSERT_EQUAL_UINT8(KC_RIGHT, pressed_keycodes[0]);
  TEST_ASSERT_EQUAL_UINT8(KC_UP, pressed_keycodes[1]);

  joystick_apply_config(joystick_test_config(JOYSTICK_MODE_DISABLED));

  TEST_ASSERT_EQUAL_UINT8(2, released_count);
  TEST_ASSERT_EQUAL_UINT8(KC_RIGHT, released_keycodes[0]);
  TEST_ASSERT_EQUAL_UINT8(KC_UP, released_keycodes[1]);
}

void test_joystick_apply_config_releases_mouse_button_on_mode_change(void) {
  analog_raw_values[0] = 4095;
  mock_time = 0;
  joystick_task();

  mock_sw_pin_state = GPIO_PIN_RESET;
  mock_time = 10;
  joystick_task();

  mock_time = 20;
  joystick_task();
  TEST_ASSERT_EQUAL_UINT8(1, last_mouse_buttons);

  joystick_apply_config(joystick_test_config(JOYSTICK_MODE_SCROLL));

  TEST_ASSERT_EQUAL_UINT32(3, mouse_move_count);
  TEST_ASSERT_EQUAL_INT8(0, last_mouse_x);
  TEST_ASSERT_EQUAL_INT8(0, last_mouse_y);
  TEST_ASSERT_EQUAL_UINT8(0, last_mouse_buttons);
}

void test_joystick_circular_correction_scales_diagonal_throw(void) {
  joystick_config_t config = joystick_test_config(JOYSTICK_MODE_DISABLED);
  joystick_fill_default_radial_boundaries(config.radial_boundaries);
  config.radial_boundaries[4] = 181;
  joystick_apply_config(config);

  analog_raw_values[0] = 4095;
  analog_raw_values[1] = 4095;
  mock_time = 1;
  joystick_task();

  joystick_state_t state = joystick_get_state();
  TEST_ASSERT_INT_WITHIN(2, 90, state.out_x);
  TEST_ASSERT_INT_WITHIN(2, 90, state.out_y);
}

void test_joystick_circular_correction_uses_monotone_sector_interpolation(void) {
  joystick_config_t config = joystick_test_config(JOYSTICK_MODE_DISABLED);
  joystick_fill_default_radial_boundaries(config.radial_boundaries);
  config.radial_boundaries[2] = 145;
  config.radial_boundaries[3] = 181;
  config.radial_boundaries[4] = 145;
  config.radial_boundaries[5] = 127;
  joystick_apply_config(config);

  analog_raw_values[0] = 4095;
  analog_raw_values[1] = 3725;
  mock_time = 1;
  joystick_task();

  joystick_state_t state = joystick_get_state();
  TEST_ASSERT_EQUAL_INT8(97, state.out_x);
  TEST_ASSERT_EQUAL_INT8(80, state.out_y);
}

void test_joystick_gamepad_mode_bypasses_adc_smoothing(void) {
  joystick_apply_config(joystick_test_config(JOYSTICK_MODE_XINPUT_RS));

  analog_raw_values[0] = 4095;
  analog_raw_values[1] = 2048;
  mock_time = 1;
  joystick_task();

  joystick_state_t state = joystick_get_state();
  TEST_ASSERT_INT_WITHIN(1, 127, state.out_x);
  TEST_ASSERT_EQUAL_INT8(0, state.out_y);
}

void test_joystick_gamepad_mode_does_not_shrink_small_input_steps(void) {
  joystick_apply_config(joystick_test_config(JOYSTICK_MODE_XINPUT_RS));

  analog_raw_values[0] = 2048;
  analog_raw_values[1] = 2048;
  mock_time = 1;
  joystick_task();

  analog_raw_values[0] = 2063;
  analog_raw_values[1] = 2048;
  mock_time = 2;
  joystick_task();

  joystick_state_t state = joystick_get_state();
  TEST_ASSERT_GREATER_THAN_INT8(0, state.out_x);
  TEST_ASSERT_EQUAL_INT8(0, state.out_y);
}

void test_joystick_preserves_fractional_axis_precision_until_output(void) {
  joystick_apply_config(joystick_test_config(JOYSTICK_MODE_DISABLED));

  analog_raw_values[0] = 3072;
  analog_raw_values[1] = 2048;
  mock_time = 1;
  joystick_task();

  joystick_state_t state = joystick_get_state();
  TEST_ASSERT_EQUAL_INT8(64, state.out_x);
  TEST_ASSERT_EQUAL_INT8(0, state.out_y);
}

void test_joystick_user_regression_config_preserves_full_vertical_throw(void) {
  joystick_apply_config(joystick_user_regression_config());

  analog_raw_values[0] = 2032;
  analog_raw_values[1] = 3513;
  mock_time = 1;
  joystick_task();

  joystick_state_t state = joystick_get_state();
  TEST_ASSERT_INT_WITHIN(1, 0, state.calibrated_x);
  TEST_ASSERT_INT_WITHIN(1, 127, state.calibrated_y);
  TEST_ASSERT_INT_WITHIN(1, 0, state.corrected_x);
  TEST_ASSERT_INT_WITHIN(1, 127, state.corrected_y);
  TEST_ASSERT_INT_WITHIN(1, 0, state.out_x);
  TEST_ASSERT_INT_WITHIN(1, 127, state.out_y);
}

void test_joystick_user_regression_config_preserves_upper_right_arc(void) {
  joystick_apply_config(joystick_user_regression_config());

  analog_raw_values[0] = 2234;
  analog_raw_values[1] = 3504;
  mock_time = 1;
  joystick_task();

  joystick_state_t state = joystick_get_state();
  TEST_ASSERT_INT_WITHIN(2, 20, state.calibrated_x);
  TEST_ASSERT_INT_WITHIN(2, 126, state.calibrated_y);
  TEST_ASSERT_INT_WITHIN(2, 20, state.corrected_x);
  TEST_ASSERT_INT_WITHIN(2, 126, state.corrected_y);
  TEST_ASSERT_INT_WITHIN(2, 20, state.out_x);
  TEST_ASSERT_INT_WITHIN(2, 126, state.out_y);
}

void test_joystick_select_mouse_preset_updates_effective_pointer_settings(void) {
  joystick_config_t config = joystick_test_config(JOYSTICK_MODE_MOUSE);
  config.mouse_presets[0] =
      (joystick_mouse_preset_t){.mouse_speed = 24, .mouse_acceleration = 96};
  config.mouse_presets[1] =
      (joystick_mouse_preset_t){.mouse_speed = 80, .mouse_acceleration = 220};

  joystick_select_mouse_preset(&config, 1);
  joystick_apply_config(config);

  joystick_config_t applied = joystick_get_config();
  TEST_ASSERT_EQUAL_UINT8(1, applied.active_mouse_preset);
  TEST_ASSERT_EQUAL_UINT8(80, applied.mouse_speed);
  TEST_ASSERT_EQUAL_UINT8(220, applied.mouse_acceleration);
}

void test_joystick_legacy_scroll_profile_waits_for_legacy_interval(void) {
  joystick_config_t config = joystick_test_config(JOYSTICK_MODE_SCROLL);
  config.scroll_profile = JOYSTICK_SCROLL_PROFILE_LEGACY;
  joystick_apply_config(config);

  analog_raw_values[0] = 4095;
  analog_raw_values[1] = 2048;

  mock_time = 1;
  joystick_task();
  TEST_ASSERT_EQUAL_UINT32(0, mouse_scroll_count);

  mock_time = 8;
  joystick_task();
  TEST_ASSERT_EQUAL_UINT32(1, mouse_scroll_count);
  TEST_ASSERT_GREATER_THAN_INT8(0, last_scroll_pan);
}

void test_joystick_smooth_scroll_profile_reports_at_high_frequency(void) {
  joystick_config_t config = joystick_test_config(JOYSTICK_MODE_SCROLL);
  config.scroll_profile = JOYSTICK_SCROLL_PROFILE_SMOOTH;
  joystick_apply_config(config);

  analog_raw_values[0] = 4095;
  analog_raw_values[1] = 2048;

  mock_time = 1;
  joystick_task();

  TEST_ASSERT_EQUAL_UINT32(1, mouse_scroll_count);
  TEST_ASSERT_GREATER_THAN_INT8(0, last_scroll_pan);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_joystick_mouse_mode_reports_motion_and_button);
  RUN_TEST(test_joystick_cursor_mode_registers_and_releases_arrow_keys);
  RUN_TEST(test_joystick_apply_config_releases_mouse_button_on_mode_change);
  RUN_TEST(test_joystick_circular_correction_scales_diagonal_throw);
  RUN_TEST(test_joystick_circular_correction_uses_monotone_sector_interpolation);
  RUN_TEST(test_joystick_gamepad_mode_bypasses_adc_smoothing);
  RUN_TEST(test_joystick_gamepad_mode_does_not_shrink_small_input_steps);
  RUN_TEST(test_joystick_preserves_fractional_axis_precision_until_output);
  RUN_TEST(test_joystick_user_regression_config_preserves_full_vertical_throw);
  RUN_TEST(test_joystick_user_regression_config_preserves_upper_right_arc);
  RUN_TEST(test_joystick_select_mouse_preset_updates_effective_pointer_settings);
  RUN_TEST(test_joystick_legacy_scroll_profile_waits_for_legacy_interval);
  RUN_TEST(test_joystick_smooth_scroll_profile_reports_at_high_frequency);
  return UNITY_END();
}
