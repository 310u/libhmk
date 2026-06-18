#include <unity.h>

#include "eeconfig.h"
#include "hardware/analog_api.h"
#include "matrix.h"

eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;

volatile uint16_t analog_key_values[NUM_KEYS];
static uint32_t mock_timer;
static uint32_t mock_cycle;
static uint32_t mock_cycle_step;
static analog_scan_diagnostics_t mock_analog_diag;
static uint32_t mock_full_scan_generation;

static void set_distance_bounds(uint8_t key, uint16_t rest, uint16_t bottom) {
  key_matrix[key].adc_rest_value = rest;
  key_matrix[key].adc_bottom_out_value = bottom;
}

void analog_task(void) {}

uint16_t analog_read(uint8_t key) { return analog_key_values[key]; }

const analog_scan_diagnostics_t *analog_get_scan_diagnostics(void) {
  return &mock_analog_diag;
}

uint32_t analog_scan_get_full_scan_generation(void) {
  return mock_full_scan_generation;
}

bool analog_scan_consume_full_scan_generation(uint32_t *last_seen_generation,
                                              uint32_t *missed_count) {
  if (last_seen_generation == NULL ||
      *last_seen_generation == mock_full_scan_generation) {
    return false;
  }

  if (missed_count != NULL &&
      mock_full_scan_generation - *last_seen_generation > 1u) {
    *missed_count += (mock_full_scan_generation - *last_seen_generation) - 1u;
  }

  *last_seen_generation = mock_full_scan_generation;
  return true;
}

uint32_t timer_read(void) { return mock_timer++; }

uint32_t board_cycle_count(void) {
  mock_cycle += mock_cycle_step;
  return mock_cycle;
}

bool wear_leveling_write(uint32_t address, const void *data, uint32_t len) {
  (void)address;
  (void)data;
  (void)len;
  return true;
}

static void init_key_state(uint8_t key) {
  key_matrix[key].adc_raw = 2400;
  key_matrix[key].adc_filtered = 2400;
  set_distance_bounds(key, 2400, 3050);
  key_matrix[key].filter_mode = MATRIX_FILTER_MODE_IDLE;
  key_matrix[key].filter_decay = 0;
  key_matrix[key].distance = 0;
  key_matrix[key].extremum = 0;
  key_matrix[key].key_dir = KEY_DIR_INACTIVE;
  key_matrix[key].is_pressed = false;
  key_matrix[key].rest_stable_since = 0;
  key_matrix[key].event_time = 0;
  analog_key_values[key] = 2400;

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
  memset((void *)analog_key_values, 0, sizeof(analog_key_values));
  mock_timer = 0;
  mock_cycle = 0;
  mock_cycle_step = 4320u;
  memset(&mock_analog_diag, 0, sizeof(mock_analog_diag));
  mock_analog_diag.estimated_scan_hz = 25000u;
  mock_analog_diag.last_scan_us = 30u;
  mock_analog_diag.max_scan_us = 44u;
  mock_full_scan_generation = 0u;

  mock_eeconfig.current_profile = 0;
  mock_eeconfig.calibration.initial_rest_value = 2400;
  mock_eeconfig.calibration.initial_bottom_out_threshold = 650;
  mock_eeconfig.options.continuous_calibration = false;
  mock_eeconfig.options.save_bottom_out_threshold = false;
  matrix_recalibrate(false);
  mock_timer = 0;
  mock_cycle = 0;

  for (uint8_t i = 0; i < NUM_KEYS; i++) {
    init_key_state(i);
  }
}

void tearDown(void) {}

void test_matrix_large_delta_press_and_release_stay_responsive(void) {
  analog_key_values[0] = 3000;
  matrix_scan();
  matrix_scan();
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
  TEST_ASSERT_EQUAL_UINT8(KEY_DIR_DOWN, key_matrix[0].key_dir);

  analog_key_values[0] = 2400;
  matrix_scan();
  matrix_scan();
  TEST_ASSERT_FALSE(key_matrix[0].is_pressed);

  analog_key_values[0] = 3000;
  matrix_scan();
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
}

