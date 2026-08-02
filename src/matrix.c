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

#include "matrix.h"

#include "analog_scan.h"
#include "distance.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "lib/bitmap.h"
#include "rgb.h"
#include "usb_bootstrap.h"

#define MATRIX_BOTTOM_OUT_SAVE_IDLE_MS 3000u

static kalman_config_t matrix_kalman_config;

__attribute__((always_inline)) static inline float
matrix_kalman_update(key_state_t *state, uint16_t raw_adc) {
  // Average two 32 kHz samples to produce one 16 kHz distance-space sample.
  const uint16_t avg_adc =
      (uint16_t)(((uint32_t)raw_adc + (uint32_t)state->adc_raw) / 2u);

  const float measurement =
      (float)adc_to_distance(avg_adc, state->adc_rest_value,
                             state->adc_bottom_out_value);
  const float predicted_pos = state->pos + state->velocity;
  const float innovation = measurement - predicted_pos;

  state->pos = predicted_pos + matrix_kalman_config.position_gain * innovation;
  state->velocity += matrix_kalman_config.velocity_gain * innovation;
  state->innovation = innovation;

  if (state->pos < 0.0f)
    state->pos = 0.0f;
  else if (state->pos > 255.0f)
    state->pos = 255.0f;

  state->adc_filtered = avg_adc;
  return innovation;
}

__attribute__((always_inline)) static inline uint16_t
matrix_ema_inline(uint16_t sample, uint16_t filtered, uint8_t exponent) {
  return (uint16_t)(((uint32_t)sample +
                     ((uint32_t)filtered * ((1u << exponent) - 1u))) >>
                    exponent);
}

__attribute__((always_inline)) static inline uint16_t
matrix_analog_read(uint8_t key) {
  uint16_t value = 0u;

#if DIGITAL_NUM_INPUTS == 0
#if defined(JOYSTICK_SW_KEY_INDEX) && defined(JOYSTICK_SW_PIN) &&               \
    defined(JOYSTICK_SW_PORT)
  value = key == JOYSTICK_SW_KEY_INDEX ? analog_read(key)
                                       : analog_scan_peek_key(key);
#else
  value = analog_scan_peek_key(key);
#endif
#else
  value = analog_read(key);
#endif

#if defined(MATRIX_INVERT_ADC_VALUES)
  return ADC_MAX_VALUE - value;
#else
  return value;
#endif
}

__attribute__((always_inline)) static inline uint16_t
matrix_bottom_out_value(uint8_t key, uint16_t rest_value) {
  return M_MIN(rest_value +
                   M_MAX(eeconfig->calibration.initial_bottom_out_threshold,
                         eeconfig->bottom_out_threshold[key]),
               ADC_MAX_VALUE);
}

__attribute__((always_inline)) static inline void
matrix_apply_continuous_calibration(uint8_t key, uint16_t sample) {
  key_state_t *state = &key_matrix[key];
  const int32_t diff = (int32_t)sample - (int32_t)state->adc_rest_value;
  const uint16_t diff_abs =
      diff >= 0 ? (uint16_t)diff : (uint16_t)(-diff);

  if (diff_abs < MATRIX_CALIBRATION_EPSILON ||
      diff_abs >= MATRIX_CONTINUOUS_CALIBRATION_RANGE)
    return;

  const uint8_t exponent =
      diff_abs >= MATRIX_CONTINUOUS_CALIBRATION_FAST_DELTA
          ? MATRIX_CONTINUOUS_CALIBRATION_FAST_ALPHA_EXPONENT
          : MATRIX_CONTINUOUS_CALIBRATION_ALPHA_EXPONENT;
  const uint16_t previous_rest = state->adc_rest_value;
  uint16_t new_rest = matrix_ema_inline(sample, previous_rest, exponent);

  // Integer EMA stalls on small deltas, so force 1-step progress once the
  // drift is larger than the calibration noise floor.
  if (new_rest == previous_rest)
    new_rest = diff > 0 ? (uint16_t)(previous_rest + 1)
                        : (uint16_t)(previous_rest - 1);

  const uint16_t bottom_out_threshold =
      state->adc_bottom_out_value > previous_rest
          ? (uint16_t)(state->adc_bottom_out_value - previous_rest)
          : (uint16_t)(matrix_bottom_out_value(key, previous_rest) -
                       previous_rest);

  state->adc_rest_value = new_rest;
  state->adc_bottom_out_value =
      M_MIN((uint32_t)new_rest + bottom_out_threshold, ADC_MAX_VALUE);
}

