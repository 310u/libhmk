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
// Analog Configuration
//--------------------------------------------------------------------+

#if !defined(ANALOG_BACKEND_MCU_ADC) && !defined(ANALOG_BACKEND_SPI_ADC)
#define ANALOG_BACKEND_MCU_ADC 1
#endif

#if defined(ANALOG_BACKEND_MCU_ADC) && defined(ANALOG_BACKEND_SPI_ADC)
#error "Analog backend must select exactly one implementation"
#endif

// Maximum sampled analog value.
#define ADC_MAX_VALUE ((1 << ADC_RESOLUTION) - 1)

#if !defined(ADC_NUM_MUX_INPUTS)
// Number of ADC inputs that are connected to the multiplexer
#define ADC_NUM_MUX_INPUTS 0
#endif

#if ADC_NUM_MUX_INPUTS > 0
#if !defined(ADC_MUX_INPUT_CHANNELS)
#error "ADC_MUX_INPUT_CHANNELS is not defined"
#endif

#if !defined(ADC_NUM_MUX_SELECT_PINS)
#error "ADC_NUM_MUX_SELECT_PINS is not defined"
#endif

#if !defined(ADC_MUX_SELECT_PORTS)
#error "ADC_MUX_SELECT_PORTS is not defined"
#endif

#if !defined(ADC_MUX_SELECT_PINS)
#error "ADC_MUX_SELECT_PINS is not defined"
#endif

#if !defined(ADC_MUX_INPUT_MATRIX)
#error "ADC_MUX_INPUT_MATRIX is not defined"
#endif

#if !defined(ADC_SAMPLE_DELAY)
// Delay in microseconds to allow the multiplexer outputs to settle
#define ADC_SAMPLE_DELAY 20
#endif

#define ANALOG_MUX_SAMPLE_DELAY_MIN_US 1u
#define ANALOG_MUX_SAMPLE_DELAY_MAX_US 50u

_Static_assert((F_CPU / 1000000) * ANALOG_MUX_SAMPLE_DELAY_MAX_US < 65536,
               "MUX sample delay exceeds maximum timer period");
#endif

#if !defined(ADC_NUM_RAW_INPUTS)
// Number of ADC inputs that are connected directly to the keys
#define ADC_NUM_RAW_INPUTS 0
#endif

#if ADC_NUM_RAW_INPUTS > 0
#if defined(ANALOG_BACKEND_MCU_ADC) && !defined(ADC_RAW_INPUT_CHANNELS)
#error "ADC_RAW_INPUT_CHANNELS is not defined"
#endif

#if !defined(ADC_RAW_INPUT_VECTOR)
#error "ADC_RAW_INPUT_VECTOR is not defined"
#endif
#endif

// Total number of sampled analog inputs presented to the generic matrix scan.
#define ANALOG_NUM_SAMPLED_INPUTS (ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS)

//--------------------------------------------------------------------+
// Digital GPIO Input Configuration
//--------------------------------------------------------------------+

#if !defined(DIGITAL_NUM_INPUTS)
// Number of GPIO-backed digital inputs exposed as keys.
#define DIGITAL_NUM_INPUTS 0
#endif

#if DIGITAL_NUM_INPUTS > 0
#if !defined(DIGITAL_INPUT_PORTS)
#error "DIGITAL_INPUT_PORTS is not defined"
#endif

#if !defined(DIGITAL_INPUT_PINS)
#error "DIGITAL_INPUT_PINS is not defined"
#endif

#if !defined(DIGITAL_INPUT_VECTOR)
#error "DIGITAL_INPUT_VECTOR is not defined"
#endif

#if defined(DIGITAL_INPUT_PULLUP) && defined(DIGITAL_INPUT_PULLDOWN)
#error "DIGITAL_INPUT_PULLUP and DIGITAL_INPUT_PULLDOWN are mutually exclusive"
#endif
#endif

#if !(0 < ANALOG_NUM_SAMPLED_INPUTS)
#error "Invalid number of analog inputs"
#endif

