#include <unity.h>

#include "distance.h"
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
  key_matrix[key].pos = 0.0f;
  key_matrix[key].velocity = 0.0f;
  key_matrix[key].innovation = 0.0f;
  key_matrix[key].distance = 0;
  key_matrix[key].extremum = 0;
  key_matrix[key].key_dir = KEY_DIR_INACTIVE;
  key_matrix[key].is_pressed = false;
  key_matrix[key].rest_stable_since = 0;
  key_matrix[key].event_time = 0;
  key_matrix[key].bottom_out_hold = 0;
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
  mock_eeconfig.kalman_config = (kalman_config_t)DEFAULT_KALMAN_CONFIG;
  mock_eeconfig.version = EECONFIG_VERSION;
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
  for (uint16_t i = 0; i < 8; i++) {
    matrix_scan();
  }
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
  TEST_ASSERT_EQUAL_UINT8(KEY_DIR_DOWN, key_matrix[0].key_dir);

  analog_key_values[0] = 2400;
  for (uint16_t i = 0; i < 8; i++) {
    matrix_scan();
  }
  TEST_ASSERT_FALSE(key_matrix[0].is_pressed);

  analog_key_values[0] = 3000;
  for (uint16_t i = 0; i < 4; i++) {
    matrix_scan();
  }
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
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
  key_matrix[0].pos = 0.0f;
  key_matrix[0].velocity = 0.0f;
  // Use the averaged ADC value so the filtered delta is zero and the key is
  // considered stable; the raw ADC value is 2410 so the 2-sample average is 2405.
  key_matrix[0].adc_filtered = 2405;
  key_matrix[0].adc_raw = 2400;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].rest_stable_since = 0;
  analog_key_values[0] = 2410;
  mock_timer = MATRIX_CONTINUOUS_CALIBRATION_IDLE_MS;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2401, key_matrix[0].adc_rest_value);
  TEST_ASSERT_EQUAL_UINT16(3051, key_matrix[0].adc_bottom_out_value);
}

void test_matrix_continuous_calibration_ignores_large_rest_drift(void) {
  mock_eeconfig.options.continuous_calibration = true;
  key_matrix[0].pos = 2490.0f;
  key_matrix[0].velocity = 0.0f;
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
  key_matrix[0].pos = 2408.0f;
  key_matrix[0].velocity = 0.0f;
  key_matrix[0].adc_filtered = 2408;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].rest_stable_since = 0;
  analog_key_values[0] = 2440;
  mock_timer = MATRIX_CONTINUOUS_CALIBRATION_IDLE_MS;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2400, key_matrix[0].adc_rest_value);
  TEST_ASSERT_EQUAL_UINT16(3050, key_matrix[0].adc_bottom_out_value);
}

void test_kalman_step_response_converges_to_new_setpoint(void) {
  // Start from rest at 2400, then apply a step to 2700.
  analog_key_values[0] = 2700;

  // Allow the Kalman filter to converge. Higher-gain configurations settle
  // quickly; lower-gain ones need more scans.
  for (uint16_t i = 0; i < 64; i++) {
    matrix_scan();
  }

  // After convergence, filtered position should be close to 2700.
  TEST_ASSERT_UINT16_WITHIN(10, 2700, key_matrix[0].adc_filtered);

  // Velocity estimate should settle near zero.
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, key_matrix[0].velocity);
}

void test_kalman_ramp_response_tracks_constant_velocity(void) {
  // Ramp up at 8 ADC units per scan from rest to near bottom-out.
  for (uint16_t i = 0; i < 32; i++) {
    analog_key_values[0] = (uint16_t)(2400 + (i * 8));
    matrix_scan();
  }

  // In steady-state ramp tracking, innovation should be small.
  // Last raw input is 2400 + 31*8 = 2648.
  TEST_ASSERT_UINT16_WITHIN(15, 2648, key_matrix[0].adc_filtered);

  // The filter should track upward motion and report a positive velocity.
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, key_matrix[0].velocity);
  TEST_ASSERT_LESS_THAN_FLOAT(10.0f, key_matrix[0].velocity);
}