key_state_t key_matrix[NUM_KEYS];

// Bitmap for tracking which keys have Rapid Trigger disabled
static bitmap_t rapid_trigger_disabled[BITMAP_SIZE(NUM_KEYS)] = {0};
static bitmap_t matrix_pending_rgb_keypresses[BITMAP_SIZE(NUM_KEYS)] = {0};
static uint16_t matrix_bottom_out_threshold_buf[NUM_KEYS];
static uint16_t matrix_rapid_trigger_disabled_count = 0u;
static uint8_t matrix_last_sample_versions[NUM_KEYS];

// Tracks the last time any key state changed
static uint32_t matrix_last_activity_time = 0;
static bool matrix_bottom_out_threshold_dirty = false;
static matrix_scan_diagnostics_t matrix_scan_diagnostics;
static uint32_t matrix_last_scan_start_cycle = 0u;
static bool matrix_last_scan_start_cycle_valid = false;
static uint64_t matrix_scan_interval_cycles_accum = 0u;
static uint32_t matrix_scan_interval_count = 0u;
static uint32_t matrix_last_seen_generation = 0u;
static uint32_t matrix_next_processing_generation = 0u;
static uint32_t matrix_last_housekeeping_tick = 0u;
static bool matrix_last_housekeeping_tick_valid = false;
static uint32_t matrix_last_housekeeping_start_cycle = 0u;
static bool matrix_last_housekeeping_start_cycle_valid = false;

static uint32_t matrix_expected_scan_hz(void);

__attribute__((always_inline)) static inline uint32_t
matrix_cycles_to_us(uint32_t cycles) {
#if defined(F_CPU) && F_CPU > 0
#if (F_CPU % 1000000u) == 0
  return cycles / (uint32_t)(F_CPU / 1000000u);
#else
  const uint64_t cpu_hz = (uint64_t)F_CPU;
  const uint64_t whole_seconds = (uint64_t)cycles / cpu_hz;
  const uint64_t remaining_cycles = (uint64_t)cycles % cpu_hz;
  // Split the conversion so larger cycle deltas do not have to multiply by
  // 1,000,000 before dividing by F_CPU.
  const uint64_t micros = whole_seconds * 1000000ull +
                          (remaining_cycles * 1000000ull) / cpu_hz;
  return micros > UINT32_MAX ? UINT32_MAX : (uint32_t)micros;
#endif
#else
  (void)cycles;
  return 0;
#endif
}

__attribute__((always_inline)) static inline uint32_t
matrix_cycles_to_hz(uint32_t cycles) {
#if defined(F_CPU) && F_CPU > 0
  if (cycles == 0u)
    return 0u;

  return (uint32_t)(((uint64_t)F_CPU + ((uint64_t)cycles / 2ull)) /
                    (uint64_t)cycles);
#else
  (void)cycles;
  return 0u;
#endif
}

static void matrix_refresh_raw_scan_diagnostics(void) {
  const analog_scan_diagnostics_t *analog_diag = analog_get_scan_diagnostics();

  matrix_scan_diagnostics.raw_scan_hz = analog_diag->estimated_scan_hz;
  matrix_scan_diagnostics.last_raw_scan_us = analog_diag->last_scan_us;
  matrix_scan_diagnostics.max_raw_scan_us = analog_diag->max_scan_us;
  matrix_scan_diagnostics.full_scan_generation =
      analog_scan_get_full_scan_generation();
  matrix_scan_diagnostics.matrix_processing_divider =
      MATRIX_PROCESSING_DIVIDER;
  matrix_scan_diagnostics.expected_matrix_scan_hz = matrix_expected_scan_hz();
}

__attribute__((always_inline)) static inline uint32_t
matrix_count_generation_matches(uint32_t start_generation,
                                uint32_t end_generation) {
  if (end_generation <= start_generation)
    return 0u;

  const uint32_t first_generation = start_generation + 1u;
  return end_generation / MATRIX_PROCESSING_DIVIDER -
         ((first_generation - 1u) / MATRIX_PROCESSING_DIVIDER);
}

static uint32_t matrix_next_scheduled_generation_after(uint32_t generation) {
  const uint32_t next_generation = generation + 1u;
  const uint32_t remainder = next_generation % MATRIX_PROCESSING_DIVIDER;
  return remainder == 0u
             ? next_generation
             : next_generation + (MATRIX_PROCESSING_DIVIDER - remainder);
}

static uint32_t matrix_latest_scheduled_generation_at_or_before(
    uint32_t generation) {
  return generation - (generation % MATRIX_PROCESSING_DIVIDER);
}

