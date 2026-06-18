#include "analog_scan.h"

#if ADC_NUM_MUX_INPUTS > 0
// Matrix containing the key index for each multiplexer input channel and each
// ADC channel. If the value is at least `NUM_KEYS`, the corresponding key is
// not connected.
const uint16_t analog_mux_input_matrix[][ADC_NUM_MUX_INPUTS] =
    ADC_MUX_INPUT_MATRIX;

_Static_assert(M_ARRAY_SIZE(analog_mux_input_matrix) ==
                   (1 << ADC_NUM_MUX_SELECT_PINS),
               "Invalid number of multiplexer select pins");
#endif

#if ADC_NUM_RAW_INPUTS > 0
// Vector containing the key index for each raw input channel. If the value is
// at least `NUM_KEYS`, the corresponding key is not connected.
const uint16_t analog_raw_input_vector[ADC_NUM_RAW_INPUTS] =
    ADC_RAW_INPUT_VECTOR;

_Static_assert(M_ARRAY_SIZE(analog_raw_input_vector) == ADC_NUM_RAW_INPUTS,
               "Invalid number of ADC raw input mappings");
#endif

volatile uint16_t analog_key_values[NUM_KEYS];
volatile uint8_t analog_key_versions[NUM_KEYS];
static volatile uint32_t analog_full_scan_generation;
static uint16_t analog_key_version_reference_values[NUM_KEYS];

#if ADC_NUM_RAW_INPUTS > 0
static volatile uint16_t analog_raw_values[ADC_NUM_RAW_INPUTS];
#endif

#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
enum {
  ANALOG_DIAG_DEFAULT_MIN_DELTA = 64u,
  ANALOG_DIAG_DEFAULT_MAX_SECONDARY_RATIO_PERCENT = 20u,
};

static volatile uint16_t
    analog_diag_raw_by_step[1u << ADC_NUM_MUX_SELECT_PINS][ADC_NUM_MUX_INPUTS];
static uint16_t
    analog_diag_baseline_by_step[1u << ADC_NUM_MUX_SELECT_PINS][ADC_NUM_MUX_INPUTS];
static uint16_t
    analog_diag_delta_by_step[1u << ADC_NUM_MUX_SELECT_PINS][ADC_NUM_MUX_INPUTS];

static uint8_t analog_diag_step_count(void) {
  return (uint8_t)(1u << ADC_NUM_MUX_SELECT_PINS);
}

static uint8_t analog_diag_lane_count(void) { return ADC_NUM_MUX_INPUTS; }

static uint8_t analog_diag_logical_key_for_step_lane(uint8_t step, uint8_t lane) {
  if (step >= analog_diag_step_count() || lane >= analog_diag_lane_count())
    return UINT8_MAX;

  const uint16_t key = analog_mux_input_matrix[step][lane];
  return key == 0u || key > NUM_KEYS ? UINT8_MAX : (uint8_t)(key - 1u);
}

static bool analog_diag_steps_are_adjacent(uint8_t lhs, uint8_t rhs) {
  if (lhs == UINT8_MAX || rhs == UINT8_MAX)
    return false;

  return lhs + 1u == rhs || rhs + 1u == lhs;
}
#endif

void analog_scan_reset(void) {
  memset((void *)analog_key_values, 0, sizeof(analog_key_values));
  memset((void *)analog_key_versions, 0, sizeof(analog_key_versions));
  memset(analog_key_version_reference_values, 0,
         sizeof(analog_key_version_reference_values));
  analog_full_scan_generation = 0u;
#if ADC_NUM_RAW_INPUTS > 0
  memset((void *)analog_raw_values, 0, sizeof(analog_raw_values));
#endif
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  memset((void *)analog_diag_raw_by_step, 0, sizeof(analog_diag_raw_by_step));
  memset(analog_diag_baseline_by_step, 0, sizeof(analog_diag_baseline_by_step));
  memset(analog_diag_delta_by_step, 0, sizeof(analog_diag_delta_by_step));
#endif
}

#if ANALOG_SCAN_KEY_VERSION_DELTA > 0
__attribute__((always_inline)) static inline void
analog_scan_store_key_sample(uint8_t key, uint16_t sample) {
  const uint16_t reference = analog_key_version_reference_values[key];
  const uint16_t delta =
      sample > reference ? (uint16_t)(sample - reference)
                         : (uint16_t)(reference - sample);
  if (delta >= ANALOG_SCAN_KEY_VERSION_DELTA) {
    analog_key_version_reference_values[key] = sample;
    analog_key_versions[key]++;
  }
  analog_key_values[key] = sample;
}
#endif

void analog_scan_store_samples(const volatile uint16_t *samples,
                               uint8_t mux_channel) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  if (mux_channel < analog_diag_step_count()) {
    for (uint32_t i = 0; i < ADC_NUM_MUX_INPUTS; i++)
      analog_diag_raw_by_step[mux_channel][i] = samples[i];
  }