void test_kalman_noise_rejection_at_rest_clamps_distance(void) {
  // Simulate small noise around rest without leaving the deadzone.
  for (uint16_t i = 0; i < 20; i++) {
    analog_key_values[0] = (i & 1u) ? 2401 : 2400;
    matrix_scan();
  }

  // Distance should remain clamped to zero because the filtered value stays
  // within the noise deadzone of the rest value.
  TEST_ASSERT_EQUAL_UINT8(0, key_matrix[0].distance);
}

void test_noise_deadzone_clamps_distance_to_zero(void) {
  // With rest=2400 and MATRIX_NOISE_DEADZONE (default 2, or overridden),
  // values at or below rest + DEADZONE should produce zero distance.
  const uint16_t rest = 2400;
  const uint16_t deadzone = MATRIX_NOISE_DEADZONE;

  // Place key just inside the deadzone.
  analog_key_values[0] = (uint16_t)(rest + deadzone);
  matrix_scan();

  TEST_ASSERT_EQUAL_UINT8(0, key_matrix[0].distance);

  // Move just outside the deadzone; distance should become non-zero.
  analog_key_values[0] = (uint16_t)(rest + deadzone + 3);
  // Ramp the value gradually so the Kalman filter follows it.
  for (uint16_t i = 0; i < 8; i++) {
    matrix_scan();
  }

  TEST_ASSERT_GREATER_THAN_UINT8(0, key_matrix[0].distance);
}

void test_bottom_out_detection_arms_release_on_collision(void) {
  if (MATRIX_INNOVATION_EVENT_THRESHOLD >= 100.0f) {
    TEST_IGNORE_MESSAGE(
        "Test requires enabled bottom-out collision detection");
  }

  // Set key near bottom-out in distance space with downward velocity.
  key_matrix[0].pos = 225.0f;
  key_matrix[0].velocity = 5.0f;
  key_matrix[0].adc_filtered = 2950;
  key_matrix[0].adc_raw = 2950;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].key_dir = KEY_DIR_DOWN;
  key_matrix[0].is_pressed = true;
  key_matrix[0].extremum = 225;

  // Sudden stop (collision with plate). The prediction overshoots the new
  // measurement, producing a large negative innovation that arms the reduced
  // release threshold.
  analog_key_values[0] = 2800;
  matrix_scan();

  // The key should remain pressed and the collision should arm the reduced
  // release threshold by setting a non-zero hold counter.
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
  TEST_ASSERT_LESS_THAN_FLOAT(5.0f, key_matrix[0].velocity);
  TEST_ASSERT_GREATER_THAN_UINT16(0, key_matrix[0].bottom_out_hold);

  // A small release should keep the key pressed because the reduced rt_up has
  // not been exceeded yet.
  analog_key_values[0] = 2900;
  matrix_scan();
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);

  // Release by enough distance units to exceed the reduced rt_up threshold.
  analog_key_values[0] = 2740;
  for (uint16_t i = 0; i < 12; i++) {
    matrix_scan();
  }

  TEST_ASSERT_FALSE(key_matrix[0].is_pressed);
  TEST_ASSERT_EQUAL_UINT8(KEY_DIR_UP, key_matrix[0].key_dir);
}

