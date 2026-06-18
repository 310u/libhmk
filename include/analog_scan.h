#pragma once

#include "hardware/analog_api.h"

#if ADC_NUM_MUX_INPUTS > 0
extern const uint16_t analog_mux_input_matrix[][ADC_NUM_MUX_INPUTS];
#endif

#if ADC_NUM_RAW_INPUTS > 0
extern const uint16_t analog_raw_input_vector[ADC_NUM_RAW_INPUTS];
#endif

#if !defined(ANALOG_SCAN_KEY_VERSION_DELTA)
// Increment key sample versions when a key moves by at least this ADC delta.
#define ANALOG_SCAN_KEY_VERSION_DELTA 0
#endif

#if !defined(HMK_DIAG_CHANNEL_IDENTITY)
#define HMK_DIAG_CHANNEL_IDENTITY 0
#endif

#if HMK_DIAG_CHANNEL_IDENTITY && ADC_NUM_MUX_INPUTS > 0
#define ANALOG_DIAG_MAX_MUX_STEPS 8u
#define ANALOG_DIAG_MAX_ADC_LANES 8u

_Static_assert((1u << ADC_NUM_MUX_SELECT_PINS) <= ANALOG_DIAG_MAX_MUX_STEPS,
               "Diagnostic raw-by-step view currently supports up to 8 mux steps");
_Static_assert(ADC_NUM_MUX_INPUTS <= ANALOG_DIAG_MAX_ADC_LANES,
               "Diagnostic raw-by-step view currently supports up to 8 ADC lanes");
#endif

typedef enum {
  ANALOG_CHANNEL_IDENTITY_FAILURE_NONE = 0,
  ANALOG_CHANNEL_IDENTITY_FAILURE_INVALID_KEY,
  ANALOG_CHANNEL_IDENTITY_FAILURE_WEAK_SIGNAL,
  ANALOG_CHANNEL_IDENTITY_FAILURE_WRONG_KEY,
  ANALOG_CHANNEL_IDENTITY_FAILURE_WRONG_STEP,
  ANALOG_CHANNEL_IDENTITY_FAILURE_WRONG_LANE,
  ANALOG_CHANNEL_IDENTITY_FAILURE_ONE_STEP_OFFSET,
  ANALOG_CHANNEL_IDENTITY_FAILURE_EXCESSIVE_CROSSTALK,
  ANALOG_CHANNEL_IDENTITY_FAILURE_AMBIGUOUS_MAX,
} analog_channel_identity_failure_t;

typedef struct {
  uint8_t expected_key;
  uint8_t expected_step;
  uint8_t expected_lane;
  uint8_t observed_max_step;
  uint8_t observed_max_lane;
  uint8_t observed_logical_key;
  uint8_t observed_second_step;
  uint8_t observed_second_lane;
  uint16_t observed_max_delta;
  uint16_t observed_second_delta;
  uint16_t min_delta;
  uint8_t max_secondary_ratio_percent;
  uint8_t failure_reason;
  bool pass;
} analog_channel_identity_result_t;

// Latest full-scan key samples staged for the matrix fast path.
extern volatile uint16_t analog_key_values[NUM_KEYS];
extern volatile uint8_t analog_key_versions[NUM_KEYS];

void analog_scan_reset(void);
void analog_scan_store_samples(const volatile uint16_t *samples,
                               uint8_t mux_channel);
void analog_scan_record_full_scan_generation(void);
uint32_t analog_scan_get_full_scan_generation(void);
bool analog_scan_consume_full_scan_generation(uint32_t *last_seen_generation,
                                              uint32_t *missed_count);

__attribute__((always_inline)) static inline uint16_t
analog_scan_peek_key(uint8_t key) {
  return key < NUM_KEYS ? analog_key_values[key] : 0u;
}

__attribute__((always_inline)) static inline uint8_t
analog_scan_peek_key_version(uint8_t key) {
  return key < NUM_KEYS ? analog_key_versions[key] : 0u;
}

uint16_t analog_scan_read_key(uint8_t key);

#if ADC_NUM_RAW_INPUTS > 0
uint16_t analog_scan_read_raw(uint8_t index);
#endif

bool analog_diag_channel_identity_enabled(void);
uint8_t analog_diag_mux_step_count(void);
uint8_t analog_diag_adc_lane_count(void);
void analog_diag_capture_baseline(void);
bool analog_diag_find_key_step_lane(uint8_t key, uint8_t *step, uint8_t *lane);
uint16_t analog_diag_read_raw_by_step(uint8_t step, uint8_t lane);
uint16_t analog_diag_read_baseline_by_step(uint8_t step, uint8_t lane);
uint16_t analog_diag_read_delta_by_step(uint8_t step, uint8_t lane);
bool analog_diag_run_channel_identity_test(
    uint8_t expected_key, uint16_t min_delta,
    uint8_t max_secondary_ratio_percent,
    analog_channel_identity_result_t *result);
