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

#if !defined(MATRIX_EMA_ALPHA_EXPONENT)
// Exponent of the alpha parameter of the exponential moving average (EMA)
// filter used to smooth the ADC values. Higher values will result in smoother
// but slower changes in the filtered ADC values. The alpha parameter is used in
// the formula: y_n = alpha * x_n + (1 - alpha) * y_{n-1}
#define MATRIX_EMA_ALPHA_EXPONENT 4
#endif

#if !defined(MATRIX_EMA_TRACK_ALPHA_EXPONENT)
// Intermediate EMA used for smaller movements, especially while hovering near
// the rest position or actuation threshold.
#define MATRIX_EMA_TRACK_ALPHA_EXPONENT 3
#endif

#if !defined(MATRIX_EMA_FAST_ALPHA_EXPONENT)
// Faster EMA used while a key is actively moving or the sampled ADC delta is
// large enough that smoothing would noticeably hurt responsiveness.
#define MATRIX_EMA_FAST_ALPHA_EXPONENT 2
#endif

#if !defined(MATRIX_EMA_BURST_ALPHA_EXPONENT)
// Fastest EMA used for large step changes where filter lag would dominate.
#define MATRIX_EMA_BURST_ALPHA_EXPONENT 1
#endif

#if !defined(MATRIX_EMA_TRACK_DELTA)
// Minimum raw-vs-filtered ADC delta required to leave the idle EMA path.
#define MATRIX_EMA_TRACK_DELTA 6
#endif

#if !defined(MATRIX_EMA_FAST_DELTA)
// Minimum ADC delta required to switch to the faster EMA path.
#define MATRIX_EMA_FAST_DELTA 16
#endif

#if !defined(MATRIX_EMA_BURST_DELTA)
// Minimum ADC delta required to switch to the fastest EMA path.
#define MATRIX_EMA_BURST_DELTA 48
#endif

#if !defined(MATRIX_EMA_TRACK_VELOCITY)
// Minimum per-scan raw ADC movement required to leave the idle EMA path.
#define MATRIX_EMA_TRACK_VELOCITY 4
#endif

#if !defined(MATRIX_EMA_FAST_VELOCITY)
// Minimum per-scan raw ADC movement required to switch to the fast EMA path.
#define MATRIX_EMA_FAST_VELOCITY 12
#endif

#if !defined(MATRIX_EMA_BURST_VELOCITY)
// Minimum per-scan raw ADC movement required to switch to the burst EMA path.
#define MATRIX_EMA_BURST_VELOCITY 32
#endif

#if !defined(MATRIX_EMA_REST_WINDOW)
// Distance window near rest where the filter should stay more responsive.
#define MATRIX_EMA_REST_WINDOW 8
#endif

#if !defined(MATRIX_EMA_ACTUATION_WINDOW)
// Distance window near the actuation threshold where responsiveness matters.
#define MATRIX_EMA_ACTUATION_WINDOW 16
#endif

#if !defined(MATRIX_EMA_MODE_DECAY_SCANS)
// Number of consecutive calmer samples required before decaying one filter
// stage.
#define MATRIX_EMA_MODE_DECAY_SCANS 2
#endif

_Static_assert(MATRIX_EMA_MODE_DECAY_SCANS <= UINT8_MAX,
               "MATRIX_EMA_MODE_DECAY_SCANS must fit in key_state_t.filter_decay");

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
#define MATRIX_DETAILED_SCAN_DIAGNOSTICS 1
#endif

#if !defined(MATRIX_IDLE_EMA_FAST_PATH)
// Bypass filter-mode resolution for keys that are fully idle at rest.
#define MATRIX_IDLE_EMA_FAST_PATH 0
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

//--------------------------------------------------------------------+
// Key Matrix
//--------------------------------------------------------------------+

typedef enum {
  KEY_DIR_INACTIVE = 0,
  KEY_DIR_DOWN,
  KEY_DIR_UP,
} key_dir_t;

typedef enum {
  MATRIX_FILTER_MODE_IDLE = 0,
  MATRIX_FILTER_MODE_TRACK,
  MATRIX_FILTER_MODE_FAST,
  MATRIX_FILTER_MODE_BURST,
  MATRIX_FILTER_MODE_COUNT,
} matrix_filter_mode_t;

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
  // Current dynamic EMA stage (matrix_filter_mode_t)
  uint8_t filter_mode;
  // Consecutive calmer samples seen while decaying the filter stage
  uint8_t filter_decay;

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
  uint16_t last_mode_counts[MATRIX_FILTER_MODE_COUNT];
  uint32_t raw_scan_hz;
  uint32_t last_raw_scan_us;
  uint32_t max_raw_scan_us;
  uint32_t full_scan_generation;
  uint32_t matrix_scan_hz;
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
 * @brief Get cycle-based diagnostics for the most recent matrix scan
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
