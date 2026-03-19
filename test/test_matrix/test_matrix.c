#include <unity.h>

#include "eeconfig.h"
#include "matrix.h"

eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;

static uint16_t analog_values[NUM_KEYS];
static uint32_t mock_timer;

void analog_task(void) {}

uint16_t analog_read(uint8_t key) { return analog_values[key]; }

uint32_t timer_read(void) { return mock_timer++; }

bool wear_leveling_write(uint32_t address, const void *data, uint32_t len) {
  (void)address;
  (void)data;
  (void)len;
  return true;
}

static void init_key_state(uint8_t key) {
  key_matrix[key].adc_filtered = 2400;
  key_matrix[key].adc_rest_value = 2400;
  key_matrix[key].adc_bottom_out_value = 3050;
  key_matrix[key].distance = 0;
  key_matrix[key].extremum = 0;
  key_matrix[key].key_dir = KEY_DIR_INACTIVE;
  key_matrix[key].is_pressed = false;
  key_matrix[key].event_time = 0;
  analog_values[key] = 2400;

  mock_eeconfig.profiles[0].actuation_map[key] = (actuation_t){
      .actuation_point = 128,
      .rt_down = 20,
      .rt_up = 20,
      .continuous = false,
  };
}

void setUp(void) {
  memset(&mock_eeconfig, 0, sizeof(mock_eeconfig));
  memset(key_matrix, 0, sizeof(key_matrix));
  memset(analog_values, 0, sizeof(analog_values));
  mock_timer = 0;

  mock_eeconfig.current_profile = 0;
  mock_eeconfig.calibration.initial_rest_value = 2400;
  mock_eeconfig.calibration.initial_bottom_out_threshold = 650;
  mock_eeconfig.options.continuous_calibration = false;
  mock_eeconfig.options.save_bottom_out_threshold = false;

  for (uint8_t i = 0; i < NUM_KEYS; i++) {
    init_key_state(i);
  }
}

void tearDown(void) {}

void test_matrix_large_delta_press_and_release_stay_responsive(void) {
  analog_values[0] = 3000;
  matrix_scan();
  matrix_scan();
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
  TEST_ASSERT_EQUAL_UINT8(KEY_DIR_DOWN, key_matrix[0].key_dir);

  analog_values[0] = 2400;
  matrix_scan();
  matrix_scan();
  TEST_ASSERT_FALSE(key_matrix[0].is_pressed);

  analog_values[0] = 3000;
  matrix_scan();
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
}

void test_matrix_uses_faster_filter_for_large_adc_delta(void) {
  key_matrix[0].adc_filtered = 3000;
  key_matrix[0].adc_rest_value = 2400;
  key_matrix[0].adc_bottom_out_value = 3050;
  key_matrix[0].distance = 200;
  key_matrix[0].extremum = 200;
  key_matrix[0].key_dir = KEY_DIR_DOWN;
  key_matrix[0].is_pressed = true;

  analog_values[0] = 2400;
  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2850, key_matrix[0].adc_filtered);
}

void test_matrix_continuous_calibration_tracks_small_rest_drift(void) {
  mock_eeconfig.options.continuous_calibration = true;
  key_matrix[0].adc_filtered = 2408;
  key_matrix[0].adc_rest_value = 2400;
  key_matrix[0].adc_bottom_out_value = 3050;
  analog_values[0] = 2408;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2401, key_matrix[0].adc_rest_value);
  TEST_ASSERT_EQUAL_UINT16(3051, key_matrix[0].adc_bottom_out_value);
}

void test_matrix_continuous_calibration_ignores_large_rest_drift(void) {
  mock_eeconfig.options.continuous_calibration = true;
  key_matrix[0].adc_filtered = 2490;
  key_matrix[0].adc_rest_value = 2400;
  key_matrix[0].adc_bottom_out_value = 3050;
  analog_values[0] = 2490;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2400, key_matrix[0].adc_rest_value);
  TEST_ASSERT_EQUAL_UINT16(3050, key_matrix[0].adc_bottom_out_value);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_matrix_large_delta_press_and_release_stay_responsive);
  RUN_TEST(test_matrix_uses_faster_filter_for_large_adc_delta);
  RUN_TEST(test_matrix_continuous_calibration_tracks_small_rest_drift);
  RUN_TEST(test_matrix_continuous_calibration_ignores_large_rest_drift);
  return UNITY_END();
}
