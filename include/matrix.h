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

#pragma once

#include "common.h"

//--------------------------------------------------------------------+
// Key Matrix Configuration
//--------------------------------------------------------------------+

#if !defined(MATRIX_CALIBRATION_DURATION)
// Duration of the calibration process in milliseconds
#define MATRIX_CALIBRATION_DURATION 500
#endif

#if !defined(MATRIX_KALMAN_POSITION_GAIN)
// Position gain applied to the Kalman filter innovation term. This controls
// how quickly the estimated position follows the raw ADC value. Higher values
// improve responsiveness but pass more noise through.
#define MATRIX_KALMAN_POSITION_GAIN 0.35f
#endif

#if !defined(MATRIX_KALMAN_VELOCITY_GAIN)
// Velocity gain applied to the Kalman filter innovation term. This controls
// how quickly the estimated velocity follows observed changes. Lower values
// produce smoother velocity estimates at the cost of tracking delay.
#define MATRIX_KALMAN_VELOCITY_GAIN 0.05f
#endif

#if !defined(MATRIX_KALMAN_VELOCITY_DAMPING)
// Multiplier applied to the estimated velocity when the key is at rest or when a
// bottom-out event is held. This reduces drift in the velocity estimate from
// sensor noise and prevents the release velocity threshold from being met by
// residual jitter.
#define MATRIX_KALMAN_VELOCITY_DAMPING 0.90f
#endif

#if !defined(MATRIX_RT_DOWN_MIN_VELOCITY)
// Minimum downward velocity (distance units per scan) required to arm a Rapid
// Trigger press or re-press. Filtering out very slow drift prevents accidental
// actuations near the actuation point.
#define MATRIX_RT_DOWN_MIN_VELOCITY 0.3f
#endif

#if !defined(MATRIX_RT_UP_MIN_VELOCITY)
// Minimum upward velocity (distance units per scan, positive magnitude) required
// to arm a Rapid Trigger release. Filtering out very slow drift prevents
// accidental releases when the finger is resting near the bottom-out point.
#define MATRIX_RT_UP_MIN_VELOCITY 0.3f
#endif

#if !defined(MATRIX_INNOVATION_EVENT_THRESHOLD)
// Fixed innovation threshold (distance units) used to detect a bottom-out
// collision. When the key slams into the plate the prediction overshoots and the
// innovation becomes a large negative spike.
#define MATRIX_INNOVATION_EVENT_THRESHOLD 5.0f
#endif

#if !defined(MATRIX_BOTTOM_OUT_DETECTION_MIN_POSITION)
// Distance-space position above which a negative innovation may be treated as
// a bottom-out collision. Position is normalized to the range 0-255.
#define MATRIX_BOTTOM_OUT_DETECTION_MIN_POSITION 224.0f
#endif

#if !defined(MATRIX_BOTTOM_OUT_HOLD_SCANS)
// Number of scans after a bottom-out event during which the release threshold is
// temporarily reduced and velocity is damped.
#define MATRIX_BOTTOM_OUT_HOLD_SCANS 4
#endif

#if !defined(MATRIX_BOTTOM_OUT_RT_UP)
// Reduced Rapid Trigger release distance used while a bottom-out hold is active.
#define MATRIX_BOTTOM_OUT_RT_UP 5
#endif

#if !defined(MATRIX_NOISE_DEADZONE)
// ADC units above the rest value that are treated as noise floor and clamped
// to zero distance. Used to prevent small sensor noise near rest from creating
// non-zero distance readings.
#define MATRIX_NOISE_DEADZONE 2
#endif

#if !defined(MATRIX_CALIBRATION_EPSILON)
// Minimum change in ADC values required to update the calibration values. This
// is used to mitigate the inconsistency of the Hall effect sensors.
#define MATRIX_CALIBRATION_EPSILON 5
#endif

#if !defined(MATRIX_CONTINUOUS_CALIBRATION_RANGE)
// Maximum ADC drift from the current rest baseline that continuous
// auto-calibration will track while a key is idle.
#define MATRIX_CONTINUOUS_CALIBRATION_RANGE 50
#endif

#if !defined(MATRIX_CONTINUOUS_CALIBRATION_ALPHA_EXPONENT)
// Baseline EMA used for small, stable rest drift.
#define MATRIX_CONTINUOUS_CALIBRATION_ALPHA_EXPONENT 6
#endif