#endif

#if ADC_NUM_MUX_INPUTS > 0
  for (uint32_t i = 0; i < ADC_NUM_MUX_INPUTS; i++) {
    const uint16_t key = analog_mux_input_matrix[mux_channel][i];
    if (key != 0 && key <= NUM_KEYS) {
#if ANALOG_SCAN_KEY_VERSION_DELTA > 0
      analog_scan_store_key_sample((uint8_t)(key - 1u), samples[i]);
#else
      analog_key_values[key - 1u] = samples[i];
#endif
    }
  }
#else
  (void)mux_channel;
#endif

#if ADC_NUM_RAW_INPUTS > 0
  for (uint32_t i = 0; i < ADC_NUM_RAW_INPUTS; i++) {
    const uint16_t sample = samples[ADC_NUM_MUX_INPUTS + i];
    const uint16_t key = analog_raw_input_vector[i];

    analog_raw_values[i] = sample;
    if (key != 0 && key <= NUM_KEYS) {
#if ANALOG_SCAN_KEY_VERSION_DELTA > 0
      analog_scan_store_key_sample((uint8_t)(key - 1u), sample);
#else
      analog_key_values[key - 1u] = sample;
#endif
    }
  }
#endif
}

uint16_t analog_scan_read_key(uint8_t key) {
  return analog_scan_peek_key(key);
}

void analog_scan_record_full_scan_generation(void) {
  analog_full_scan_generation++;
}

uint32_t analog_scan_get_full_scan_generation(void) {
  return analog_full_scan_generation;
}

bool analog_scan_consume_full_scan_generation(uint32_t *last_seen_generation,
                                              uint32_t *missed_count) {
  if (last_seen_generation == NULL)
    return false;

  const uint32_t current_generation = analog_scan_get_full_scan_generation();
  if (current_generation == *last_seen_generation)
    return false;

  if (missed_count != NULL &&
      current_generation - *last_seen_generation > 1u) {
    *missed_count += (current_generation - *last_seen_generation) - 1u;
  }

  *last_seen_generation = current_generation;
  return true;
}

#if ADC_NUM_RAW_INPUTS > 0
uint16_t analog_scan_read_raw(uint8_t index) {
  return index < ADC_NUM_RAW_INPUTS ? analog_raw_values[index] : 0;
}
#endif

bool analog_diag_channel_identity_enabled(void) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  return true;
#else
  return false;
#endif
}

uint8_t analog_diag_mux_step_count(void) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  return analog_diag_step_count();
#else
  return 0u;
#endif
}

uint8_t analog_diag_adc_lane_count(void) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  return analog_diag_lane_count();
#else
  return 0u;
#endif
}

void analog_diag_capture_baseline(void) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  for (uint8_t step = 0; step < analog_diag_step_count(); step++) {
    for (uint8_t lane = 0; lane < analog_diag_lane_count(); lane++)
      analog_diag_baseline_by_step[step][lane] = analog_diag_raw_by_step[step][lane];
  }
  memset(analog_diag_delta_by_step, 0, sizeof(analog_diag_delta_by_step));
#endif
}

bool analog_diag_find_key_step_lane(uint8_t key, uint8_t *step, uint8_t *lane) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  for (uint8_t step_index = 0; step_index < analog_diag_step_count();
       step_index++) {
    for (uint8_t lane_index = 0; lane_index < analog_diag_lane_count();
         lane_index++) {
      if (analog_mux_input_matrix[step_index][lane_index] ==
          (uint16_t)(key + 1u)) {
        if (step != NULL)
          *step = step_index;
        if (lane != NULL)
          *lane = lane_index;
        return true;
      }
    }
  }

  return false;
#else
  (void)key;
  (void)step;
  (void)lane;
  return false;
#endif
}

uint16_t analog_diag_read_raw_by_step(uint8_t step, uint8_t lane) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  if (step >= analog_diag_step_count() || lane >= analog_diag_lane_count())
    return 0u;

  return analog_diag_raw_by_step[step][lane];
#else
  (void)step;
  (void)lane;
  return 0u;
#endif
}

uint16_t analog_diag_read_baseline_by_step(uint8_t step, uint8_t lane) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  if (step >= analog_diag_step_count() || lane >= analog_diag_lane_count())
    return 0u;

  return analog_diag_baseline_by_step[step][lane];
#else
  (void)step;
  (void)lane;
  return 0u;
#endif
}

uint16_t analog_diag_read_delta_by_step(uint8_t step, uint8_t lane) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  if (step >= analog_diag_step_count() || lane >= analog_diag_lane_count())
    return 0u;

  return analog_diag_delta_by_step[step][lane];
#else
  (void)step;
  (void)lane;
  return 0u;
#endif
}

