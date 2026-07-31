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
#include "eeconfig.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// Commands
//--------------------------------------------------------------------+

typedef enum {
  COMMAND_FIRMWARE_VERSION = 0,
  COMMAND_REBOOT,
  COMMAND_BOOTLOADER,
  COMMAND_FACTORY_RESET,
  COMMAND_RECALIBRATE,
  COMMAND_ANALOG_INFO,
  COMMAND_GET_CALIBRATION,
  COMMAND_SET_CALIBRATION,
  COMMAND_GET_PROFILE,
  COMMAND_GET_OPTIONS,
  COMMAND_SET_OPTIONS,
  COMMAND_RESET_PROFILE,
  COMMAND_DUPLICATE_PROFILE,
  COMMAND_GET_METADATA,
  COMMAND_GET_SERIAL,
  COMMAND_SAVE_CALIBRATION_THRESHOLD,
  COMMAND_ANALOG_INFO_RAW,

  COMMAND_GET_KEYMAP = 128,
  COMMAND_SET_KEYMAP,
  COMMAND_GET_ACTUATION_MAP,
  COMMAND_SET_ACTUATION_MAP,
  COMMAND_GET_ADVANCED_KEYS,
  COMMAND_SET_ADVANCED_KEYS,
  COMMAND_GET_TICK_RATE,
  COMMAND_SET_TICK_RATE,
  COMMAND_GET_GAMEPAD_BUTTONS,
  COMMAND_SET_GAMEPAD_BUTTONS,
  COMMAND_GET_GAMEPAD_OPTIONS,
  COMMAND_SET_GAMEPAD_OPTIONS,
  COMMAND_GET_MACROS,
  COMMAND_SET_MACROS,
  COMMAND_GET_RGB_CONFIG,
  COMMAND_SET_RGB_CONFIG,
  
  COMMAND_GET_JOYSTICK_STATE,
  COMMAND_GET_JOYSTICK_CONFIG,
  COMMAND_SET_JOYSTICK_CONFIG,
  COMMAND_SET_HOST_TIME,
  COMMAND_GET_TRACKBALL_STATE,
  COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS,
  COMMAND_RESET_MATRIX_SCAN_DIAGNOSTICS,
  COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS,
  COMMAND_RESET_ANALOG_SCAN_DIAGNOSTICS,
  COMMAND_GET_ANALOG_RAW_CHANNELS,
  COMMAND_GET_ANALOG_DEBUG_FRAMES,
  COMMAND_GET_ANALOG_SCAN_CONFIG,
  COMMAND_SET_ANALOG_SCAN_CONFIG,
  COMMAND_CAPTURE_ANALOG_DIAG_BASELINE,
  COMMAND_RUN_ANALOG_CHANNEL_IDENTITY_TEST,
  COMMAND_GET_ANALOG_RAW_BY_STEP,
  COMMAND_GET_KALMAN_CONFIG,
  COMMAND_SET_KALMAN_CONFIG,

  COMMAND_UNKNOWN = 255,
} command_id_t;

//---------------------------------------------------------------------+
// Input Report Structures
//---------------------------------------------------------------------+

typedef struct __attribute__((packed)) {
  uint8_t offset;
} command_in_analog_info_t;

typedef eeconfig_calibration_t command_in_calibration_t;

typedef eeconfig_options_t command_in_options_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
} command_in_reset_profile_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint8_t src_profile;
} command_in_duplicate_profile_t;

typedef struct __attribute__((packed)) {
  uint32_t offset;
} command_in_metadata_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint8_t layer;
  uint8_t offset;
  uint8_t len;
  uint8_t keymap[59];
} command_in_keymap_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint8_t offset;
  uint8_t len;
  actuation_t actuation_map[15];
} command_in_actuation_map_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint8_t offset;
  uint8_t len;
  advanced_key_t advanced_keys[4]; // 4 * 13 bytes = 52 bytes, fits in 64-byte RAW_HID buffer
} command_in_advanced_keys_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint8_t tick_rate;
} command_in_tick_rate_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint8_t offset;
  uint8_t len;
  uint8_t gamepad_buttons[60];
} command_in_gamepad_buttons_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  gamepad_options_t gamepad_options;
} command_in_gamepad_options_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint8_t offset;
  uint8_t len;
  macro_t macros[1];
} command_in_macros_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint8_t offset;
  uint8_t len;
  uint8_t data[59];
} command_in_rgb_config_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  joystick_config_t joystick_config;
} command_in_joystick_config_t;

