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
  joystick_config_t config = {
      .x = {0, 2048, 4095},
      .y = {0, 2048, 4095},
      .deadzone = 0,
      .mode = mode,
      .mouse_speed = 64,
      .mouse_acceleration = 255,
      .sw_debounce_ms = 5,
  };
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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_joystick_mouse_mode_reports_motion_and_button);
  RUN_TEST(test_joystick_cursor_mode_registers_and_releases_arrow_keys);
  RUN_TEST(test_joystick_apply_config_releases_mouse_button_on_mode_change);
  return UNITY_END();
}