#if defined(ANALOG_BACKEND_MCU_ADC)
#if !defined(ADC_NUM_CHANNELS)
#error "ADC_NUM_CHANNELS is not defined"
#endif

#if !(ANALOG_NUM_SAMPLED_INPUTS <= ADC_NUM_CHANNELS)
#error "Invalid number of ADC inputs"
#endif
#endif

//--------------------------------------------------------------------+
// Analog API
//--------------------------------------------------------------------+

typedef struct {
  uint32_t scan_count;
  uint32_t last_scan_cycles;
  uint32_t max_scan_cycles;
  uint32_t last_scan_us;
  uint32_t max_scan_us;
  uint32_t estimated_scan_hz;
  uint32_t last_bus_completion_skew_cycles;
  uint32_t max_bus_completion_skew_cycles;
  uint32_t bad_channel_id_count;
  uint32_t dma_overrun_count;
  uint32_t overrun_count;
  uint32_t spi_error_count;
  uint32_t missed_scan_count;
  uint16_t mux_sample_delay_us;
  uint16_t reserved;
  uint8_t mux_step_count;
  uint8_t active_bus_count;
  uint8_t active_device_count;
  uint8_t reserved2;
} analog_scan_diagnostics_t;

/**
 * @brief Initialize the analog module
 *
 * This function should initialize the analog sampling backend and any other
 * peripherals needed to scan the keys. This may be the MCU ADC, or an external
 * converter such as an SPI ADC. In a non-blocking implementation, this
 * function can be used to start the conversion loop.
 *
 * @return None
 */
void analog_init(void);

/**
 * @brief Analog task
 *
 * This function will be called before reading the sampled values. In a
 * blocking implementation, this function can be used to start and wait for a
 * conversion to complete.
 *
 * @return None
 */
void analog_task(void);

/**
 * @brief Read the raw sampled value of the specified key
 *
 * @param key Key index
 *
 * @return Raw sampled value
 */
uint16_t analog_read(uint8_t key);

#if ADC_NUM_RAW_INPUTS > 0
/**
 * @brief Read the raw sampled value of the specified raw input index
 *
 * @param index Raw input index
 *
 * @return Raw sampled value
 */
uint16_t analog_read_raw(uint8_t index);
#endif

/**
 * @brief Get backend-specific analog scan diagnostics
 *
 * For mux-based scan backends, the timing fields report full-scan timing after
 * every mux select state has been sampled once.
 *
 * @return Pointer to the current diagnostics snapshot
 */
const analog_scan_diagnostics_t *analog_get_scan_diagnostics(void);

/**
 * @brief Reset accumulated analog scan diagnostics counters
 *
 * This clears the timing counters so a host can start a fresh measurement
 * interval.
 *
 * @return None
 */
void analog_reset_scan_diagnostics(void);

/**
 * @brief Read the active runtime multiplexer settle delay
 *
 * Backends without a mux-based scan pipeline should return `0`.
 *
 * @return Active settle delay in microseconds
 */
uint16_t analog_get_mux_sample_delay_us(void);

/**
 * @brief Update the runtime multiplexer settle delay
 *
 * Implementations should validate the value and apply it safely at a scan-step
 * boundary when the scan engine is already active.
 *
 * @param delay_us Requested delay in microseconds
 *
 * @return `true` if the new value was accepted
 */
bool analog_set_mux_sample_delay_us(uint16_t delay_us);

/**
 * @brief Return the number of backend-specific debug frames available
 *
 * Backends that do not expose frame-level transport diagnostics should return
 * `0`.
 *
 * @return Number of readable debug frames
 */
uint16_t analog_debug_frame_count(void);

/**
 * @brief Read one backend-specific debug frame
 *
 * For the ADS7953 backend, this returns the raw 16-bit word most recently read
 * from the converter transport stream. Backends that do not support this
 * should return `0`.
 *
 * @param index Flattened debug frame index
 *
 * @return Raw backend-specific frame data
 */
uint16_t analog_read_debug_frame(uint8_t index);