void test_matrix_uses_faster_filter_for_large_adc_delta(void) {
  key_matrix[0].adc_raw = 2980;
  key_matrix[0].adc_filtered = 3000;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].distance = 200;
  key_matrix[0].extremum = 200;
  key_matrix[0].key_dir = KEY_DIR_DOWN;
  key_matrix[0].is_pressed = true;

  analog_key_values[0] = 2960;
  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2990, key_matrix[0].adc_filtered);
  TEST_ASSERT_EQUAL_UINT8(MATRIX_FILTER_MODE_FAST, key_matrix[0].filter_mode);
}

void test_matrix_uses_track_filter_for_small_adc_delta(void) {
  key_matrix[0].adc_raw = 2408;
  key_matrix[0].adc_filtered = 2400;
  analog_key_values[0] = 2410;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2401, key_matrix[0].adc_filtered);
  TEST_ASSERT_EQUAL_UINT8(MATRIX_FILTER_MODE_TRACK, key_matrix[0].filter_mode);
}

void test_matrix_uses_burst_filter_and_records_scan_diagnostics(void) {
  key_matrix[0].adc_raw = 2400;
  key_matrix[0].adc_filtered = 2400;
  analog_key_values[0] = 3000;

  matrix_scan();

  const matrix_scan_diagnostics_t *diag = matrix_get_scan_diagnostics();
  TEST_ASSERT_EQUAL_UINT16(2700, key_matrix[0].adc_filtered);
  TEST_ASSERT_EQUAL_UINT8(MATRIX_FILTER_MODE_BURST, key_matrix[0].filter_mode);
  TEST_ASSERT_EQUAL_UINT32(1, diag->scan_count);
  TEST_ASSERT_EQUAL_UINT32(4320, diag->last_scan_cycles);
  TEST_ASSERT_EQUAL_UINT32(4320, diag->max_scan_cycles);
  TEST_ASSERT_EQUAL_UINT32(20, diag->last_scan_us);
  TEST_ASSERT_EQUAL_UINT32(20, diag->max_scan_us);
  TEST_ASSERT_EQUAL_UINT16(600, diag->max_sample_delta);
  TEST_ASSERT_EQUAL_UINT16(600, diag->max_sample_velocity);
  TEST_ASSERT_EQUAL_UINT16(NUM_KEYS - 1, diag->last_mode_counts[MATRIX_FILTER_MODE_IDLE]);
  TEST_ASSERT_EQUAL_UINT16(1, diag->last_mode_counts[MATRIX_FILTER_MODE_BURST]);
}

void test_matrix_clips_bottom_out_value_to_adc_maximum(void) {
  key_matrix[0].adc_raw = ADC_MAX_VALUE;
  key_matrix[0].adc_filtered = ADC_MAX_VALUE;
  set_distance_bounds(0, key_matrix[0].adc_rest_value,
                      (uint16_t)(ADC_MAX_VALUE -
                                 MATRIX_CALIBRATION_EPSILON));
  analog_key_values[0] = UINT16_MAX;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(ADC_MAX_VALUE, key_matrix[0].adc_bottom_out_value);
}

void test_matrix_records_large_scan_intervals_in_microseconds(void) {
  mock_cycle_step = 3000000000u;

  matrix_scan();

  const matrix_scan_diagnostics_t *diag = matrix_get_scan_diagnostics();
  TEST_ASSERT_EQUAL_UINT32(3000000000u, diag->last_scan_cycles);
  TEST_ASSERT_EQUAL_UINT32(13888888u, diag->last_scan_us);
  TEST_ASSERT_EQUAL_UINT32(13888888u, diag->max_scan_us);
}

void test_matrix_continuous_calibration_tracks_small_rest_drift(void) {
  mock_eeconfig.options.continuous_calibration = true;
  key_matrix[0].adc_filtered = 2408;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].rest_stable_since = 0;
  analog_key_values[0] = 2408;
  mock_timer = MATRIX_CONTINUOUS_CALIBRATION_IDLE_MS;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2401, key_matrix[0].adc_rest_value);
  TEST_ASSERT_EQUAL_UINT16(3051, key_matrix[0].adc_bottom_out_value);
}

void test_matrix_continuous_calibration_ignores_large_rest_drift(void) {
  mock_eeconfig.options.continuous_calibration = true;
  key_matrix[0].adc_filtered = 2490;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].rest_stable_since = 0;
  analog_key_values[0] = 2490;
  mock_timer = MATRIX_CONTINUOUS_CALIBRATION_IDLE_MS;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2400, key_matrix[0].adc_rest_value);
  TEST_ASSERT_EQUAL_UINT16(3050, key_matrix[0].adc_bottom_out_value);
}

