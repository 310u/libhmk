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