#if !defined(MATRIX_CONTINUOUS_CALIBRATION_FAST_ALPHA_EXPONENT)
// Faster baseline EMA used once the rest drift becomes more noticeable.
#define MATRIX_CONTINUOUS_CALIBRATION_FAST_ALPHA_EXPONENT 4
#endif

#if !defined(MATRIX_CONTINUOUS_CALIBRATION_FAST_DELTA)
// Minimum ADC drift required to switch to the faster continuous calibration
// path.
#define MATRIX_CONTINUOUS_CALIBRATION_FAST_DELTA 24
#endif

#if !defined(MATRIX_CONTINUOUS_CALIBRATION_STABLE_DELTA)
// Maximum filtered ADC movement allowed while considering a key stable at
// rest. A value of 1 means any filtered step resets the settle timer.
#define MATRIX_CONTINUOUS_CALIBRATION_STABLE_DELTA 1
#endif

#if !defined(MATRIX_CONTINUOUS_CALIBRATION_IDLE_MS)
// Continuous auto-calibration only runs after the key has been stably idle for
// this long.
#define MATRIX_CONTINUOUS_CALIBRATION_IDLE_MS 200
#endif

#if !defined(MATRIX_INACTIVITY_TIMEOUT)
// Inactivity timeout in milliseconds. Bottom-out threshold will be saved after
// there is no change to the threshold of any key for this duration.
#define MATRIX_INACTIVITY_TIMEOUT 3000
#endif

#if !defined(MATRIX_HOUSEKEEPING_INTERVAL_MS)
// Run slower calibration/persistence housekeeping at up to this cadence.
#define MATRIX_HOUSEKEEPING_INTERVAL_MS 1
#endif

#if !defined(MATRIX_PROCESSING_DIVIDER)
// Run matrix processing once per this many completed raw full scans.
#define MATRIX_PROCESSING_DIVIDER 1
#endif

#if !defined(MATRIX_SCHEDULER_MAX_CATCHUP_SCANS)
// Legacy catch-up cap kept for compatibility with older builds. Latest-snapshot
// matrix scheduling now coalesces due generations into a single fast scan.
#define MATRIX_SCHEDULER_MAX_CATCHUP_SCANS 2
#endif

#if !defined(MATRIX_SCHEDULER_BUDGET_US)
// Soft time budget used for overload diagnostics in matrix_task().
#define MATRIX_SCHEDULER_BUDGET_US 250
#endif

#if !defined(MATRIX_DETAILED_SCAN_DIAGNOSTICS)
// Collect per-key filter diagnostics during every matrix fast scan.
#define MATRIX_DETAILED_SCAN_DIAGNOSTICS 0
#endif

#if !defined(MATRIX_LIVE_SCAN_TIMING_DIAGNOSTICS)
// Update matrix scan-rate timing diagnostics during every matrix fast scan.
#define MATRIX_LIVE_SCAN_TIMING_DIAGNOSTICS 1
#endif

#if !defined(MATRIX_IDLE_RAW_FAST_PATH_MARGIN)
// Treat fully idle keys within this ADC margin above rest as distance 0.
#define MATRIX_IDLE_RAW_FAST_PATH_MARGIN 0
#endif

_Static_assert(MATRIX_PROCESSING_DIVIDER > 0,
               "MATRIX_PROCESSING_DIVIDER must be greater than zero");
_Static_assert(MATRIX_SCHEDULER_MAX_CATCHUP_SCANS > 0,
               "MATRIX_SCHEDULER_MAX_CATCHUP_SCANS must be greater than zero");

#if !defined(DEFAULT_KALMAN_CONFIG)
// Default runtime Kalman filter configuration. The values are taken from the
// compile-time macros so board-specific tuning can still override defaults.
#define DEFAULT_KALMAN_CONFIG                                                  \
  {                                                                            \
      .position_gain = MATRIX_KALMAN_POSITION_GAIN,                            \
      .velocity_gain = MATRIX_KALMAN_VELOCITY_GAIN,                            \
      .velocity_damping = MATRIX_KALMAN_VELOCITY_DAMPING,                      \
      .rt_down_min_velocity = MATRIX_RT_DOWN_MIN_VELOCITY,                     \
      .rt_up_min_velocity = MATRIX_RT_UP_MIN_VELOCITY,                         \
      .innovation_event_threshold = MATRIX_INNOVATION_EVENT_THRESHOLD,         \
      .bottom_out_hold_scans = MATRIX_BOTTOM_OUT_HOLD_SCANS,                   \
      .bottom_out_rt_up = MATRIX_BOTTOM_OUT_RT_UP,                             \
      .noise_deadzone = MATRIX_NOISE_DEADZONE,                                 \
  }