static uint32_t matrix_expected_scan_hz(void) {
  if (matrix_scan_diagnostics.raw_scan_hz == 0u)
    return 0u;

  return (matrix_scan_diagnostics.raw_scan_hz +
          (MATRIX_PROCESSING_DIVIDER / 2u)) /
         MATRIX_PROCESSING_DIVIDER;
}

#if MATRIX_LIVE_SCAN_TIMING_DIAGNOSTICS
static uint32_t matrix_fast_scan_budget_us(void) {
  if (matrix_scan_diagnostics.last_raw_scan_us != 0u) {
    return matrix_scan_diagnostics.last_raw_scan_us * MATRIX_PROCESSING_DIVIDER;
  }

  if (matrix_scan_diagnostics.raw_scan_hz == 0u)
    return 0u;

  return (uint32_t)(((uint64_t)MATRIX_PROCESSING_DIVIDER * 1000000ull +
                     ((uint64_t)matrix_scan_diagnostics.raw_scan_hz / 2ull)) /
                    (uint64_t)matrix_scan_diagnostics.raw_scan_hz);
}
#endif

static void matrix_account_generation_progress(uint32_t current_generation) {
  if (current_generation <= matrix_last_seen_generation)
    return;

  const uint32_t generation_delta =
      current_generation - matrix_last_seen_generation;
  const uint32_t scheduled_generations =
      matrix_count_generation_matches(matrix_last_seen_generation,
                                      current_generation);

  matrix_scan_diagnostics.intentional_skip_count +=
      generation_delta - scheduled_generations;
  matrix_last_seen_generation = current_generation;
}

static void matrix_dispatch_pending_rgb_keypresses(void) {
#if defined(RGB_ENABLED)
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    if (!bitmap_get(matrix_pending_rgb_keypresses, i))
      continue;

    bitmap_set(matrix_pending_rgb_keypresses, i, false);
    rgb_matrix_record_keypress((uint8_t)i);
  }
#endif
}

static void matrix_run_continuous_calibration_housekeeping(void) {
  if (!eeconfig->options.continuous_calibration)
    return;

  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    key_state_t *state = &key_matrix[i];
    if (state->key_dir != KEY_DIR_INACTIVE || state->is_pressed)
      continue;

    if (timer_elapsed(state->rest_stable_since) <
        MATRIX_CONTINUOUS_CALIBRATION_IDLE_MS)
      continue;

    matrix_apply_continuous_calibration((uint8_t)i, state->adc_filtered);
  }
}

static void matrix_persist_bottom_out_thresholds(void) {
  if (!matrix_bottom_out_threshold_dirty ||
      !eeconfig->options.save_bottom_out_threshold ||
      matrix_get_idle_time() < MATRIX_BOTTOM_OUT_SAVE_IDLE_MS) {
    return;
  }

  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    if (key_matrix[i].adc_bottom_out_value < key_matrix[i].adc_rest_value)
      matrix_bottom_out_threshold_buf[i] = 0;
    else
      matrix_bottom_out_threshold_buf[i] =
          key_matrix[i].adc_bottom_out_value - key_matrix[i].adc_rest_value;
  }

  if (EECONFIG_WRITE(bottom_out_threshold, matrix_bottom_out_threshold_buf))
    matrix_bottom_out_threshold_dirty = false;
}

