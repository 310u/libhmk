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

static volatile uint16_t analog_key_values[NUM_KEYS];

#if ADC_NUM_RAW_INPUTS > 0
static volatile uint16_t analog_raw_values[ADC_NUM_RAW_INPUTS];
#endif

void analog_scan_reset(void) {
  memset((void *)analog_key_values, 0, sizeof(analog_key_values));
#if ADC_NUM_RAW_INPUTS > 0
  memset((void *)analog_raw_values, 0, sizeof(analog_raw_values));
#endif
}

void analog_scan_store_samples(const volatile uint16_t *samples,
                               uint8_t mux_channel) {
#if ADC_NUM_MUX_INPUTS > 0
  for (uint32_t i = 0; i < ADC_NUM_MUX_INPUTS; i++) {
    const uint16_t key = analog_mux_input_matrix[mux_channel][i];
    if (key != 0 && key <= NUM_KEYS) {
      analog_key_values[key - 1] = samples[i];
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
      analog_key_values[key - 1] = sample;
    }
  }
#endif
}

uint16_t analog_scan_read_key(uint8_t key) { return analog_key_values[key]; }

#if ADC_NUM_RAW_INPUTS > 0
uint16_t analog_scan_read_raw(uint8_t index) { return analog_raw_values[index]; }
#endif