typedef struct __attribute__((packed)) {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} command_in_host_time_t;

typedef struct __attribute__((packed)) {
  uint16_t mux_sample_delay_us;
} command_analog_scan_config_t;

typedef kalman_config_t command_in_kalman_config_t;

typedef struct __attribute__((packed)) {
  uint8_t expected_key;
  uint16_t min_delta;
  uint8_t max_secondary_ratio_percent;
} command_in_analog_channel_identity_test_t;

typedef struct __attribute__((packed)) {
  uint8_t step;
} command_in_analog_raw_by_step_t;

// Command input buffer type
typedef struct __attribute__((packed)) {
  uint8_t command_id;
  union __attribute__((packed)) {
    command_in_analog_info_t analog_info;
    command_in_calibration_t calibration;
    command_in_options_t options;
    command_in_reset_profile_t reset_profile;
    command_in_duplicate_profile_t duplicate_profile;
    command_in_metadata_t metadata;

    command_in_keymap_t keymap;
    command_in_actuation_map_t actuation_map;
    command_in_advanced_keys_t advanced_keys;
    command_in_tick_rate_t tick_rate;
    command_in_gamepad_buttons_t gamepad_buttons;
    command_in_gamepad_options_t gamepad_options;
    command_in_macros_t macros;
    command_in_rgb_config_t rgb_config;
    command_in_joystick_config_t joystick_config;
    command_in_host_time_t host_time;
    command_analog_scan_config_t analog_scan_config;
    command_in_kalman_config_t kalman_config;
    command_in_analog_channel_identity_test_t analog_channel_identity_test;
    command_in_analog_raw_by_step_t analog_raw_by_step;
  };
} command_in_buffer_t;

_Static_assert(sizeof(command_in_buffer_t) <= RAW_HID_EP_SIZE,
               "Invalid command input buffer size");

//---------------------------------------------------------------------+
// Output Report Structures
//---------------------------------------------------------------------+

typedef struct __attribute__((packed)) {
  uint16_t adc_value;
  uint8_t distance;
} command_out_analog_info_t;

typedef struct __attribute__((packed)) {
  uint32_t len;
  uint8_t metadata[59];
} command_out_metadata_t;

typedef struct __attribute__((packed)) {
  uint8_t profile;
  uint16_t raw_x;
  uint16_t raw_y;
  int8_t out_x;
  int8_t out_y;
  bool sw;
  int8_t calibrated_x;
  int8_t calibrated_y;
  int8_t corrected_x;
  int8_t corrected_y;
} command_out_joystick_state_t;

typedef struct __attribute__((packed)) {
  uint8_t data[sizeof(joystick_config_t)];
} command_out_joystick_config_t;

typedef struct __attribute__((packed)) {
  bool enabled;
  uint16_t current_cpi;
  int16_t last_dx;
  int16_t last_dy;
} command_out_trackball_state_t;

typedef struct __attribute__((packed)) {
  uint32_t matrix_scan_count;
  uint32_t matrix_scan_hz;
  uint32_t last_matrix_scan_us;
  uint32_t max_matrix_scan_us;
  uint32_t raw_scan_hz;
  uint32_t last_raw_scan_us;
  uint32_t max_raw_scan_us;
  uint32_t full_scan_generation;
  uint32_t missed_generation_count;
  uint32_t matrix_processing_divider;
  uint32_t intentional_skip_count;
  uint32_t coalesced_generation_count;
  uint32_t overload_missed_generation_count;
  uint32_t scheduler_budget_exhausted_count;
  uint32_t matrix_catchup_scan_count;
  uint16_t expected_matrix_scan_hz;
  uint8_t matrix_fast_overrun_count;
} command_out_matrix_scan_diagnostics_t;

typedef struct __attribute__((packed)) {
  uint16_t mux_sample_delay_us;
  uint16_t mux_step_count;
  uint32_t scan_count;
  uint32_t last_scan_cycles;
  uint32_t max_scan_cycles;
  uint32_t last_scan_us;
  uint32_t max_scan_us;
  uint32_t estimated_scan_hz;
  uint32_t bad_channel_id_count;
  uint32_t dma_overrun_count;
  uint32_t overrun_count;
  uint32_t spi_error_count;
  uint32_t missed_scan_count;
} command_out_analog_scan_diagnostics_t;

typedef struct __attribute__((packed)) {
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
} command_out_analog_channel_identity_result_t;