static void matrix_recalibrate_internal(bool reset_bottom_out_threshold,
                                         bool service_usb_during_calibration) {
  matrix_kalman_config = eeconfig->kalman_config;

  if (reset_bottom_out_threshold) {
    memset(matrix_bottom_out_threshold_buf, 0,
           sizeof(matrix_bottom_out_threshold_buf));
    EECONFIG_WRITE(bottom_out_threshold, matrix_bottom_out_threshold_buf);
  }

  memset(rapid_trigger_disabled, 0, sizeof(rapid_trigger_disabled));
  memset(matrix_pending_rgb_keypresses, 0, sizeof(matrix_pending_rgb_keypresses));
  memset(&matrix_scan_diagnostics, 0, sizeof(matrix_scan_diagnostics));
  matrix_rapid_trigger_disabled_count = 0u;
  memset(matrix_last_sample_versions, 0, sizeof(matrix_last_sample_versions));
  matrix_last_activity_time = 0;
  matrix_bottom_out_threshold_dirty = false;
  matrix_last_scan_start_cycle_valid = false;
  matrix_scan_interval_cycles_accum = 0u;
  matrix_scan_interval_count = 0u;
  matrix_last_housekeeping_tick_valid = false;
  matrix_last_housekeeping_start_cycle_valid = false;

  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    key_matrix[i].adc_raw = eeconfig->calibration.initial_rest_value;
    key_matrix[i].adc_filtered = eeconfig->calibration.initial_rest_value;
    key_matrix[i].adc_rest_value = eeconfig->calibration.initial_rest_value;
    key_matrix[i].adc_bottom_out_value = matrix_bottom_out_value(
        i, eeconfig->calibration.initial_rest_value);
    key_matrix[i].pos = 0.0f;
    key_matrix[i].velocity = 0.0f;
    key_matrix[i].innovation = 0.0f;
    key_matrix[i].distance = 0;
    key_matrix[i].extremum = 0;
    key_matrix[i].key_dir = KEY_DIR_INACTIVE;
    key_matrix[i].is_pressed = false;
    key_matrix[i].rest_stable_since = 0;
    key_matrix[i].event_time = 0;
    key_matrix[i].bottom_out_hold = 0;
  }

  // We only calibrate the rest value. The bottom-out value will be updated
  // during the scan process.
  const uint32_t calibration_start = timer_read();
  while (timer_elapsed(calibration_start) < MATRIX_CALIBRATION_DURATION) {
    if (service_usb_during_calibration)
      usb_bootstrap_pump();

    // Run the analog task to possibly update the ADC values
    analog_task();

    for (uint32_t i = 0; i < NUM_KEYS; i++) {
      const uint16_t raw_adc = matrix_analog_read(i);
      const uint16_t new_adc_filtered =
          matrix_ema_inline(raw_adc, key_matrix[i].adc_filtered, 4);

      key_matrix[i].adc_raw = raw_adc;
      key_matrix[i].adc_filtered = new_adc_filtered;

      if (new_adc_filtered + MATRIX_CALIBRATION_EPSILON <=
          key_matrix[i].adc_rest_value)
        // Only update the rest value if the new value is smaller and the
        // difference is at least the calibration epsilon
        key_matrix[i].adc_rest_value = new_adc_filtered;

      // Update the bottom-out value to be the minimum bottom-out value based on
      // the updated rest value
      key_matrix[i].adc_bottom_out_value =
          matrix_bottom_out_value(i, key_matrix[i].adc_rest_value);
    }
  }

  matrix_last_seen_generation = analog_scan_get_full_scan_generation();
  matrix_next_processing_generation =
      matrix_next_scheduled_generation_after(matrix_last_seen_generation);
#if ANALOG_SCAN_KEY_VERSION_DELTA > 0
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    matrix_last_sample_versions[i] = analog_scan_peek_key_version((uint8_t)i);
  }
#endif
  matrix_refresh_raw_scan_diagnostics();
}

void matrix_init(void) { matrix_recalibrate_internal(false, true); }

const kalman_config_t *matrix_get_kalman_config(void) {
  return &matrix_kalman_config;
}

bool matrix_set_kalman_config(const kalman_config_t *config) {
  if (config == NULL)
    return false;

  if (config->position_gain < 0.0f || config->position_gain > 1.0f)
    return false;
  if (config->velocity_gain < 0.0f || config->velocity_gain > 1.0f)
    return false;
  if (config->velocity_damping < 0.0f || config->velocity_damping > 1.0f)
    return false;
  if (config->rt_down_min_velocity < 0.0f)
    return false;
  if (config->rt_up_min_velocity < 0.0f)
    return false;
  if (config->innovation_event_threshold <= 0.0f)
    return false;
  if (config->noise_deadzone > ADC_MAX_VALUE)
    return false;

  matrix_kalman_config = *config;
  return EECONFIG_WRITE(kalman_config, config);
}

void matrix_recalibrate(bool reset_bottom_out_threshold) {
  matrix_recalibrate_internal(reset_bottom_out_threshold, false);
}