#endif

//--------------------------------------------------------------------+
// Kalman Filter Configuration
//--------------------------------------------------------------------+

typedef struct __attribute__((packed)) {
  // Position gain applied to the Kalman filter innovation term (0.0-1.0)
  float position_gain;
  // Velocity gain applied to the Kalman filter innovation term (0.0-1.0)
  float velocity_gain;
  // Multiplier applied to the estimated velocity while at rest or holding a
  // bottom-out event. Values in the range [0.0, 1.0] reduce drift.
  float velocity_damping;
  // Legacy compatibility field. RT press and re-press are position-based, so
  // this value is retained in the persisted/wire format but is not used for
  // event gating.
  float rt_down_min_velocity;
  // Legacy compatibility field. RT release direction is established by
  // displacement from the tracked extremum, so this value is not used for
  // event gating.
  float rt_up_min_velocity;
  // Fixed innovation threshold (distance units) used to detect a bottom-out
  // collision
  float innovation_event_threshold;
  // Number of additional scans after the collision-detection scan during which
  // the release threshold is temporarily reduced and velocity is damped.
  uint16_t bottom_out_hold_scans;
  // Reduced Rapid Trigger release distance used while a bottom-out hold is active
  uint8_t bottom_out_rt_up;
  // ADC units above rest treated as noise floor and clamped to zero distance
  uint16_t noise_deadzone;
} kalman_config_t;

//--------------------------------------------------------------------+
// Key Matrix
//--------------------------------------------------------------------+

typedef enum {
  KEY_DIR_INACTIVE = 0,
  KEY_DIR_DOWN,
  KEY_DIR_UP,
} key_dir_t;

// Key state
typedef struct {
  // Most recent raw ADC value
  uint16_t adc_raw;
  // Filtered ADC value
  uint16_t adc_filtered;
  // ADC value when the key is fully released
  uint16_t adc_rest_value;
  // ADC value when the key is fully pressed
  uint16_t adc_bottom_out_value;
  // Estimated position from the Kalman filter (distance units, 0-255)
  float pos;
  // Estimated velocity from the Kalman filter (distance units per scan)
  float velocity;
  // Most recent innovation term (measurement - prediction, distance units)
  float innovation;
  // Key travel distance (0-255)
  uint8_t distance;
  // Last extremum point of the key travel distance (0-255)
  uint8_t extremum;
  // Current key travel direction
  uint8_t key_dir;
  // Whether the key is pressed
  bool is_pressed;
  // Timestamp when the key last left a stable resting state
  uint32_t rest_stable_since;
  // Timestamp when is_pressed last changed (used for event ordering)
  uint32_t event_time;
  // Scans remaining in the bottom-out hold state
  uint16_t bottom_out_hold;
} key_state_t;

// Key matrix
extern key_state_t key_matrix[NUM_KEYS];

typedef struct {
  uint32_t scan_count;
  uint32_t last_scan_cycles;
  uint32_t max_scan_cycles;
  uint32_t last_scan_us;
  uint32_t max_scan_us;
  uint16_t max_sample_delta;
  uint16_t max_sample_velocity;
  uint16_t reserved_filter_mode[4];
  uint16_t idle_keys_detected;
  uint16_t active_keys_detected;
  uint32_t raw_scan_hz;
  uint32_t last_raw_scan_us;
  uint32_t max_raw_scan_us;
  uint32_t full_scan_generation;
  uint32_t matrix_scan_hz;
  uint32_t expected_matrix_scan_hz;
  uint32_t skipped_main_loop_count;
  uint32_t missed_generation_count;
  uint32_t intentional_skip_count;
  uint32_t coalesced_generation_count;
  uint32_t overload_missed_generation_count;
  uint32_t scheduler_budget_exhausted_count;
  uint32_t matrix_catchup_scan_count;
  uint32_t matrix_processing_divider;
  uint32_t matrix_fast_overrun_count;
  uint32_t matrix_housekeeping_hz;
  uint32_t last_housekeeping_us;
  uint32_t max_housekeeping_us;
  uint16_t rpt_trigger_count;
} matrix_scan_diagnostics_t;