void test_idle_fast_path_resets_kalman_state(void) {
  // Start with an active key state that is eligible for the idle fast path.
  // The idle fast path requires key_dir == INACTIVE, !is_pressed, and
  // distance == 0.
  key_matrix[0].pos = 0.0f;
  key_matrix[0].velocity = 10.0f;
  key_matrix[0].innovation = 1.0f;
  key_matrix[0].adc_filtered = 2400;
  key_matrix[0].adc_raw = 2400;
  key_matrix[0].distance = 0;
  key_matrix[0].extremum = 50;
  key_matrix[0].key_dir = KEY_DIR_INACTIVE;
  key_matrix[0].is_pressed = false;
  key_matrix[0].rest_stable_since = 0;
  key_matrix[0].bottom_out_hold = 3;
  set_distance_bounds(0, 2400, 3050);

  // Return raw value to rest; idle fast path should take over.
  analog_key_values[0] = 2400;
  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(2400, key_matrix[0].adc_filtered);
  TEST_ASSERT_EQUAL_UINT16(2400, key_matrix[0].adc_raw);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, key_matrix[0].pos);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, key_matrix[0].velocity);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, key_matrix[0].innovation);
  TEST_ASSERT_EQUAL_UINT8(0, key_matrix[0].distance);
  TEST_ASSERT_EQUAL_UINT8(0, key_matrix[0].extremum);
  TEST_ASSERT_EQUAL_UINT16(0, key_matrix[0].bottom_out_hold);
}

void test_rt_disabled_uses_actuation_point_threshold(void) {
  // Disable Rapid Trigger for key 0.
  matrix_disable_rapid_trigger(0, true);

  analog_key_values[0] = 3000;
  for (uint16_t i = 0; i < 8; i++) {
    matrix_scan();
  }

  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);

  // Release partially; with RT disabled, release should only happen when
  // distance drops below the actuation point.
  analog_key_values[0] = 2450;
  for (uint16_t i = 0; i < 8; i++) {
    matrix_scan();
  }

  // 2450 is only slightly above rest, so distance should be below 128.
  TEST_ASSERT_FALSE(key_matrix[0].is_pressed);

  // Re-enable for other tests.
  matrix_disable_rapid_trigger(0, false);
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
  TEST_ASSERT_EQUAL_UINT32(25000u, diag->expected_matrix_scan_hz);
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

void test_matrix_set_kalman_config_rejects_null(void) {
  TEST_ASSERT_FALSE(matrix_set_kalman_config(NULL));
}

void test_matrix_set_kalman_config_rejects_out_of_range_gains(void) {
  const kalman_config_t valid = (kalman_config_t)DEFAULT_KALMAN_CONFIG;
  kalman_config_t cfg = valid;

  cfg.position_gain = 1.1f;
  TEST_ASSERT_FALSE(matrix_set_kalman_config(&cfg));
  cfg.position_gain = -0.1f;
  TEST_ASSERT_FALSE(matrix_set_kalman_config(&cfg));
  cfg.position_gain = valid.position_gain;

  cfg.velocity_gain = 1.1f;
  TEST_ASSERT_FALSE(matrix_set_kalman_config(&cfg));
  cfg.velocity_gain = -0.1f;
  TEST_ASSERT_FALSE(matrix_set_kalman_config(&cfg));
}

void test_matrix_set_kalman_config_rejects_negative_thresholds(void) {
  const kalman_config_t valid = (kalman_config_t)DEFAULT_KALMAN_CONFIG;
  kalman_config_t cfg = valid;

  cfg.velocity_damping = -0.1f;
  TEST_ASSERT_FALSE(matrix_set_kalman_config(&cfg));
  cfg.velocity_damping = valid.velocity_damping;

  cfg.rt_down_min_velocity = -0.1f;
  TEST_ASSERT_FALSE(matrix_set_kalman_config(&cfg));
  cfg.rt_down_min_velocity = valid.rt_down_min_velocity;

  cfg.rt_up_min_velocity = -0.1f;
  TEST_ASSERT_FALSE(matrix_set_kalman_config(&cfg));
  cfg.rt_up_min_velocity = valid.rt_up_min_velocity;

  cfg.innovation_event_threshold = 0.0f;
  TEST_ASSERT_FALSE(matrix_set_kalman_config(&cfg));
}