void matrix_scan_fast(void) {
  const uint32_t scan_time = timer_read();
  const uint32_t scan_cycle_start = board_cycle_count();
  const actuation_t *actuation_map = CURRENT_PROFILE.actuation_map;
#if MATRIX_DETAILED_SCAN_DIAGNOSTICS
  uint16_t max_sample_delta = 0;
  uint16_t max_sample_velocity = 0;
#endif

  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    key_state_t *state = &key_matrix[i];
#if ANALOG_SCAN_KEY_VERSION_DELTA > 0
    const uint8_t sample_version = analog_scan_peek_key_version((uint8_t)i);
    if (state->key_dir == KEY_DIR_INACTIVE && !state->is_pressed &&
        state->distance == 0u &&
        sample_version == matrix_last_sample_versions[i]) {
      continue;
    }
    matrix_last_sample_versions[i] = sample_version;
#endif
    const uint16_t previous_filtered = state->adc_filtered;
    const uint16_t raw_adc = matrix_analog_read((uint8_t)i);
    const actuation_t *actuation = &actuation_map[i];
    uint16_t new_adc_filtered = previous_filtered;
    float innovation = 0.0f;

    // Kalman Fast Path: key is fully idle at rest.
    const uint16_t idle_fast_path_rest_limit =
        (uint16_t)M_MIN((uint32_t)state->adc_rest_value +
                            (uint32_t)MATRIX_IDLE_RAW_FAST_PATH_MARGIN,
                        (uint32_t)ADC_MAX_VALUE);
    if (state->key_dir == KEY_DIR_INACTIVE && !state->is_pressed &&
        state->distance == 0u && raw_adc <= idle_fast_path_rest_limit) {
      const uint16_t filtered_delta =
          raw_adc > previous_filtered ? (uint16_t)(raw_adc - previous_filtered)
                                      : (uint16_t)(previous_filtered - raw_adc);
#if MATRIX_DETAILED_SCAN_DIAGNOSTICS
      const uint16_t previous_raw = state->adc_raw;
#endif
      state->adc_raw = raw_adc;
      state->adc_filtered = raw_adc;
      state->pos = 0.0f;
      state->velocity = 0.0f;
      state->innovation = 0.0f;
      state->bottom_out_hold = 0;
      state->distance = 0u;
      state->extremum = 0;
#if MATRIX_DETAILED_SCAN_DIAGNOSTICS
      if (filtered_delta > max_sample_delta)
        max_sample_delta = filtered_delta;
      const uint16_t sample_velocity =
          raw_adc > previous_raw ? (uint16_t)(raw_adc - previous_raw)
                                 : (uint16_t)(previous_raw - raw_adc);
      if (sample_velocity > max_sample_velocity)
        max_sample_velocity = sample_velocity;
#endif
      if (filtered_delta >= MATRIX_CONTINUOUS_CALIBRATION_STABLE_DELTA)
        state->rest_stable_since = scan_time;
      continue;
    }

    // Kalman filter update for active keys.
    const float velocity_before_update = state->velocity;
    innovation = matrix_kalman_update(state, raw_adc);
    state->adc_raw = raw_adc;
    new_adc_filtered = state->adc_filtered;
    const uint16_t filtered_delta =
        new_adc_filtered > previous_filtered
            ? (uint16_t)(new_adc_filtered - previous_filtered)
            : (uint16_t)(previous_filtered - new_adc_filtered);

#if MATRIX_DETAILED_SCAN_DIAGNOSTICS
    if (filtered_delta > max_sample_delta)
      max_sample_delta = filtered_delta;
    const float innovation_abs = innovation < 0.0f ? -innovation : innovation;
    const uint16_t sample_velocity =
        innovation_abs != innovation_abs ||
                innovation_abs >= (float)UINT16_MAX
            ? UINT16_MAX
            : (uint16_t)innovation_abs;
    if (sample_velocity > max_sample_velocity)
      max_sample_velocity = sample_velocity;
#endif

    if (new_adc_filtered >=
        state->adc_bottom_out_value + MATRIX_CALIBRATION_EPSILON) {
      // Only update the bottom-out value if the new value is larger and the
      // difference is at least the calibration epsilon.
      state->adc_bottom_out_value =
          (uint16_t)M_MIN((uint32_t)new_adc_filtered, ADC_MAX_VALUE);
      matrix_bottom_out_threshold_dirty = true;
    }

    if (new_adc_filtered <=
        state->adc_rest_value + matrix_kalman_config.noise_deadzone) {
      state->pos = 0.0f;
      state->distance = 0;
    } else {
      state->distance = (uint8_t)state->pos;
    }

    const bool was_pressed = state->is_pressed;

    const bool rt_disabled =
        matrix_rapid_trigger_disabled_count != 0u &&
        bitmap_get(rapid_trigger_disabled, i);

    if (rt_disabled || actuation->rt_down == 0u) {
      state->key_dir = KEY_DIR_INACTIVE;
      state->is_pressed = (state->distance >= actuation->actuation_point);
    } else {
      const uint8_t reset_point =
          actuation->continuous ? 0 : actuation->actuation_point;
      const uint8_t rt_up =
          actuation->rt_up == 0 ? actuation->rt_down : actuation->rt_up;

      // Only damp residual velocity at the physical rest position. Damping an
      // inactive key throughout its stroke suppresses legitimate slow motion
      // before the initial actuation point is reached.
      if (state->key_dir == KEY_DIR_INACTIVE && !state->is_pressed &&
          state->distance == 0u) {
        state->velocity *= matrix_kalman_config.velocity_damping;
      }

      // Bottom-out collision detection and hold state.
      uint8_t effective_rt_up = rt_up;
      // A large negative innovation is treated as a bottom-out collision only
      // when the key is near the bottom and was moving downward before the
      // filter update. This prevents a fast release from being mistaken for a
      // plate collision.
      const bool near_bottom =
          state->pos >= MATRIX_BOTTOM_OUT_DETECTION_MIN_POSITION;
      const bool moving_down = velocity_before_update > 0.0f;
      const bool large_negative_innovation =
          innovation < -matrix_kalman_config.innovation_event_threshold;
      const bool bottom_out_collision =
          near_bottom && moving_down && large_negative_innovation;

      if (state->bottom_out_hold > 0u) {
        state->bottom_out_hold--;
        effective_rt_up = matrix_kalman_config.bottom_out_rt_up;
        state->velocity *= matrix_kalman_config.velocity_damping;
      } else if (bottom_out_collision) {
        // This counter is the number of additional scans after this collision
        // scan for which bottom-out release hysteresis remains active.
        state->bottom_out_hold = matrix_kalman_config.bottom_out_hold_scans;
        effective_rt_up = matrix_kalman_config.bottom_out_rt_up;
        state->velocity *= matrix_kalman_config.velocity_damping;
      }

      switch (state->key_dir) {
      case KEY_DIR_INACTIVE:
        if (state->pos >= actuation->actuation_point) {
          // Initial actuation is position-based so arbitrarily slow valid
          // strokes are not rejected by a per-scan velocity threshold.
          state->extremum = (uint8_t)state->pos;
          state->key_dir = KEY_DIR_DOWN;
          state->is_pressed = true;
        }
        break;

      case KEY_DIR_DOWN:
        if (state->pos <= reset_point) {
          // Released past reset point
          state->extremum = (uint8_t)state->pos;
          state->key_dir = KEY_DIR_INACTIVE;
          state->is_pressed = false;
        } else if (state->pos + effective_rt_up < state->extremum) {
          // The displacement from the down extremum already establishes the
          // release direction and provides spatial noise hysteresis.
          state->extremum = (uint8_t)state->pos;
          state->key_dir = KEY_DIR_UP;
          state->is_pressed = false;
        } else if (state->pos > state->extremum) {
          // Pressed down further
          state->extremum = (uint8_t)state->pos;
        }
        break;

      case KEY_DIR_UP:
        if (state->pos <= reset_point) {
          // Released past reset point
          state->extremum = (uint8_t)state->pos;
          state->key_dir = KEY_DIR_INACTIVE;
          state->is_pressed = false;
        } else if (state->extremum + actuation->rt_down < state->pos) {
          // The displacement from the up extremum establishes the press
          // direction without rejecting slow re-presses.
          state->extremum = (uint8_t)state->pos;
          state->key_dir = KEY_DIR_DOWN;
          state->is_pressed = true;
        } else if (state->pos < state->extremum) {
          // Released further
          state->extremum = (uint8_t)state->pos;
        }
        break;

      default:
        break;
      }
    }

    if (state->key_dir != KEY_DIR_INACTIVE || state->is_pressed ||
        filtered_delta >= MATRIX_CONTINUOUS_CALIBRATION_STABLE_DELTA) {
      state->rest_stable_since = scan_time;
    }

    // Record the time when the key state changes. This is used by
    // layout_task to process key events in chronological order instead of
    // preventing key input swapping on simultaneous presses.
    if (state->is_pressed != was_pressed) {
      state->event_time = scan_time;
      matrix_last_activity_time = scan_time;
      if (state->is_pressed)
        bitmap_set(matrix_pending_rgb_keypresses, i, true);
    }
  }

  const uint32_t scan_cycles = board_cycle_count() - scan_cycle_start;
  matrix_scan_diagnostics.scan_count++;
  matrix_scan_diagnostics.last_scan_cycles = scan_cycles;
  matrix_scan_diagnostics.last_scan_us = matrix_cycles_to_us(scan_cycles);