//--------------------------------------------------------------------+
// Key Matrix API
//--------------------------------------------------------------------+

/**
 * @brief Initialize the key matrix module
 *
 * @return None
 */
void matrix_init(void);

/**
 * @brief Restart the calibration process
 *
 * This function will block until the calibration process is complete.
 *
 * @param reset_bottom_out_threshold Whether to reset the saved bottom-out
 * threshold as well
 *
 * @return None
 */
void matrix_recalibrate(bool reset_bottom_out_threshold);

/**
 * @brief Update the key matrix to reflect the current state of the keys
 *
 * Legacy/manual scan entry point. Do not call this from the high-rate
 * scheduler; use `matrix_task()` or `matrix_scan_fast()` instead.
 *
 * @return None
 */
void matrix_scan(void);

/**
 * @brief Run the latency-sensitive matrix processing path
 *
 * This only performs the per-key filter, distance, and actuation state update
 * needed for fast analog/Rapid Trigger behavior.
 *
 * @return None
 */
void matrix_scan_fast(void);

/**
 * @brief Run slower matrix housekeeping outside the fast path
 *
 * This handles deferred work such as continuous calibration, RGB keypress
 * notifications, and bottom-out threshold persistence at a lower cadence.
 *
 * @return None
 */
void matrix_scan_housekeeping(void);

/**
 * @brief Run one generation-gated matrix task from the main loop
 *
 * This only calls `matrix_scan_fast()` when a new full analog scan has
 * completed and the configured divider selects that generation.
 *
 * @return None
 */
void matrix_task(void);

/**
 * @brief Return the latest processed pressed snapshot for a key
 *
 * @param key Key index
 *
 * @return `true` if the key is pressed in the latest completed matrix scan
 */
bool matrix_snapshot_key_pressed(uint8_t key);

/**
 * @brief Disable Rapid Trigger of a key
 *
 * This function is used by the advanced keys that override the Rapid Trigger
 * settings e.g. Dynamic Keystroke.
 *
 * @param key Key index
 * @param disable Whether to disable Rapid Trigger
 *
 * @return None
 */
void matrix_disable_rapid_trigger(uint8_t key, bool disable);

/**
 * @brief Get the duration in milliseconds since the keyboard was last active
 *
 * @return Idle time in milliseconds, or 0 if any key is currently pressed
 */
uint32_t matrix_get_idle_time(void);

/**
 * @brief Refresh matrix diagnostics derived from the raw scan backend
 *
 * Call this before reading diagnostics when a current raw-scan snapshot is
 * required. Keeping refresh separate makes the getter side-effect free.
 *
 * @return None
 */
void matrix_refresh_scan_diagnostics(void);

/**
 * @brief Get the current cycle-based matrix diagnostics snapshot
 *
 * These counters are intended for tuning high-rate analog scan pipelines
 * without changing the public event timestamp semantics.
 *
 * @return Pointer to the current diagnostics snapshot
 */
const matrix_scan_diagnostics_t *matrix_get_scan_diagnostics(void);

/**
 * @brief Reset accumulated matrix scan diagnostics counters
 *
 * This clears the scan counter, extrema, and last per-mode summary so a host
 * can start a fresh measurement interval without recalibrating the matrix.
 *
 * @return None
 */
void matrix_reset_scan_diagnostics(void);

/**
 * @brief Get the current runtime Kalman filter configuration
 *
 * @return Pointer to the current Kalman filter configuration
 */
const kalman_config_t *matrix_get_kalman_config(void);

/**
 * @brief Set the runtime Kalman filter configuration and persist it
 *
 * Invalid values are rejected without modifying the runtime state or flash.
 *
 * @param config New Kalman filter configuration
 *
 * @return `true` if the configuration was accepted and persisted
 */
bool matrix_set_kalman_config(const kalman_config_t *config);
