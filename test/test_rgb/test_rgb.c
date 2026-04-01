#include <string.h>
#include <unity.h>

#include "eeconfig.h"
#include "matrix.h"
#include "rgb.h"
#include "rgb_animated.h"
#include "rgb_internal.h"
#include "rgb_static.h"

static eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;
key_state_t key_matrix[NUM_KEYS];

static uint8_t last_grb_frame[NUM_LEDS * 3];
static uint16_t last_grb_frame_len;
static uint32_t mock_time;

void rgb_driver_init(void) {}
void rgb_driver_task(void) {}

void rgb_driver_write(const uint8_t *grb_data, uint16_t byte_count) {
  last_grb_frame_len = byte_count;
  memcpy(last_grb_frame, grb_data, byte_count);
}

uint32_t timer_read(void) { return mock_time; }

uint32_t matrix_get_idle_time(void) { return 0; }

uint8_t layout_get_current_layer(void) { return 0; }

void rgb_animated_reset(void) {}
void rgb_animated_render(rgb_effect_t effect,
                         const rgb_animated_context_t *context) {
  (void)effect;
  (void)context;
}

void rgb_static_reset(void) {}

bool rgb_static_render(rgb_effect_t effect, const rgb_static_context_t *context) {
  if (effect == RGB_EFFECT_SOLID_COLOR) {
    const rgb_color_t color = context->config->solid_color;
    rgb_set_all_color((uint8_t)(((uint32_t)color.r * context->effective_brightness) / 255u),
                      (uint8_t)(((uint32_t)color.g * context->effective_brightness) / 255u),
                      (uint8_t)(((uint32_t)color.b * context->effective_brightness) / 255u));
    return true;
  }

  return false;
}

void rgb_reactive_decay_heatmap(uint32_t current_tick) {
  (void)current_tick;
}

void rgb_reactive_record_keypress(uint8_t index) { (void)index; }

void rgb_reactive_render_heatmap(uint8_t effective_brightness) {
  (void)effective_brightness;
}

void rgb_reactive_render_effect(uint8_t effect, uint8_t base_hue,
                                uint8_t effective_brightness, uint8_t speed) {
  (void)effect;
  (void)base_hue;
  (void)effective_brightness;
  (void)speed;
}

void rgb_reactive_render_splash(uint8_t effect, uint8_t base_hue,
                                uint8_t effective_brightness, uint8_t speed) {
  (void)effect;
  (void)base_hue;
  (void)effective_brightness;
  (void)speed;
}

static rgb_color_t driver_rgb_at(uint8_t led_index) {
  const uint16_t offset = (uint16_t)led_index * 3u;
  return (rgb_color_t){
      .r = last_grb_frame[offset + 1u],
      .g = last_grb_frame[offset],
      .b = last_grb_frame[offset + 2u],
  };
}

static rgb_color_t driver_rgb_for_key(uint8_t key_index) {
  const uint8_t led_index = rgb_key_to_led_at(key_index);
  TEST_ASSERT_NOT_EQUAL_UINT8(255u, led_index);
  return driver_rgb_at(led_index);
}

void setUp(void) {
  memset(&mock_eeconfig, 0, sizeof(mock_eeconfig));
  memset(key_matrix, 0, sizeof(key_matrix));
  memset(last_grb_frame, 0, sizeof(last_grb_frame));
  last_grb_frame_len = 0;
  mock_time = 0;

  mock_eeconfig.profiles[0].rgb_config.enabled = 1u;
  mock_eeconfig.profiles[0].rgb_config.global_brightness = 255u;
  mock_eeconfig.profiles[0].rgb_config.current_effect = RGB_EFFECT_TRIGGER_STATE;
  mock_eeconfig.profiles[0].rgb_config.trigger_state_colors[RGB_TRIGGER_STATE_IDLE] =
      (rgb_color_t){.r = 1u, .g = 2u, .b = 3u};
  mock_eeconfig.profiles[0].rgb_config.trigger_state_colors[RGB_TRIGGER_STATE_RELEASE] =
      (rgb_color_t){.r = 4u, .g = 5u, .b = 6u};
  mock_eeconfig.profiles[0].rgb_config.trigger_state_colors[RGB_TRIGGER_STATE_PRESS] =
      (rgb_color_t){.r = 7u, .g = 8u, .b = 9u};
  mock_eeconfig.profiles[0].rgb_config.trigger_state_colors[RGB_TRIGGER_STATE_HOLD] =
      (rgb_color_t){.r = 10u, .g = 11u, .b = 12u};

  rgb_init();
}

void tearDown(void) {}