#if MATRIX_DETAILED_SCAN_DIAGNOSTICS
  matrix_scan_diagnostics.max_sample_delta = max_sample_delta;
  matrix_scan_diagnostics.max_sample_velocity = max_sample_velocity;
  memset(matrix_scan_diagnostics.reserved_filter_mode, 0,
         sizeof(matrix_scan_diagnostics.reserved_filter_mode));
  matrix_scan_diagnostics.idle_keys_detected = 0u;
  matrix_scan_diagnostics.active_keys_detected = 0u;
#else
  matrix_scan_diagnostics.max_sample_delta = 0u;
  matrix_scan_diagnostics.max_sample_velocity = 0u;
  memset(matrix_scan_diagnostics.reserved_filter_mode, 0,
         sizeof(matrix_scan_diagnostics.reserved_filter_mode));
  matrix_scan_diagnostics.idle_keys_detected = 0u;
  matrix_scan_diagnostics.active_keys_detected = 0u;
#endif
  if (MATRIX_LIVE_SCAN_TIMING_DIAGNOSTICS && matrix_last_scan_start_cycle_valid) {
    matrix_scan_interval_cycles_accum +=
        (uint64_t)(scan_cycle_start - matrix_last_scan_start_cycle);
    if (matrix_scan_interval_count < UINT32_MAX)
      matrix_scan_interval_count++;
    if (matrix_scan_interval_cycles_accum != 0u) {
      matrix_scan_diagnostics.matrix_scan_hz =
          (uint32_t)((((uint64_t)F_CPU *
                       (uint64_t)matrix_scan_interval_count) +
                      (matrix_scan_interval_cycles_accum / 2ull)) /
                     matrix_scan_interval_cycles_accum);
    }
  }
  matrix_last_scan_start_cycle = scan_cycle_start;
  matrix_last_scan_start_cycle_valid = true;