void test_matrix_set_kalman_config_accepts_valid_values(void) {
  const kalman_config_t cfg = {
      .position_gain = 0.5f,
      .velocity_gain = 0.1f,
      .velocity_damping = 0.95f,
      .rt_down_min_velocity = 0.5f,
      .rt_up_min_velocity = 0.4f,
      .innovation_event_threshold = 6.0f,
      .bottom_out_hold_scans = 6,
      .bottom_out_rt_up = 3,
      .noise_deadzone = 4u,
  };

  TEST_ASSERT_TRUE(matrix_set_kalman_config(&cfg));
  const kalman_config_t *runtime = matrix_get_kalman_config();
  TEST_ASSERT_EQUAL_FLOAT(0.5f, runtime->position_gain);
  TEST_ASSERT_EQUAL_FLOAT(0.1f, runtime->velocity_gain);
  TEST_ASSERT_EQUAL_FLOAT(0.95f, runtime->velocity_damping);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, runtime->rt_down_min_velocity);
  TEST_ASSERT_EQUAL_FLOAT(0.4f, runtime->rt_up_min_velocity);
  TEST_ASSERT_EQUAL_FLOAT(6.0f, runtime->innovation_event_threshold);
  TEST_ASSERT_EQUAL_UINT16(6u, runtime->bottom_out_hold_scans);
  TEST_ASSERT_EQUAL_UINT8(3u, runtime->bottom_out_rt_up);
  TEST_ASSERT_EQUAL_UINT16(4u, runtime->noise_deadzone);
}

void test_ab_filter_in_distance_space(void) {
  // Apply a step input and let the distance-space filter converge.
  analog_key_values[0] = 2700;
  for (uint16_t i = 0; i < 64; i++) {
    matrix_scan();
  }

  // Position and distance should settle near the distance mapped from 2700.
  const uint8_t expected_distance =
      adc_to_distance(2700, key_matrix[0].adc_rest_value,
                      key_matrix[0].adc_bottom_out_value);
  TEST_ASSERT_UINT8_WITHIN(3, expected_distance, key_matrix[0].distance);
  TEST_ASSERT_FLOAT_WITHIN(3.0f, (float)expected_distance, key_matrix[0].pos);
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 0.0f, key_matrix[0].velocity);
}

void test_velocity_damping_on_idle(void) {
  // Put the key in the idle state but with a non-zero velocity estimate.
  key_matrix[0].pos = 0.0f;
  key_matrix[0].velocity = 10.0f;
  key_matrix[0].distance = 0;
  key_matrix[0].key_dir = KEY_DIR_INACTIVE;
  key_matrix[0].is_pressed = false;
  key_matrix[0].bottom_out_hold = 0;
  analog_key_values[0] = 2400;

  for (uint16_t i = 0; i < 20; i++) {
    matrix_scan();
  }

  // Velocity should decay toward zero while the key remains idle.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, key_matrix[0].velocity);
}

void test_bottom_out_hold_counter_decrements(void) {
  key_matrix[0].pos = 200.0f;
  key_matrix[0].velocity = 0.0f;
  key_matrix[0].adc_filtered = 2900;
  key_matrix[0].adc_raw = 2900;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].key_dir = KEY_DIR_DOWN;
  key_matrix[0].is_pressed = true;
  key_matrix[0].bottom_out_hold = 10;
  analog_key_values[0] = 2900;

  matrix_scan();

  TEST_ASSERT_EQUAL_UINT16(9, key_matrix[0].bottom_out_hold);
}

void test_bottom_out_hold_uses_reduced_rt_up(void) {
  // Press key near bottom-out and set an active bottom-out hold.
  key_matrix[0].pos = 200.0f;
  key_matrix[0].velocity = 0.0f;
  key_matrix[0].adc_filtered = 2900;
  key_matrix[0].adc_raw = 2900;
  set_distance_bounds(0, 2400, 3050);
  key_matrix[0].key_dir = KEY_DIR_DOWN;
  key_matrix[0].is_pressed = true;
  key_matrix[0].extremum = 200;
  key_matrix[0].bottom_out_hold = 10;

  // Release by enough distance units to exceed the reduced bottom_out_rt_up
  // (5) while the hold is still active.
  analog_key_values[0] = 2740;
  for (uint16_t i = 0; i < 8; i++) {
    matrix_scan();
  }

  TEST_ASSERT_FALSE(key_matrix[0].is_pressed);
  TEST_ASSERT_EQUAL_UINT8(KEY_DIR_UP, key_matrix[0].key_dir);
}