bool analog_diag_run_channel_identity_test(
    uint8_t expected_key, uint16_t min_delta,
    uint8_t max_secondary_ratio_percent,
    analog_channel_identity_result_t *result) {
#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
  if (result == NULL)
    return false;

  memset(result, 0, sizeof(*result));
  result->expected_key = expected_key;
  result->observed_max_step = UINT8_MAX;
  result->observed_max_lane = UINT8_MAX;
  result->observed_logical_key = UINT8_MAX;
  result->observed_second_step = UINT8_MAX;
  result->observed_second_lane = UINT8_MAX;
  result->min_delta =
      min_delta == 0u ? ANALOG_DIAG_DEFAULT_MIN_DELTA : min_delta;
  result->max_secondary_ratio_percent =
      max_secondary_ratio_percent == 0u
          ? ANALOG_DIAG_DEFAULT_MAX_SECONDARY_RATIO_PERCENT
          : max_secondary_ratio_percent;

  if (!analog_diag_find_key_step_lane(expected_key, &result->expected_step,
                                      &result->expected_lane)) {
    result->failure_reason = ANALOG_CHANNEL_IDENTITY_FAILURE_INVALID_KEY;
    return true;
  }

  for (uint8_t step = 0; step < analog_diag_step_count(); step++) {
    for (uint8_t lane = 0; lane < analog_diag_lane_count(); lane++) {
      const uint16_t raw = analog_diag_raw_by_step[step][lane];
      const uint16_t baseline = analog_diag_baseline_by_step[step][lane];
      const uint16_t delta = raw > baseline ? (uint16_t)(raw - baseline)
                                            : (uint16_t)(baseline - raw);
      analog_diag_delta_by_step[step][lane] = delta;

      if (delta > result->observed_max_delta) {
        result->observed_second_delta = result->observed_max_delta;
        result->observed_second_step = result->observed_max_step;
        result->observed_second_lane = result->observed_max_lane;
        result->observed_max_delta = delta;
        result->observed_max_step = step;
        result->observed_max_lane = lane;
        result->observed_logical_key =
            analog_diag_logical_key_for_step_lane(step, lane);
      } else if (delta > result->observed_second_delta) {
        result->observed_second_delta = delta;
        result->observed_second_step = step;
        result->observed_second_lane = lane;
      }
    }
  }

  if (result->observed_max_delta < result->min_delta) {
    result->failure_reason = ANALOG_CHANNEL_IDENTITY_FAILURE_WEAK_SIGNAL;
    return true;
  }

  if (result->observed_second_delta == result->observed_max_delta &&
      result->observed_second_delta != 0u &&
      (result->observed_second_step != result->observed_max_step ||
       result->observed_second_lane != result->observed_max_lane)) {
    result->failure_reason = ANALOG_CHANNEL_IDENTITY_FAILURE_AMBIGUOUS_MAX;
    return true;
  }

  if (result->observed_logical_key != expected_key) {
    if ((analog_diag_steps_are_adjacent(result->observed_max_step,
                                        result->expected_step) &&
         result->observed_max_lane == result->expected_lane) ||
        (analog_diag_steps_are_adjacent(result->observed_max_lane,
                                        result->expected_lane) &&
         result->observed_max_step == result->expected_step)) {
      result->failure_reason = ANALOG_CHANNEL_IDENTITY_FAILURE_ONE_STEP_OFFSET;
    } else {
      result->failure_reason = ANALOG_CHANNEL_IDENTITY_FAILURE_WRONG_KEY;
    }
    return true;
  }

  if (result->observed_max_step != result->expected_step) {
    result->failure_reason =
        analog_diag_steps_are_adjacent(result->observed_max_step,
                                       result->expected_step)
            ? ANALOG_CHANNEL_IDENTITY_FAILURE_ONE_STEP_OFFSET
            : ANALOG_CHANNEL_IDENTITY_FAILURE_WRONG_STEP;
    return true;
  }

  if (result->observed_max_lane != result->expected_lane) {
    result->failure_reason =
        analog_diag_steps_are_adjacent(result->observed_max_lane,
                                       result->expected_lane)
            ? ANALOG_CHANNEL_IDENTITY_FAILURE_ONE_STEP_OFFSET
            : ANALOG_CHANNEL_IDENTITY_FAILURE_WRONG_LANE;
    return true;
  }

  if (result->observed_second_delta != 0u &&
      (uint32_t)result->observed_second_delta * 100u >
          (uint32_t)result->observed_max_delta *
              result->max_secondary_ratio_percent) {
    result->failure_reason =
        ANALOG_CHANNEL_IDENTITY_FAILURE_EXCESSIVE_CROSSTALK;
    return true;
  }

  result->pass = true;
  return true;
#else
  (void)expected_key;
  (void)min_delta;
  (void)max_secondary_ratio_percent;
  (void)result;
  return false;
#endif
}