#if MATRIX_LIVE_SCAN_TIMING_DIAGNOSTICS
  matrix_refresh_raw_scan_diagnostics();
  const uint32_t fast_budget_us = matrix_fast_scan_budget_us();
  if (fast_budget_us != 0u &&
      matrix_scan_diagnostics.last_scan_us > fast_budget_us) {
    matrix_scan_diagnostics.matrix_fast_overrun_count++;
  }
#endif

  if (scan_cycles > matrix_scan_diagnostics.max_scan_cycles) {
    matrix_scan_diagnostics.max_scan_cycles = scan_cycles;
    matrix_scan_diagnostics.max_scan_us = matrix_scan_diagnostics.last_scan_us;
  }
}

static void matrix_scan_housekeeping_internal(bool force) {
  const uint32_t housekeeping_tick = timer_read();
  if (!force && matrix_last_housekeeping_tick_valid &&
      housekeeping_tick - matrix_last_housekeeping_tick <
          MATRIX_HOUSEKEEPING_INTERVAL_MS) {
    return;
  }

  const uint32_t housekeeping_cycle_start = board_cycle_count();

  matrix_dispatch_pending_rgb_keypresses();
  matrix_run_continuous_calibration_housekeeping();
  matrix_persist_bottom_out_thresholds();

  const uint32_t housekeeping_cycles =
      board_cycle_count() - housekeeping_cycle_start;
  matrix_scan_diagnostics.last_housekeeping_us =
      matrix_cycles_to_us(housekeeping_cycles);
  if (housekeeping_cycles > 0u &&
      matrix_last_housekeeping_start_cycle_valid) {
    matrix_scan_diagnostics.matrix_housekeeping_hz =
        matrix_cycles_to_hz(housekeeping_cycle_start -
                            matrix_last_housekeeping_start_cycle);
  }
  if (matrix_scan_diagnostics.last_housekeeping_us >
      matrix_scan_diagnostics.max_housekeeping_us) {
    matrix_scan_diagnostics.max_housekeeping_us =
        matrix_scan_diagnostics.last_housekeeping_us;
  }

  matrix_last_housekeeping_start_cycle = housekeeping_cycle_start;
  matrix_last_housekeeping_start_cycle_valid = true;
  matrix_last_housekeeping_tick = housekeeping_tick;
  matrix_last_housekeeping_tick_valid = true;
}

void matrix_scan_housekeeping(void) {
  matrix_scan_housekeeping_internal(false);
}

void matrix_scan(void) {
  matrix_scan_fast();
  matrix_scan_housekeeping_internal(false);
}