void test_rt_press_accepts_slow_stroke_regardless_of_velocity_threshold(void) {
  kalman_config_t cfg = *matrix_get_kalman_config();
  cfg.rt_down_min_velocity = 1000.0f;
  TEST_ASSERT_TRUE(matrix_set_kalman_config(&cfg));

  // Ramp one ADC count per scan past the actuation point. A deliberately
  // unreachable legacy velocity threshold proves actuation is position-based.
  for (uint16_t i = 0; i < 400; i++) {
    analog_key_values[0] = (uint16_t)(2400u + i);
    matrix_scan();
  }

  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);
  TEST_ASSERT_EQUAL_UINT8(KEY_DIR_DOWN, key_matrix[0].key_dir);
}

void test_rt_release_accepts_slow_stroke_regardless_of_velocity_threshold(void) {
  // Press the key with a fast stroke.
  analog_key_values[0] = 3000;
  for (uint16_t i = 0; i < 8; i++) {
    matrix_scan();
  }
  TEST_ASSERT_TRUE(key_matrix[0].is_pressed);

  kalman_config_t cfg = *matrix_get_kalman_config();
  cfg.rt_up_min_velocity = 1000.0f;
  TEST_ASSERT_TRUE(matrix_set_kalman_config(&cfg));

  // Release one ADC count per scan. The deliberately unreachable legacy
  // threshold proves RT release is based on displacement from the extremum.
  for (uint16_t i = 0; i < 120; i++) {
    analog_key_values[0] = (uint16_t)(3000u - i);
    matrix_scan();
  }

  TEST_ASSERT_FALSE(key_matrix[0].is_pressed);
  TEST_ASSERT_EQUAL_UINT8(KEY_DIR_UP, key_matrix[0].key_dir);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_matrix_large_delta_press_and_release_stay_responsive);
  RUN_TEST(test_matrix_clips_bottom_out_value_to_adc_maximum);
  RUN_TEST(test_matrix_records_large_scan_intervals_in_microseconds);
  RUN_TEST(test_matrix_continuous_calibration_tracks_small_rest_drift);
  RUN_TEST(test_matrix_continuous_calibration_ignores_large_rest_drift);
  RUN_TEST(test_matrix_continuous_calibration_ignores_unstable_keystroke_motion);
  RUN_TEST(test_kalman_step_response_converges_to_new_setpoint);
  RUN_TEST(test_kalman_ramp_response_tracks_constant_velocity);
  RUN_TEST(test_kalman_noise_rejection_at_rest_clamps_distance);
  RUN_TEST(test_noise_deadzone_clamps_distance_to_zero);
  RUN_TEST(test_bottom_out_detection_arms_release_on_collision);
  RUN_TEST(test_idle_fast_path_resets_kalman_state);
  RUN_TEST(test_rt_disabled_uses_actuation_point_threshold);
  RUN_TEST(test_matrix_task_runs_only_when_a_new_full_scan_is_ready);
  RUN_TEST(test_matrix_task_counts_missed_generations);
  RUN_TEST(test_matrix_set_kalman_config_rejects_null);
  RUN_TEST(test_matrix_set_kalman_config_rejects_out_of_range_gains);
  RUN_TEST(test_matrix_set_kalman_config_rejects_negative_thresholds);
  RUN_TEST(test_matrix_set_kalman_config_accepts_valid_values);
  RUN_TEST(test_ab_filter_in_distance_space);
  RUN_TEST(test_velocity_damping_on_idle);
  RUN_TEST(test_bottom_out_hold_counter_decrements);
  RUN_TEST(test_bottom_out_hold_uses_reduced_rt_up);
  RUN_TEST(test_rt_press_accepts_slow_stroke_regardless_of_velocity_threshold);
  RUN_TEST(test_rt_release_accepts_slow_stroke_regardless_of_velocity_threshold);
  return UNITY_END();
}