void test_matrix_continuous_calibration_ignores_unstable_keystroke_motion(void) {
  mock_eeconfig.options.continuous_calibration = true;
  key_matrix[0].adc_filtered = 2408;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].rest_stable_since = 0;
  analog_key_values[0] = 2440;
  mock_timer = MATRIX_CONTINUOUS_CALIBRATION_IDLE_MS;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2400, key_matrix[0].adc_rest_value);
  TEST_ASSERT_EQUAL_UINT16(3050, key_matrix[0].adc_bottom_out_value);
}

void test_matrix_task_runs_only_when_a_new_full_scan_is_ready(void) {
  mock_eeconfig.profiles[0].actuation_map[0].actuation_point = 1u;
  analog_key_values[0] = 3000;

  matrix_task();
  TEST_ASSERT_FALSE(key_matrix[0].is_pressed);

  mock_full_scan_generation = 1u;
  matrix_task();

  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
  const matrix_scan_diagnostics_t *diag = matrix_get_scan_diagnostics();
  TEST_ASSERT_TRUE(matrix_snapshot_key_pressed(0));
  TEST_ASSERT_EQUAL_UINT32(1u, diag->scan_count);
  TEST_ASSERT_EQUAL_UINT32(25000u, diag->raw_scan_hz);
  TEST_ASSERT_EQUAL_UINT32(30u, diag->last_raw_scan_us);
  TEST_ASSERT_EQUAL_UINT32(44u, diag->max_raw_scan_us);
  TEST_ASSERT_EQUAL_UINT32(1u, diag->full_scan_generation);
  TEST_ASSERT_EQUAL_UINT32(1u, diag->matrix_processing_divider);
  TEST_ASSERT_EQUAL_UINT32(1u, diag->skipped_main_loop_count);
  TEST_ASSERT_EQUAL_UINT32(0u, diag->coalesced_generation_count);
  TEST_ASSERT_EQUAL_UINT32(0u, diag->matrix_catchup_scan_count);
}

void test_matrix_task_counts_missed_generations(void) {
  mock_eeconfig.profiles[0].actuation_map[0].actuation_point = 1u;
  analog_key_values[0] = 3000;

  mock_full_scan_generation = 1u;
  matrix_task();
  mock_full_scan_generation = 4u;
  matrix_task();
  matrix_task();

  const matrix_scan_diagnostics_t *diag = matrix_get_scan_diagnostics();
  TEST_ASSERT_EQUAL_UINT32(2u, diag->scan_count);
  TEST_ASSERT_EQUAL_UINT32(25000u, diag->matrix_scan_hz);
  TEST_ASSERT_EQUAL_UINT32(4u, diag->full_scan_generation);
  TEST_ASSERT_EQUAL_UINT32(2u, diag->missed_generation_count);
  TEST_ASSERT_EQUAL_UINT32(0u, diag->intentional_skip_count);
  TEST_ASSERT_EQUAL_UINT32(2u, diag->coalesced_generation_count);
  TEST_ASSERT_EQUAL_UINT32(2u, diag->overload_missed_generation_count);
  TEST_ASSERT_EQUAL_UINT32(0u, diag->scheduler_budget_exhausted_count);
  TEST_ASSERT_EQUAL_UINT32(0u, diag->matrix_catchup_scan_count);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_matrix_large_delta_press_and_release_stay_responsive);
  RUN_TEST(test_matrix_uses_faster_filter_for_large_adc_delta);
  RUN_TEST(test_matrix_uses_track_filter_for_small_adc_delta);
  RUN_TEST(test_matrix_uses_burst_filter_and_records_scan_diagnostics);
  RUN_TEST(test_matrix_clips_bottom_out_value_to_adc_maximum);
  RUN_TEST(test_matrix_records_large_scan_intervals_in_microseconds);
  RUN_TEST(test_matrix_continuous_calibration_tracks_small_rest_drift);
  RUN_TEST(test_matrix_continuous_calibration_ignores_large_rest_drift);
  RUN_TEST(test_matrix_continuous_calibration_ignores_unstable_keystroke_motion);
  RUN_TEST(test_matrix_task_runs_only_when_a_new_full_scan_is_ready);
  RUN_TEST(test_matrix_task_counts_missed_generations);
  return UNITY_END();
}
