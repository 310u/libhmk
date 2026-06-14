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

void analog_scan_reset(void) {
  memset((void *)analog_key_values, 0, sizeof(analog_key_values));
  memset((void *)analog_key_versions, 0, sizeof(analog_key_versions));
  memset(analog_key_version_reference_values, 0,
         sizeof(analog_key_version_reference_values));
  analog_full_scan_generation = 0u;
#if ADC_NUM_RAW_INPUTS > 0
  memset((void *)analog_raw_values, 0, sizeof(analog_raw_values));
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