void matrix_task(void) {
  uint32_t current_generation = analog_scan_get_full_scan_generation();

  if (current_generation < matrix_last_seen_generation) {
    matrix_last_seen_generation = current_generation;
    matrix_next_processing_generation =
        matrix_next_scheduled_generation_after(current_generation);
    matrix_refresh_raw_scan_diagnostics();
    return;
  }

  matrix_account_generation_progress(current_generation);
  matrix_scan_diagnostics.missed_generation_count =
      matrix_scan_diagnostics.overload_missed_generation_count;

  if (current_generation < matrix_next_processing_generation) {
    matrix_scan_diagnostics.skipped_main_loop_count++;
    matrix_refresh_raw_scan_diagnostics();
    return;
  }

  const uint32_t due_generation = matrix_next_processing_generation;
  const uint32_t latest_scheduled_generation =
      matrix_latest_scheduled_generation_at_or_before(current_generation);
  const uint32_t scheduled_due_count =
      ((latest_scheduled_generation - due_generation) /
       MATRIX_PROCESSING_DIVIDER) +
      1u;
  const uint32_t coalesced_generations = current_generation - due_generation;

  if (coalesced_generations != 0u)
    matrix_scan_diagnostics.coalesced_generation_count +=
        coalesced_generations;

  if (scheduled_due_count > 1u) {
    matrix_scan_diagnostics.overload_missed_generation_count +=
        scheduled_due_count - 1u;
    matrix_scan_diagnostics.missed_generation_count =
        matrix_scan_diagnostics.overload_missed_generation_count;
  }

  // Latest-snapshot scheduling intentionally coalesces overdue generations
  // into one fast pass; do not replay matrix_scan_fast() against old samples.
  matrix_scan_fast();
  matrix_next_processing_generation =
      latest_scheduled_generation + MATRIX_PROCESSING_DIVIDER;

  if (MATRIX_SCHEDULER_BUDGET_US != 0u &&
      matrix_scan_diagnostics.last_scan_us > MATRIX_SCHEDULER_BUDGET_US) {
    matrix_scan_diagnostics.scheduler_budget_exhausted_count++;
  }

  current_generation = analog_scan_get_full_scan_generation();
  if (current_generation < matrix_last_seen_generation) {
    matrix_last_seen_generation = current_generation;
    matrix_next_processing_generation =
        matrix_next_scheduled_generation_after(current_generation);
  } else if (current_generation >= matrix_next_processing_generation) {
    matrix_scan_diagnostics.scheduler_budget_exhausted_count++;
  }

  matrix_refresh_raw_scan_diagnostics();
}

bool matrix_snapshot_key_pressed(uint8_t key) {
  return key < NUM_KEYS ? key_matrix[key].is_pressed : false;
}

void matrix_disable_rapid_trigger(uint8_t key, bool disable) {
  if (key >= NUM_KEYS)
    return;

  const bool was_disabled = bitmap_get(rapid_trigger_disabled, key);
  if (was_disabled == disable)
    return;

  bitmap_set(rapid_trigger_disabled, key, disable);
  if (disable) {
    if (matrix_rapid_trigger_disabled_count < UINT16_MAX)
      matrix_rapid_trigger_disabled_count++;
  } else if (matrix_rapid_trigger_disabled_count != 0u) {
    matrix_rapid_trigger_disabled_count--;
  }
}

uint32_t matrix_get_idle_time(void) {
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    if (matrix_snapshot_key_pressed((uint8_t)i)) {
      return 0; // Not idle if any key is held
    }
  }
  return timer_elapsed(matrix_last_activity_time);
}

void matrix_refresh_scan_diagnostics(void) {
  matrix_refresh_raw_scan_diagnostics();
  matrix_scan_diagnostics.missed_generation_count =
      matrix_scan_diagnostics.overload_missed_generation_count;
#if !MATRIX_LIVE_SCAN_TIMING_DIAGNOSTICS
  matrix_scan_diagnostics.matrix_scan_hz =
      matrix_scan_diagnostics.expected_matrix_scan_hz;
#endif
}

const matrix_scan_diagnostics_t *matrix_get_scan_diagnostics(void) {
  return &matrix_scan_diagnostics;
}

void matrix_reset_scan_diagnostics(void) {
  memset(&matrix_scan_diagnostics, 0, sizeof(matrix_scan_diagnostics));
  matrix_last_scan_start_cycle_valid = false;
  matrix_scan_interval_cycles_accum = 0u;
  matrix_scan_interval_count = 0u;
  matrix_last_housekeeping_tick_valid = false;
  matrix_last_housekeeping_start_cycle_valid = false;
  matrix_last_seen_generation = analog_scan_get_full_scan_generation();
  matrix_next_processing_generation =
      matrix_next_scheduled_generation_after(matrix_last_seen_generation);
  matrix_refresh_raw_scan_diagnostics();
}
