#pragma once

#include "hardware/analog_api.h"

#if ADC_NUM_MUX_INPUTS > 0
extern const uint16_t analog_mux_input_matrix[][ADC_NUM_MUX_INPUTS];
#endif

#if ADC_NUM_RAW_INPUTS > 0
extern const uint16_t analog_raw_input_vector[ADC_NUM_RAW_INPUTS];
#endif

void analog_scan_reset(void);
void analog_scan_store_samples(const volatile uint16_t *samples,
                               uint8_t mux_channel);
uint16_t analog_scan_read_key(uint8_t key);

#if ADC_NUM_RAW_INPUTS > 0
uint16_t analog_scan_read_raw(uint8_t index);
#endif
