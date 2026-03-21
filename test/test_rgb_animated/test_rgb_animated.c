#include <unity.h>

#include "rgb_animated.h"

static rgb_color_t led_colors[NUM_LEDS];
static uint32_t mock_time = 0;

static rgb_animated_context_t pixel_flow_context(bool effect_changed) {
  rgb_animated_context_t context = {
      .base_hue = 20,
      .effective_brightness = 128,
      .effect_speed = 96,
      .effect_changed = effect_changed,
  };
  return context;
}

static bool colors_equal(const rgb_color_t *a, const rgb_color_t *b) {
  return memcmp(a, b, sizeof(rgb_color_t) * NUM_LEDS) == 0;
}

uint32_t timer_read(void) { return mock_time; }

void rgb_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
  led_colors[index].r = r;
  led_colors[index].g = g;
  led_colors[index].b = b;
}

rgb_color_t hsv_to_rgb(hsv_t hsv) {
  return (rgb_color_t){.r = hsv.h, .g = hsv.s, .b = hsv.v};
}

uint8_t rgb_coord_x_at(uint8_t led) { return (uint8_t)(led * 16u); }
uint8_t rgb_coord_y_at(uint8_t led) { return (uint8_t)(led * 8u); }
uint8_t rgb_key_to_led_at(uint8_t key) { return key; }
uint8_t rgb_reactive_clip_at(uint8_t source_led, uint8_t target_led) {
  (void)source_led;
  (void)target_led;
  return 0;
}

void setUp(void) {
  memset(led_colors, 0, sizeof(led_colors));
  mock_time = 0;
  rgb_animated_reset();
}

void tearDown(void) {}

void test_rgb_animated_reset_restores_deterministic_first_frame(void) {
  rgb_color_t first_frame[NUM_LEDS];
  rgb_animated_context_t context = pixel_flow_context(true);

  rgb_animated_render(RGB_EFFECT_PIXEL_FLOW, &context);
  memcpy(first_frame, led_colors, sizeof(first_frame));

  mock_time = 100;
  context.effect_changed = false;
  rgb_animated_render(RGB_EFFECT_PIXEL_FLOW, &context);

  memset(led_colors, 0, sizeof(led_colors));
  rgb_animated_reset();
  context.effect_changed = true;
  mock_time = 0;
  rgb_animated_render(RGB_EFFECT_PIXEL_FLOW, &context);

  TEST_ASSERT_TRUE(colors_equal(first_frame, led_colors));
}

void test_rgb_animated_pixel_flow_waits_for_interval_before_shifting(void) {
  rgb_color_t initial_frame[NUM_LEDS];
  rgb_animated_context_t context = pixel_flow_context(true);

  rgb_animated_render(RGB_EFFECT_PIXEL_FLOW, &context);
  memcpy(initial_frame, led_colors, sizeof(initial_frame));

  context.effect_changed = false;
  mock_time = 1;
  rgb_animated_render(RGB_EFFECT_PIXEL_FLOW, &context);
  TEST_ASSERT_TRUE(colors_equal(initial_frame, led_colors));

  mock_time = 5000;
  rgb_animated_render(RGB_EFFECT_PIXEL_FLOW, &context);
  TEST_ASSERT_TRUE(colors_equal(initial_frame, led_colors));

  mock_time = 5001;
  rgb_animated_render(RGB_EFFECT_PIXEL_FLOW, &context);
  TEST_ASSERT_FALSE(colors_equal(initial_frame, led_colors));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_rgb_animated_reset_restores_deterministic_first_frame);
  RUN_TEST(test_rgb_animated_pixel_flow_waits_for_interval_before_shifting);
  return UNITY_END();
}