enum {
  COMMAND_ANALOG_DIAG_MAX_ADC_LANES = 8u,
};

typedef struct __attribute__((packed)) {
  uint8_t step;
  uint8_t lane_count;
  uint16_t raw_by_lane[COMMAND_ANALOG_DIAG_MAX_ADC_LANES];
  uint16_t baseline_by_lane[COMMAND_ANALOG_DIAG_MAX_ADC_LANES];
  uint16_t delta_by_lane[COMMAND_ANALOG_DIAG_MAX_ADC_LANES];
} command_out_analog_raw_by_step_t;

typedef kalman_config_t command_out_kalman_config_t;

// Command output buffer type
typedef struct __attribute__((packed)) {
  uint8_t command_id;
  union __attribute__((packed)) {
    // For `COMMAND_FIRMWARE_VERSION`
    uint16_t firmware_version;
    // For `COMMAND_ANALOG_INFO`
    command_out_analog_info_t analog_info[21];
    // For `COMMAND_GET_CALIBRATION`
    eeconfig_calibration_t calibration;
    // For `COMMAND_GET_PROFILE`
    uint8_t current_profile;
    // For `COMMAND_GET_OPTIONS`
    eeconfig_options_t options;
    // For `COMMAND_GET_METADATA`
    command_out_metadata_t metadata;
    // For `COMMAND_GET_SERIAL`
    char serial[32];

    // For `COMMAND_GET_KEYMAP`
    uint8_t keymap[63];
    // For `COMMAND_GET_ACTUATION_MAP`
    actuation_t actuation_map[15];
    // For `COMMAND_GET_ADVANCED_KEYS`
    advanced_key_t advanced_keys[4]; // 4 * 13 bytes = 52 bytes, fits in 64-byte RAW_HID buffer
    // For `COMMAND_GET_TICK_RATE`
    uint8_t tick_rate;
    // For `COMMAND_GET_GAMEPAD_BUTTONS`
    uint8_t gamepad_buttons[63];
    // For `COMMAND_GET_GAMEPAD_OPTIONS`
    gamepad_options_t gamepad_options;
    // For `COMMAND_GET_MACROS`
    macro_t macros[1];
    // For `COMMAND_GET_RGB_CONFIG`
    uint8_t rgb_config_data[63];
    // For `COMMAND_GET_JOYSTICK_STATE`
    command_out_joystick_state_t joystick_state;
    // For `COMMAND_GET_JOYSTICK_CONFIG`
    command_out_joystick_config_t joystick_config;
    // For `COMMAND_GET_TRACKBALL_STATE`
    command_out_trackball_state_t trackball_state;
    // For `COMMAND_GET_ANALOG_SCAN_CONFIG`
    command_analog_scan_config_t analog_scan_config;
    // For `COMMAND_GET_KALMAN_CONFIG`
    command_out_kalman_config_t kalman_config;
    // For `COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS`
    command_out_matrix_scan_diagnostics_t matrix_scan_diagnostics;
    // For `COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS`
    command_out_analog_scan_diagnostics_t analog_scan_diagnostics;
    // For `COMMAND_RUN_ANALOG_CHANNEL_IDENTITY_TEST`
    command_out_analog_channel_identity_result_t analog_channel_identity_result;
    // For `COMMAND_GET_ANALOG_RAW_BY_STEP`
    command_out_analog_raw_by_step_t analog_raw_by_step;
  };
} command_out_buffer_t;

_Static_assert(sizeof(command_out_buffer_t) <= RAW_HID_EP_SIZE,
               "Invalid command output buffer size");

//---------------------------------------------------------------------+
// Command API
//---------------------------------------------------------------------+

/**
 * @brief Initialize the command module
 *
 * @return None
 */
void command_init(void);

/**
 * @brief Queue a raw HID command for later processing
 *
 * Only one raw HID command can be queued at a time. Additional commands are
 * dropped while a request or response is already pending.
 *
 * @param buf Command buffer
 * @param len Buffer length in bytes
 *
 * @return `true` if the command was queued
 */
bool command_enqueue(const uint8_t *buf, uint16_t len);

/**
 * @brief Process a command buffer immediately
 *
 * This is primarily used by tests and non-USB call sites. Responses are still
 * deferred until `command_task()`.
 *
 * @param buf Command buffer
 *
 * @return None
 */
void command_process(const uint8_t *buf);

/**
 * @brief Background task for processing queued commands and deferred responses
 */
void command_task(void);