void test_rgb_trigger_state_uses_configured_color_for_each_state(void) {
  key_matrix[1].key_dir = KEY_DIR_UP;
  key_matrix[2].is_pressed = true;
  key_matrix[2].key_dir = KEY_DIR_DOWN;
  key_matrix[3].is_pressed = true;
  key_matrix[3].key_dir = KEY_DIR_INACTIVE;

  mock_time = 16u;
  rgb_task();

  TEST_ASSERT_EQUAL_UINT16(NUM_LEDS * 3u, last_grb_frame_len);

  const rgb_color_t idle = driver_rgb_for_key(0u);
  TEST_ASSERT_EQUAL_UINT8(1u, idle.r);
  TEST_ASSERT_EQUAL_UINT8(2u, idle.g);
  TEST_ASSERT_EQUAL_UINT8(3u, idle.b);

  const rgb_color_t released = driver_rgb_for_key(1u);
  TEST_ASSERT_EQUAL_UINT8(4u, released.r);
  TEST_ASSERT_EQUAL_UINT8(5u, released.g);
  TEST_ASSERT_EQUAL_UINT8(6u, released.b);

  const rgb_color_t pressed_down = driver_rgb_for_key(2u);
  TEST_ASSERT_EQUAL_UINT8(7u, pressed_down.r);
  TEST_ASSERT_EQUAL_UINT8(8u, pressed_down.g);
  TEST_ASSERT_EQUAL_UINT8(9u, pressed_down.b);

  const rgb_color_t held = driver_rgb_for_key(3u);
  TEST_ASSERT_EQUAL_UINT8(10u, held.r);
  TEST_ASSERT_EQUAL_UINT8(11u, held.g);
  TEST_ASSERT_EQUAL_UINT8(12u, held.b);
}

void test_rgb_solid_color_scales_global_brightness(void) {
  rgb_config_t *config = rgb_get_config();
  config->current_effect = RGB_EFFECT_SOLID_COLOR;
  config->global_brightness = 128u;
  config->solid_color = (rgb_color_t){.r = 100u, .g = 50u, .b = 25u};

  mock_time = 1000u;
  rgb_task();

  TEST_ASSERT_EQUAL_UINT16(NUM_LEDS * 3u, last_grb_frame_len);

  const rgb_color_t solid = driver_rgb_at(0u);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)((100u * 128u) / 255u), solid.r);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)((50u * 128u) / 255u), solid.g);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)((25u * 128u) / 255u), solid.b);
}

void test_rgb_binary_clock_renders_time_digits_and_seconds_progress(void) {
  rgb_config_t *config = rgb_get_config();
  config->current_effect = RGB_EFFECT_BINARY_CLOCK;
  config->solid_color = (rgb_color_t){.r = 120u, .g = 40u, .b = 10u};
  config->secondary_color = (rgb_color_t){.r = 10u, .g = 90u, .b = 30u};
  config->background_color = (rgb_color_t){.r = 60u, .g = 20u, .b = 90u};

  rgb_set_clock_time(12u, 34u, 45u);

  mock_time = 16u;
  rgb_task();

  TEST_ASSERT_EQUAL_UINT16(NUM_LEDS * 3u, last_grb_frame_len);

  const rgb_color_t background = {
      .r = 60u,
      .g = 20u,
      .b = 90u,
  };

  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(0u).r);
  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(1u).r);
  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(2u).r);
  TEST_ASSERT_EQUAL_UINT8(120u, driver_rgb_for_key(3u).r);

  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(10u).r);
  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(11u).r);
  TEST_ASSERT_EQUAL_UINT8(120u, driver_rgb_for_key(12u).r);
  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(13u).r);

  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(6u).r);
  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(7u).r);
  TEST_ASSERT_EQUAL_UINT8(120u, driver_rgb_for_key(8u).r);
  TEST_ASSERT_EQUAL_UINT8(120u, driver_rgb_for_key(9u).r);

  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(16u).r);
  TEST_ASSERT_EQUAL_UINT8(120u, driver_rgb_for_key(17u).r);
  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(18u).r);
  TEST_ASSERT_EQUAL_UINT8(background.r, driver_rgb_for_key(19u).r);

  for (uint8_t key = 30u; key <= 39u; key++) {
    const rgb_color_t color = driver_rgb_for_key(key);
    TEST_ASSERT_EQUAL_UINT8(background.r, color.r);
    TEST_ASSERT_EQUAL_UINT8(background.g, color.g);
    TEST_ASSERT_EQUAL_UINT8(background.b, color.b);
  }

  for (uint8_t key = 20u; key <= 26u; key++) {
    const rgb_color_t color = driver_rgb_for_key(key);
    TEST_ASSERT_EQUAL_UINT8(10u, color.r);
    TEST_ASSERT_EQUAL_UINT8(90u, color.g);
    TEST_ASSERT_EQUAL_UINT8(30u, color.b);
  }

  {
    const rgb_color_t head = driver_rgb_for_key(27u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)((120u * 170u) / 255u), head.r);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)((40u * 170u) / 255u), head.g);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)((10u * 170u) / 255u), head.b);
  }

  for (uint8_t key = 28u; key <= 29u; key++) {
    const rgb_color_t color = driver_rgb_for_key(key);
    TEST_ASSERT_EQUAL_UINT8(background.r, color.r);
    TEST_ASSERT_EQUAL_UINT8(background.g, color.g);
    TEST_ASSERT_EQUAL_UINT8(background.b, color.b);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_rgb_trigger_state_uses_configured_color_for_each_state);
  RUN_TEST(test_rgb_solid_color_scales_global_brightness);
  RUN_TEST(test_rgb_binary_clock_renders_time_digits_and_seconds_progress);
  return UNITY_END();
}
