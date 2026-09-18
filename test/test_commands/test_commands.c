#include <unity.h>

#include "analog_scan.h"
#include "commands.h"
#include "diagnostic_mode.h"
#include "hardware/analog_api.h"
#include "layout.h"
#include "matrix.h"
#include "rgb.h"
#include "trackball.h"
#include "tusb.h"
#include "usb_descriptors.h"

void profile_runtime_reload_current(void);

key_state_t key_matrix[NUM_KEYS];
eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;
bool is_sniper_active = false;

static bool raw_hid_ready;
static uint32_t raw_hid_report_count;
static uint8_t raw_hid_reports[4][RAW_HID_EP_SIZE];
static uint32_t wear_leveling_write_count;
static bool wear_leveling_write_result;
static uint32_t wear_leveling_last_addr;
static uint32_t wear_leveling_last_len;
static uint8_t wear_leveling_last_data[8];
static uint32_t layout_reset_count;
static uint32_t profile_reload_count;
static uint32_t recalibrate_count;
static uint32_t board_reset_count;
static uint32_t board_bootloader_count;
static uint32_t rgb_apply_count;
static uint32_t matrix_diag_reset_count;
static uint32_t analog_diag_reset_count;
static bool host_time_synced;
static uint8_t host_time_hours;
static uint8_t host_time_minutes;
static uint8_t host_time_seconds;
static matrix_scan_diagnostics_t mock_matrix_diag;
static analog_scan_diagnostics_t mock_analog_diag;
static uint16_t mock_analog_mux_sample_delay_us;
static bool analog_set_mux_sample_delay_result;
static uint32_t analog_set_mux_sample_delay_count;
static kalman_config_t mock_kalman_config;
static kalman_config_t mock_kalman_config_set;
static bool matrix_set_kalman_config_result;
static uint32_t matrix_set_kalman_config_count;
static bool mock_diagnostic_mode_active;
static bool mock_diag_channel_identity_enabled;

static distance_curve_config_t mock_distance_curve_config;
static distance_curve_config_t mock_distance_curve_config_set;
static bool matrix_set_distance_curve_config_result;
static uint32_t matrix_set_distance_curve_config_count;

void diagnostic_mode_init(void) { mock_diagnostic_mode_active = false; }
bool diagnostic_mode_is_active(void) { return mock_diagnostic_mode_active; }
void diagnostic_mode_set_active(bool active) {
  mock_diagnostic_mode_active = active;
}
void diagnostic_mode_touch(void) {}
void diagnostic_mode_task(void) {}

#if defined(RGB_ENABLED)
static rgb_config_t mock_rgb_config;
#endif

bool wear_leveling_write(uint32_t addr, const void *buf, uint32_t len) {
  wear_leveling_write_count++;
  wear_leveling_last_addr = addr;
  wear_leveling_last_len = len;
  memset(wear_leveling_last_data, 0, sizeof(wear_leveling_last_data));
  memcpy(wear_leveling_last_data, buf,
         M_MIN(len, (uint32_t)sizeof(wear_leveling_last_data)));
  return wear_leveling_write_result;
}

bool eeconfig_reset(void) { return true; }

bool eeconfig_reset_profile(uint8_t profile) {
  (void)profile;
  return true;
}

void layout_reset_runtime_state(void) { layout_reset_count++; }

void profile_runtime_reload_current(void) { profile_reload_count++; }

void matrix_recalibrate(bool reset_bottom_out_threshold) {
  (void)reset_bottom_out_threshold;
  recalibrate_count++;
}

const matrix_scan_diagnostics_t *matrix_get_scan_diagnostics(void) {
  return &mock_matrix_diag;
}

void matrix_refresh_scan_diagnostics(void) {}

void matrix_reset_scan_diagnostics(void) {
  memset(&mock_matrix_diag, 0, sizeof(mock_matrix_diag));
  matrix_diag_reset_count++;
}

const kalman_config_t *matrix_get_kalman_config(void) {
  return &mock_kalman_config;
}

bool matrix_set_kalman_config(const kalman_config_t *config) {
  matrix_set_kalman_config_count++;
  if (config != NULL)
    mock_kalman_config_set = *config;
  return matrix_set_kalman_config_result;
}

const distance_curve_config_t *matrix_get_distance_curve_config(void) {
  return &mock_distance_curve_config;
}

bool matrix_set_distance_curve_config(const distance_curve_config_t *config) {
  matrix_set_distance_curve_config_count++;
  if (config != NULL)
    mock_distance_curve_config_set = *config;
  return matrix_set_distance_curve_config_result;
}

const analog_scan_diagnostics_t *analog_get_scan_diagnostics(void) {
  return &mock_analog_diag;
}

void analog_reset_scan_diagnostics(void) {
  const uint16_t mux_sample_delay_us = mock_analog_diag.mux_sample_delay_us;
  const uint8_t mux_step_count = mock_analog_diag.mux_step_count;
  const uint8_t active_bus_count = mock_analog_diag.active_bus_count;
  const uint8_t active_device_count = mock_analog_diag.active_device_count;
  memset(&mock_analog_diag, 0, sizeof(mock_analog_diag));
  mock_analog_diag.mux_sample_delay_us = mux_sample_delay_us;
  mock_analog_diag.mux_step_count = mux_step_count;
  mock_analog_diag.active_bus_count = active_bus_count;
  mock_analog_diag.active_device_count = active_device_count;
  analog_diag_reset_count++;
}

uint16_t analog_get_mux_sample_delay_us(void) {
  return mock_analog_mux_sample_delay_us;
}

bool analog_set_mux_sample_delay_us(uint16_t delay_us) {
  analog_set_mux_sample_delay_count++;
  if (delay_us < 1u || delay_us > 50u || !analog_set_mux_sample_delay_result) {
    return false;
  }

  mock_analog_mux_sample_delay_us = delay_us;
  mock_analog_diag.mux_sample_delay_us = delay_us;
  return true;
}

uint16_t analog_read_raw(uint8_t index) {
  (void)index;
  return 0u;
}

bool analog_diag_channel_identity_enabled(void) {
  return mock_diag_channel_identity_enabled;
}

uint8_t analog_diag_mux_step_count(void) { return 0u; }

uint8_t analog_diag_adc_lane_count(void) { return 0u; }

void analog_diag_capture_baseline(void) {}

bool analog_diag_find_key_step_lane(uint8_t key, uint8_t *step, uint8_t *lane) {
  (void)key;
  (void)step;
  (void)lane;
  return false;
}

uint16_t analog_diag_read_raw_by_step(uint8_t step, uint8_t lane) {
  (void)step;
  (void)lane;
  return 0u;
}

uint16_t analog_diag_read_baseline_by_step(uint8_t step, uint8_t lane) {
  (void)step;
  (void)lane;
  return 0u;
}

uint16_t analog_diag_read_delta_by_step(uint8_t step, uint8_t lane) {
  (void)step;
  (void)lane;
  return 0u;
}

bool analog_diag_run_channel_identity_test(
    uint8_t expected_key, uint16_t min_delta,
    uint8_t max_secondary_ratio_percent,
    analog_channel_identity_result_t *result) {
  (void)expected_key;
  (void)min_delta;
  (void)max_secondary_ratio_percent;
  (void)result;
  return false;
}

uint16_t analog_debug_frame_count(void) { return 0u; }

uint16_t analog_read_debug_frame(uint8_t index) {
  (void)index;
  return 0u;
}

void trackball_get_state(trackball_diagnostic_state_t *state) {
  memset(state, 0, sizeof(*state));
}

void board_reset(void) { board_reset_count++; }

void board_enter_bootloader(void) { board_bootloader_count++; }

uint32_t board_serial(char *buf) {
  static const char serial[] = "CMDTESTSERIAL0123456789ABCDEF";
  memcpy(buf, serial, sizeof(serial) - 1u);
  return (uint32_t)(sizeof(serial) - 1u);
}

#if defined(RGB_ENABLED)
rgb_config_t *rgb_get_config(void) { return &mock_rgb_config; }

void rgb_apply_config(void) { rgb_apply_count++; }

void rgb_set_clock_time(uint8_t hours, uint8_t minutes, uint8_t seconds) {
  host_time_synced = true;
  host_time_hours = hours;
  host_time_minutes = minutes;
  host_time_seconds = seconds;
}
#endif

bool tud_hid_n_ready(uint8_t instance) {
  return instance == USB_ITF_RAW_HID && raw_hid_ready;
}

bool tud_hid_n_report(uint8_t instance, uint8_t report_id, const void *report,
                      uint16_t len) {
  if (instance == USB_ITF_RAW_HID && report_id == 0 && len == RAW_HID_EP_SIZE &&
      raw_hid_report_count < M_ARRAY_SIZE(raw_hid_reports)) {
    memcpy(raw_hid_reports[raw_hid_report_count], report, len);
    raw_hid_report_count++;
  }
  return true;
}

static void command_send_and_flush(const command_in_buffer_t *command) {
  command_process((const uint8_t *)command);
  command_task();
}

static bool buffer_has_nonzero_from(const uint8_t *buffer, uint32_t start) {
  for (uint32_t i = start; i < RAW_HID_EP_SIZE; i++) {
    if (buffer[i] != 0u)
      return true;
  }
  return false;
}

void setUp(void) {
  memset(&mock_eeconfig, 0, sizeof(mock_eeconfig));
  memset(key_matrix, 0, sizeof(key_matrix));
  raw_hid_ready = true;
  raw_hid_report_count = 0;
  memset(raw_hid_reports, 0, sizeof(raw_hid_reports));
  wear_leveling_write_count = 0;
  wear_leveling_write_result = true;
  wear_leveling_last_addr = 0;
  wear_leveling_last_len = 0;
  memset(wear_leveling_last_data, 0, sizeof(wear_leveling_last_data));
  layout_reset_count = 0;
  profile_reload_count = 0;
  recalibrate_count = 0;
  board_reset_count = 0;
  board_bootloader_count = 0;
  rgb_apply_count = 0;
  matrix_diag_reset_count = 0;
  analog_diag_reset_count = 0;
  mock_diag_channel_identity_enabled = false;
  mock_diagnostic_mode_active = false;
  host_time_synced = false;
  host_time_hours = 0;
  host_time_minutes = 0;
  host_time_seconds = 0;
  memset(&mock_matrix_diag, 0, sizeof(mock_matrix_diag));
  memset(&mock_analog_diag, 0, sizeof(mock_analog_diag));
  mock_analog_mux_sample_delay_us = ADC_SAMPLE_DELAY_DEFAULT;
  analog_set_mux_sample_delay_result = true;
  analog_set_mux_sample_delay_count = 0;
  memset(&mock_kalman_config, 0, sizeof(mock_kalman_config));
  memset(&mock_kalman_config_set, 0, sizeof(mock_kalman_config_set));
  matrix_set_kalman_config_result = true;
  matrix_set_kalman_config_count = 0;
  memset(&mock_distance_curve_config, 0, sizeof(mock_distance_curve_config));
  memset(&mock_distance_curve_config_set, 0,
         sizeof(mock_distance_curve_config_set));
  matrix_set_distance_curve_config_result = true;
  matrix_set_distance_curve_config_count = 0;
#if defined(RGB_ENABLED)
  memset(&mock_rgb_config, 0, sizeof(mock_rgb_config));
#endif
  command_init();
}

void tearDown(void) {}

void test_command_short_response_clears_previous_payload(void) {
  command_in_buffer_t metadata = {
      .command_id = COMMAND_GET_METADATA,
      .metadata = {.offset = 0},
  };
  command_in_buffer_t get_profile = {
      .command_id = COMMAND_GET_PROFILE,
  };

  mock_eeconfig.current_profile = 2;

  command_send_and_flush(&metadata);
  command_send_and_flush(&get_profile);

  TEST_ASSERT_EQUAL_UINT32(2, raw_hid_report_count);
  TEST_ASSERT_TRUE(buffer_has_nonzero_from(raw_hid_reports[0], 5));
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_PROFILE, raw_hid_reports[1][0]);
  TEST_ASSERT_EQUAL_UINT8(2, raw_hid_reports[1][1]);
  for (uint32_t i = 2; i < RAW_HID_EP_SIZE; i++) {
    TEST_ASSERT_EQUAL_UINT8(0, raw_hid_reports[1][i]);
  }
}

void test_command_invalid_keymap_range_returns_unknown_without_write(void) {
  command_in_buffer_t set_keymap = {
      .command_id = COMMAND_SET_KEYMAP,
      .keymap =
          {
              .profile = 0,
              .layer = 0,
              .offset = NUM_KEYS - 1,
              .len = 2,
          },
  };

  memset(set_keymap.keymap.keymap, 0xA5, sizeof(set_keymap.keymap.keymap));
  command_send_and_flush(&set_keymap);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT32(0, wear_leveling_write_count);
  TEST_ASSERT_EQUAL_UINT32(0, layout_reset_count);
  TEST_ASSERT_EQUAL_UINT32(0, profile_reload_count);
}

void test_command_unknown_command_returns_clean_unknown_response(void) {
  command_in_buffer_t unknown = {
      .command_id = 254,
  };

  command_send_and_flush(&unknown);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
  for (uint32_t i = 1; i < RAW_HID_EP_SIZE; i++) {
    TEST_ASSERT_EQUAL_UINT8(0, raw_hid_reports[0][i]);
  }
}

void test_command_task_waits_until_raw_hid_is_ready(void) {
  command_in_buffer_t get_profile = {
      .command_id = COMMAND_GET_PROFILE,
  };

  mock_eeconfig.current_profile = 1;
  raw_hid_ready = false;

  command_process((const uint8_t *)&get_profile);
  command_task();

  TEST_ASSERT_EQUAL_UINT32(0, raw_hid_report_count);

  raw_hid_ready = true;
  command_task();

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_PROFILE, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT8(1, raw_hid_reports[0][1]);
}

void test_command_enqueue_defers_processing_until_task(void) {
  command_in_buffer_t get_profile = {
      .command_id = COMMAND_GET_PROFILE,
  };

  mock_eeconfig.current_profile = 3;

  TEST_ASSERT_TRUE(
      command_enqueue((const uint8_t *)&get_profile, RAW_HID_EP_SIZE));
  TEST_ASSERT_EQUAL_UINT32(0, raw_hid_report_count);

  command_task();

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_PROFILE, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT8(3, raw_hid_reports[0][1]);
}

void test_command_enqueue_rejects_second_pending_request(void) {
  command_in_buffer_t get_profile = {
      .command_id = COMMAND_GET_PROFILE,
  };

  TEST_ASSERT_TRUE(
      command_enqueue((const uint8_t *)&get_profile, RAW_HID_EP_SIZE));
  TEST_ASSERT_FALSE(
      command_enqueue((const uint8_t *)&get_profile, RAW_HID_EP_SIZE));

  command_task();

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
}

void test_command_get_matrix_scan_diagnostics_returns_current_snapshot(void) {
  command_in_buffer_t get_diag = {
      .command_id = COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS,
  };

  mock_matrix_diag.scan_count = 77u;
  mock_matrix_diag.last_scan_cycles = 8100u;
  mock_matrix_diag.max_scan_cycles = 9100u;
  mock_matrix_diag.last_scan_us = 37u;
  mock_matrix_diag.max_scan_us = 42u;
  mock_matrix_diag.max_sample_delta = 55u;
  mock_matrix_diag.max_sample_velocity = 34u;
  mock_matrix_diag.reserved_filter_mode[0] = 12u;
  mock_matrix_diag.reserved_filter_mode[1] = 7u;
  mock_matrix_diag.reserved_filter_mode[2] = 2u;
  mock_matrix_diag.reserved_filter_mode[3] = 1u;
  mock_matrix_diag.raw_scan_hz = 28750u;
  mock_matrix_diag.last_raw_scan_us = 30u;
  mock_matrix_diag.max_raw_scan_us = 44u;
  mock_matrix_diag.full_scan_generation = 99u;
  mock_matrix_diag.matrix_scan_hz = 14375u;
  mock_matrix_diag.missed_generation_count = 2u;
  mock_matrix_diag.matrix_processing_divider = 2u;
  mock_matrix_diag.intentional_skip_count = 11u;
  mock_matrix_diag.coalesced_generation_count = 17u;
  mock_matrix_diag.overload_missed_generation_count = 3u;
  mock_matrix_diag.scheduler_budget_exhausted_count = 5u;
  mock_matrix_diag.matrix_catchup_scan_count = 13u;
  mock_matrix_diag.expected_matrix_scan_hz = 14375u;
  mock_matrix_diag.matrix_fast_overrun_count = 9u;

  command_send_and_flush(&get_diag);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_MATRIX_SCAN_DIAGNOSTICS,
                          raw_hid_reports[0][0]);

  command_out_buffer_t out = {0};
  memcpy(&out, raw_hid_reports[0], sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(77u, out.matrix_scan_diagnostics.matrix_scan_count);
  TEST_ASSERT_EQUAL_UINT32(14375u, out.matrix_scan_diagnostics.matrix_scan_hz);
  TEST_ASSERT_EQUAL_UINT32(37u,
                           out.matrix_scan_diagnostics.last_matrix_scan_us);
  TEST_ASSERT_EQUAL_UINT32(42u, out.matrix_scan_diagnostics.max_matrix_scan_us);
  TEST_ASSERT_EQUAL_UINT32(28750u, out.matrix_scan_diagnostics.raw_scan_hz);
  TEST_ASSERT_EQUAL_UINT32(30u, out.matrix_scan_diagnostics.last_raw_scan_us);
  TEST_ASSERT_EQUAL_UINT32(44u, out.matrix_scan_diagnostics.max_raw_scan_us);
  TEST_ASSERT_EQUAL_UINT32(99u,
                           out.matrix_scan_diagnostics.full_scan_generation);
  TEST_ASSERT_EQUAL_UINT32(2u,
                           out.matrix_scan_diagnostics.missed_generation_count);
  TEST_ASSERT_EQUAL_UINT32(
      2u, out.matrix_scan_diagnostics.matrix_processing_divider);
  TEST_ASSERT_EQUAL_UINT32(11u,
                           out.matrix_scan_diagnostics.intentional_skip_count);
  TEST_ASSERT_EQUAL_UINT32(
      17u, out.matrix_scan_diagnostics.coalesced_generation_count);
  TEST_ASSERT_EQUAL_UINT32(
      3u, out.matrix_scan_diagnostics.overload_missed_generation_count);
  TEST_ASSERT_EQUAL_UINT32(
      5u, out.matrix_scan_diagnostics.scheduler_budget_exhausted_count);
  TEST_ASSERT_EQUAL_UINT32(
      13u, out.matrix_scan_diagnostics.matrix_catchup_scan_count);
  TEST_ASSERT_EQUAL_UINT16(14375u,
                           out.matrix_scan_diagnostics.expected_matrix_scan_hz);
  TEST_ASSERT_EQUAL_UINT8(
      9u, out.matrix_scan_diagnostics.matrix_fast_overrun_count);
}

void test_command_reset_matrix_scan_diagnostics_clears_snapshot(void) {
  command_in_buffer_t reset_diag = {
      .command_id = COMMAND_RESET_MATRIX_SCAN_DIAGNOSTICS,
  };

  mock_matrix_diag.scan_count = 1u;
  mock_matrix_diag.max_scan_cycles = 2u;
  mock_matrix_diag.max_sample_delta = 3u;

  command_send_and_flush(&reset_diag);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_RESET_MATRIX_SCAN_DIAGNOSTICS,
                          raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT32(1, matrix_diag_reset_count);
  TEST_ASSERT_EQUAL_UINT32(0, mock_matrix_diag.scan_count);
  TEST_ASSERT_EQUAL_UINT32(0, mock_matrix_diag.max_scan_cycles);
  TEST_ASSERT_EQUAL_UINT16(0, mock_matrix_diag.max_sample_delta);
}

void test_command_get_analog_scan_diagnostics_returns_current_snapshot(void) {
  command_in_buffer_t get_diag = {
      .command_id = COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS,
  };

  mock_analog_diag.mux_sample_delay_us = 10u;
  mock_analog_diag.mux_step_count = 8u;
  mock_analog_diag.scan_count = 123u;
  mock_analog_diag.last_scan_cycles = 8200u;
  mock_analog_diag.max_scan_cycles = 9100u;
  mock_analog_diag.last_scan_us = 38u;
  mock_analog_diag.max_scan_us = 42u;
  mock_analog_diag.estimated_scan_hz = 12345u;
  mock_analog_diag.bad_channel_id_count = 3u;
  mock_analog_diag.dma_overrun_count = 4u;
  mock_analog_diag.overrun_count = 7u;
  mock_analog_diag.spi_error_count = 5u;
  mock_analog_diag.missed_scan_count = 6u;

  command_send_and_flush(&get_diag);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_ANALOG_SCAN_DIAGNOSTICS,
                          raw_hid_reports[0][0]);

  command_out_buffer_t out = {0};
  memcpy(&out, raw_hid_reports[0], sizeof(out));
  TEST_ASSERT_EQUAL_UINT16(10u,
                           out.analog_scan_diagnostics.mux_sample_delay_us);
  TEST_ASSERT_EQUAL_UINT16(8u, out.analog_scan_diagnostics.mux_step_count);
  TEST_ASSERT_EQUAL_UINT32(123u, out.analog_scan_diagnostics.scan_count);
  TEST_ASSERT_EQUAL_UINT32(8200u, out.analog_scan_diagnostics.last_scan_cycles);
  TEST_ASSERT_EQUAL_UINT32(9100u, out.analog_scan_diagnostics.max_scan_cycles);
  TEST_ASSERT_EQUAL_UINT32(38u, out.analog_scan_diagnostics.last_scan_us);
  TEST_ASSERT_EQUAL_UINT32(42u, out.analog_scan_diagnostics.max_scan_us);
  TEST_ASSERT_EQUAL_UINT32(12345u,
                           out.analog_scan_diagnostics.estimated_scan_hz);
  TEST_ASSERT_EQUAL_UINT32(3u,
                           out.analog_scan_diagnostics.bad_channel_id_count);
  TEST_ASSERT_EQUAL_UINT32(4u, out.analog_scan_diagnostics.dma_overrun_count);
  TEST_ASSERT_EQUAL_UINT32(7u, out.analog_scan_diagnostics.overrun_count);
  TEST_ASSERT_EQUAL_UINT32(5u, out.analog_scan_diagnostics.spi_error_count);
  TEST_ASSERT_EQUAL_UINT32(6u, out.analog_scan_diagnostics.missed_scan_count);
}

void test_command_reset_analog_scan_diagnostics_clears_snapshot(void) {
  command_in_buffer_t reset_diag = {
      .command_id = COMMAND_RESET_ANALOG_SCAN_DIAGNOSTICS,
  };

  mock_analog_diag.scan_count = 1u;
  mock_analog_diag.max_scan_cycles = 2u;
  mock_analog_diag.mux_sample_delay_us = 12u;
  mock_analog_diag.mux_step_count = 8u;

  command_send_and_flush(&reset_diag);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_RESET_ANALOG_SCAN_DIAGNOSTICS,
                          raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT32(1, analog_diag_reset_count);
  TEST_ASSERT_EQUAL_UINT32(0, mock_analog_diag.scan_count);
  TEST_ASSERT_EQUAL_UINT32(0, mock_analog_diag.max_scan_cycles);
  TEST_ASSERT_EQUAL_UINT16(12u, mock_analog_diag.mux_sample_delay_us);
  TEST_ASSERT_EQUAL_UINT8(8u, mock_analog_diag.mux_step_count);
}

void test_command_get_analog_scan_config_returns_current_value(void) {
  command_in_buffer_t get_config = {
      .command_id = COMMAND_GET_ANALOG_SCAN_CONFIG,
  };

  mock_analog_mux_sample_delay_us = 15u;

  command_send_and_flush(&get_config);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_ANALOG_SCAN_CONFIG,
                          raw_hid_reports[0][0]);

  command_out_buffer_t out = {0};
  memcpy(&out, raw_hid_reports[0], sizeof(out));
  TEST_ASSERT_EQUAL_UINT16(15u, out.analog_scan_config.mux_sample_delay_us);
}

void test_command_set_analog_scan_config_updates_runtime_and_persists(void) {
  command_in_buffer_t set_config = {
      .command_id = COMMAND_SET_ANALOG_SCAN_CONFIG,
      .analog_scan_config = {.mux_sample_delay_us = 12u},
  };

  mock_analog_mux_sample_delay_us = 20u;

  command_send_and_flush(&set_config);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_SET_ANALOG_SCAN_CONFIG,
                          raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT16(12u, mock_analog_mux_sample_delay_us);
  TEST_ASSERT_EQUAL_UINT32(1u, analog_set_mux_sample_delay_count);
  TEST_ASSERT_EQUAL_UINT32(1u, wear_leveling_write_count);
  TEST_ASSERT_EQUAL_UINT32(offsetof(eeconfig_t, mux_sample_delay_us),
                           wear_leveling_last_addr);
  TEST_ASSERT_EQUAL_UINT32(sizeof(mock_eeconfig.mux_sample_delay_us),
                           wear_leveling_last_len);

  uint16_t written_delay_us = 0u;
  memcpy(&written_delay_us, wear_leveling_last_data, sizeof(written_delay_us));
  TEST_ASSERT_EQUAL_UINT16(12u, written_delay_us);
}

void test_command_set_analog_scan_config_rejects_invalid_value_without_write(
    void) {
  command_in_buffer_t set_config = {
      .command_id = COMMAND_SET_ANALOG_SCAN_CONFIG,
      .analog_scan_config = {.mux_sample_delay_us = 0u},
  };

  mock_analog_mux_sample_delay_us = 20u;

  command_send_and_flush(&set_config);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT16(20u, mock_analog_mux_sample_delay_us);
  TEST_ASSERT_EQUAL_UINT32(1u, analog_set_mux_sample_delay_count);
  TEST_ASSERT_EQUAL_UINT32(0u, wear_leveling_write_count);
}

void test_command_get_kalman_config_returns_current_value(void) {
  command_in_buffer_t get_config = {
      .command_id = COMMAND_GET_KALMAN_CONFIG,
  };

  mock_kalman_config = (kalman_config_t){
      .position_gain = 0.12f,
      .velocity_gain = 0.34f,
      .velocity_damping = 0.56f,
      .rt_down_min_velocity = 0.78f,
      .rt_up_min_velocity = 0.91f,
      .innovation_event_threshold = 1.23f,
      .bottom_out_hold_scans = 7,
      .bottom_out_rt_up = 4,
      .noise_deadzone = 9u,
  };

  command_send_and_flush(&get_config);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_KALMAN_CONFIG, raw_hid_reports[0][0]);

  command_out_buffer_t out = {0};
  memcpy(&out, raw_hid_reports[0], sizeof(out));
  TEST_ASSERT_EQUAL_FLOAT(0.12f, out.kalman_config.position_gain);
  TEST_ASSERT_EQUAL_FLOAT(0.34f, out.kalman_config.velocity_gain);
  TEST_ASSERT_EQUAL_FLOAT(0.56f, out.kalman_config.velocity_damping);
  TEST_ASSERT_EQUAL_FLOAT(0.78f, out.kalman_config.rt_down_min_velocity);
  TEST_ASSERT_EQUAL_FLOAT(0.91f, out.kalman_config.rt_up_min_velocity);
  TEST_ASSERT_EQUAL_FLOAT(1.23f, out.kalman_config.innovation_event_threshold);
  TEST_ASSERT_EQUAL_UINT16(7u, out.kalman_config.bottom_out_hold_scans);
  TEST_ASSERT_EQUAL_UINT8(4u, out.kalman_config.bottom_out_rt_up);
  TEST_ASSERT_EQUAL_UINT16(9u, out.kalman_config.noise_deadzone);
}

void test_command_set_kalman_config_updates_runtime_and_persists(void) {
  command_in_buffer_t set_config = {
      .command_id = COMMAND_SET_KALMAN_CONFIG,
      .kalman_config =
          {
              .position_gain = 0.22f,
              .velocity_gain = 0.44f,
              .velocity_damping = 0.55f,
              .rt_down_min_velocity = 0.66f,
              .rt_up_min_velocity = 0.77f,
              .innovation_event_threshold = 0.88f,
              .bottom_out_hold_scans = 5,
              .bottom_out_rt_up = 2,
              .noise_deadzone = 3u,
          },
  };

  command_send_and_flush(&set_config);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_SET_KALMAN_CONFIG, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT32(1u, matrix_set_kalman_config_count);
  TEST_ASSERT_EQUAL_FLOAT(0.22f, mock_kalman_config_set.position_gain);
  TEST_ASSERT_EQUAL_FLOAT(0.44f, mock_kalman_config_set.velocity_gain);
  TEST_ASSERT_EQUAL_FLOAT(0.55f, mock_kalman_config_set.velocity_damping);
  TEST_ASSERT_EQUAL_FLOAT(0.66f, mock_kalman_config_set.rt_down_min_velocity);
  TEST_ASSERT_EQUAL_FLOAT(0.77f, mock_kalman_config_set.rt_up_min_velocity);
  TEST_ASSERT_EQUAL_FLOAT(0.88f,
                          mock_kalman_config_set.innovation_event_threshold);
  TEST_ASSERT_EQUAL_UINT16(5u, mock_kalman_config_set.bottom_out_hold_scans);
  TEST_ASSERT_EQUAL_UINT8(2u, mock_kalman_config_set.bottom_out_rt_up);
  TEST_ASSERT_EQUAL_UINT16(3u, mock_kalman_config_set.noise_deadzone);
}

void test_command_set_kalman_config_rejects_invalid_value_without_write(void) {
  command_in_buffer_t set_config = {
      .command_id = COMMAND_SET_KALMAN_CONFIG,
      .kalman_config =
          {
              .position_gain = 0.22f,
              .velocity_gain = 0.44f,
              .velocity_damping = 0.55f,
              .rt_down_min_velocity = 0.66f,
              .rt_up_min_velocity = 0.77f,
              .innovation_event_threshold = 0.88f,
              .bottom_out_hold_scans = 5,
              .bottom_out_rt_up = 2,
              .noise_deadzone = 3u,
          },
  };

  matrix_set_kalman_config_result = false;

  command_send_and_flush(&set_config);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT32(1u, matrix_set_kalman_config_count);
}

static void send_distance_curve_set_chunk(uint16_t offset, uint8_t len,
                                          const uint8_t *data) {
  command_in_buffer_t cmd = {0};
  cmd.command_id = COMMAND_SET_DISTANCE_CURVE_CONFIG;
  cmd.distance_curve_config_set.offset = offset;
  cmd.distance_curve_config_set.len = len;
  if (len > 0u)
    memcpy(cmd.distance_curve_config_set.data, data, len);
  command_send_and_flush(&cmd);
}

static void send_distance_curve_get(uint16_t offset) {
  command_in_buffer_t cmd = {0};
  cmd.command_id = COMMAND_GET_DISTANCE_CURVE_CONFIG;
  cmd.distance_curve_config_get.offset = offset;
  command_send_and_flush(&cmd);
}

void test_command_get_distance_curve_config_returns_first_chunk(void) {
  memset(&mock_distance_curve_config, 0, sizeof(mock_distance_curve_config));
  mock_distance_curve_config.num_curves = 2;
  mock_distance_curve_config.curves[0].num_points = 2;
  mock_distance_curve_config.curves[0].total_travel_um = 4000u;
  mock_distance_curve_config.curves[0].points[0].adc = 0u;
  mock_distance_curve_config.curves[0].points[0].dist = 0u;
  mock_distance_curve_config.curves[0].points[1].adc = 255u;
  mock_distance_curve_config.curves[0].points[1].dist = 255u;
  mock_distance_curve_config.curves[1].num_points = 1;
  mock_distance_curve_config.curves[1].total_travel_um = 3500u;
  mock_distance_curve_config.curves[1].points[0].adc = 128u;
  mock_distance_curve_config.curves[1].points[0].dist = 64u;
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    mock_distance_curve_config.key_curve[i] = (uint8_t)(i & 0xFFu);
  }

  send_distance_curve_get(0u);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_DISTANCE_CURVE_CONFIG,
                          raw_hid_reports[0][0]);

  command_out_buffer_t out = {0};
  memcpy(&out, raw_hid_reports[0], sizeof(out));
  const uint32_t config_size = sizeof(mock_distance_curve_config);
  const uint8_t expected_len = config_size < COMMAND_DISTANCE_CURVE_CHUNK_SIZE
                                   ? (uint8_t)config_size
                                   : COMMAND_DISTANCE_CURVE_CHUNK_SIZE;
  TEST_ASSERT_EQUAL_UINT8(expected_len, out.distance_curve_config.len);
  TEST_ASSERT_EQUAL_UINT8(2u, out.distance_curve_config.data[0]);
  TEST_ASSERT_EQUAL_UINT8(2u, out.distance_curve_config.data[1]);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)4000u, out.distance_curve_config.data[2]);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)(4000u >> 8),
                          out.distance_curve_config.data[3]);
  TEST_ASSERT_EQUAL_UINT8(0u, out.distance_curve_config.data[4]);
  TEST_ASSERT_EQUAL_UINT8(0u, out.distance_curve_config.data[5]);
  TEST_ASSERT_EQUAL_UINT8(255u, out.distance_curve_config.data[6]);
  TEST_ASSERT_EQUAL_UINT8(255u, out.distance_curve_config.data[7]);
}

void test_command_get_distance_curve_config_returns_final_chunk(void) {
  memset(&mock_distance_curve_config, 0, sizeof(mock_distance_curve_config));
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    mock_distance_curve_config.key_curve[i] = (uint8_t)(0xA5u + i);
  }

  const uint32_t config_size = sizeof(mock_distance_curve_config);
  const uint16_t offset = (uint16_t)(config_size - 4u);

  send_distance_curve_get(offset);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_DISTANCE_CURVE_CONFIG,
                          raw_hid_reports[0][0]);

  command_out_buffer_t out = {0};
  memcpy(&out, raw_hid_reports[0], sizeof(out));
  TEST_ASSERT_EQUAL_UINT8(4u, out.distance_curve_config.len);
  const uint32_t first_key_index =
      offset - offsetof(distance_curve_config_t, key_curve);
  for (uint32_t i = 0; i < 4u; i++) {
    TEST_ASSERT_EQUAL_UINT8(
        mock_distance_curve_config.key_curve[first_key_index + i],
        out.distance_curve_config.data[i]);
  }
}

void test_command_get_distance_curve_config_rejects_invalid_offset(void) {
  send_distance_curve_get((uint16_t)sizeof(mock_distance_curve_config));

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
}

void test_command_set_distance_curve_config_updates_runtime_and_persists(void) {
  distance_curve_config_t expected;
  memset(&expected, 0, sizeof(expected));
  expected.num_curves = 1;
  expected.curves[0].num_points = 3;
  expected.curves[0].total_travel_um = 4200u;
  expected.curves[0].points[0].adc = 0u;
  expected.curves[0].points[0].dist = 10u;
  expected.curves[0].points[1].adc = 127u;
  expected.curves[0].points[1].dist = 128u;
  expected.curves[0].points[2].adc = 255u;
  expected.curves[0].points[2].dist = 250u;
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    expected.key_curve[i] = (uint8_t)((i * 7u) & 0xFFu);
  }

  const uint8_t *src = (const uint8_t *)&expected;
  const uint32_t config_size = sizeof(expected);
  uint32_t offset = 0;
  while (offset < config_size) {
    uint32_t len = config_size - offset;
    if (len > COMMAND_DISTANCE_CURVE_CHUNK_SIZE)
      len = COMMAND_DISTANCE_CURVE_CHUNK_SIZE;

    send_distance_curve_set_chunk((uint16_t)offset, (uint8_t)len, &src[offset]);

    const bool is_final = (offset + len == config_size);
    TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
    TEST_ASSERT_EQUAL_UINT8(COMMAND_SET_DISTANCE_CURVE_CONFIG,
                            raw_hid_reports[0][0]);
    TEST_ASSERT_EQUAL_UINT32(is_final ? 1u : 0u,
                             matrix_set_distance_curve_config_count);

    raw_hid_report_count = 0;
    offset += len;
  }

  TEST_ASSERT_EQUAL_UINT32(1u, matrix_set_distance_curve_config_count);
  TEST_ASSERT_EQUAL_MEMORY(&expected, &mock_distance_curve_config_set,
                           sizeof(expected));
}

void test_command_set_distance_curve_config_rejects_invalid_value_without_write(
    void) {
  distance_curve_config_t expected;
  memset(&expected, 0, sizeof(expected));
  expected.num_curves = 1;
  expected.curves[0].num_points = 2;
  expected.curves[0].total_travel_um = 3000u;
  expected.curves[0].points[0].adc = 0u;
  expected.curves[0].points[0].dist = 0u;
  expected.curves[0].points[1].adc = 255u;
  expected.curves[0].points[1].dist = 255u;
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    expected.key_curve[i] = (uint8_t)i;
  }

  matrix_set_distance_curve_config_result = false;

  const uint8_t *src = (const uint8_t *)&expected;
  const uint32_t config_size = sizeof(expected);
  uint32_t offset = 0;
  while (offset < config_size) {
    uint32_t len = config_size - offset;
    if (len > COMMAND_DISTANCE_CURVE_CHUNK_SIZE)
      len = COMMAND_DISTANCE_CURVE_CHUNK_SIZE;

    send_distance_curve_set_chunk((uint16_t)offset, (uint8_t)len, &src[offset]);

    offset += len;
    if (offset < config_size)
      raw_hid_report_count = 0;
  }

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT32(1u, matrix_set_distance_curve_config_count);
}

void test_command_set_distance_curve_config_rejects_invalid_range(void) {
  const uint8_t dummy[4] = {0x12, 0x34, 0x56, 0x78};

  send_distance_curve_set_chunk(
      (uint16_t)(sizeof(distance_curve_config_t) - 2u), 4u, dummy);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT32(0u, matrix_set_distance_curve_config_count);
}

void test_command_set_distance_curve_config_rejects_nonzero_first_chunk(void) {
  const uint8_t dummy[8] = {0};

  send_distance_curve_set_chunk(8u, 8u, dummy);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT32(0u, matrix_set_distance_curve_config_count);
}

void test_command_set_distance_curve_config_resets_staging_on_offset_zero(
    void) {
  distance_curve_config_t expected;
  memset(&expected, 0, sizeof(expected));
  expected.num_curves = 1;
  expected.curves[0].num_points = 2;
  expected.curves[0].total_travel_um = 5000u;
  expected.curves[0].points[0].adc = 0u;
  expected.curves[0].points[0].dist = 0u;
  expected.curves[0].points[1].adc = 255u;
  expected.curves[0].points[1].dist = 255u;
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    expected.key_curve[i] = (uint8_t)(0xC0u + i);
  }

  const uint8_t *src = (const uint8_t *)&expected;
  const uint32_t config_size = sizeof(expected);

  // First start with a different prefix, then restart cleanly.
  const uint8_t dummy[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  send_distance_curve_set_chunk(0u, 4u, dummy);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_SET_DISTANCE_CURVE_CONFIG,
                          raw_hid_reports[0][0]);
  raw_hid_report_count = 0;

  // Restart from offset 0 with the real config.
  uint32_t offset = 0;
  while (offset < config_size) {
    uint32_t len = config_size - offset;
    if (len > COMMAND_DISTANCE_CURVE_CHUNK_SIZE)
      len = COMMAND_DISTANCE_CURVE_CHUNK_SIZE;

    send_distance_curve_set_chunk((uint16_t)offset, (uint8_t)len, &src[offset]);

    raw_hid_report_count = 0;
    offset += len;
  }

  TEST_ASSERT_EQUAL_UINT32(1u, matrix_set_distance_curve_config_count);
  TEST_ASSERT_EQUAL_MEMORY(&expected, &mock_distance_curve_config_set,
                           sizeof(expected));
}

void test_command_capture_analog_diag_baseline_requires_diag_firmware(void) {
  command_in_buffer_t capture = {
      .command_id = COMMAND_CAPTURE_ANALOG_DIAG_BASELINE,
  };

  command_send_and_flush(&capture);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
}

void test_command_diagnostic_mode_can_be_enabled_and_queried(void) {
  mock_diag_channel_identity_enabled = true;
  command_in_buffer_t enable = {
      .command_id = COMMAND_SET_DIAGNOSTIC_MODE,
      .diagnostic_mode = {.active = true},
  };

  command_send_and_flush(&enable);

  TEST_ASSERT_TRUE(mock_diagnostic_mode_active);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_SET_DIAGNOSTIC_MODE, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT8(1u, raw_hid_reports[0][1]);

  raw_hid_report_count = 0;
  command_in_buffer_t get = {.command_id = COMMAND_GET_DIAGNOSTIC_MODE};
  command_send_and_flush(&get);

  TEST_ASSERT_EQUAL_UINT8(COMMAND_GET_DIAGNOSTIC_MODE, raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT8(1u, raw_hid_reports[0][1]);
}

void test_command_run_analog_channel_identity_requires_diag_firmware(void) {
  command_in_buffer_t run_test = {
      .command_id = COMMAND_RUN_ANALOG_CHANNEL_IDENTITY_TEST,
      .analog_channel_identity_test =
          {
              .expected_key = 0u,
              .min_delta = 32u,
              .max_secondary_ratio_percent = 20u,
          },
  };

  command_send_and_flush(&run_test);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_UNKNOWN, raw_hid_reports[0][0]);
}

#if defined(RGB_ENABLED)
void test_command_set_host_time_updates_runtime_clock_without_flash_write(
    void) {
  command_in_buffer_t set_host_time = {
      .command_id = COMMAND_SET_HOST_TIME,
      .host_time =
          {
              .hours = 12,
              .minutes = 34,
              .seconds = 56,
          },
  };

  command_send_and_flush(&set_host_time);

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8(COMMAND_SET_HOST_TIME, raw_hid_reports[0][0]);
  TEST_ASSERT_TRUE(host_time_synced);
  TEST_ASSERT_EQUAL_UINT8(12, host_time_hours);
  TEST_ASSERT_EQUAL_UINT8(34, host_time_minutes);
  TEST_ASSERT_EQUAL_UINT8(56, host_time_seconds);
  TEST_ASSERT_EQUAL_UINT32(0, wear_leveling_write_count);
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_command_short_response_clears_previous_payload);
  RUN_TEST(test_command_invalid_keymap_range_returns_unknown_without_write);
  RUN_TEST(test_command_unknown_command_returns_clean_unknown_response);
  RUN_TEST(test_command_task_waits_until_raw_hid_is_ready);
  RUN_TEST(test_command_enqueue_defers_processing_until_task);
  RUN_TEST(test_command_enqueue_rejects_second_pending_request);
  RUN_TEST(test_command_get_matrix_scan_diagnostics_returns_current_snapshot);
  RUN_TEST(test_command_reset_matrix_scan_diagnostics_clears_snapshot);
  RUN_TEST(test_command_get_analog_scan_diagnostics_returns_current_snapshot);
  RUN_TEST(test_command_reset_analog_scan_diagnostics_clears_snapshot);
  RUN_TEST(test_command_get_analog_scan_config_returns_current_value);
  RUN_TEST(test_command_set_analog_scan_config_updates_runtime_and_persists);
  RUN_TEST(
      test_command_set_analog_scan_config_rejects_invalid_value_without_write);
  RUN_TEST(test_command_get_kalman_config_returns_current_value);
  RUN_TEST(test_command_set_kalman_config_updates_runtime_and_persists);
  RUN_TEST(test_command_set_kalman_config_rejects_invalid_value_without_write);
  RUN_TEST(test_command_get_distance_curve_config_returns_first_chunk);
  RUN_TEST(test_command_get_distance_curve_config_returns_final_chunk);
  RUN_TEST(test_command_get_distance_curve_config_rejects_invalid_offset);
  RUN_TEST(test_command_set_distance_curve_config_updates_runtime_and_persists);
  RUN_TEST(
      test_command_set_distance_curve_config_rejects_invalid_value_without_write);
  RUN_TEST(test_command_set_distance_curve_config_rejects_invalid_range);
  RUN_TEST(test_command_set_distance_curve_config_rejects_nonzero_first_chunk);
  RUN_TEST(
      test_command_set_distance_curve_config_resets_staging_on_offset_zero);
  RUN_TEST(test_command_capture_analog_diag_baseline_requires_diag_firmware);
  RUN_TEST(test_command_diagnostic_mode_can_be_enabled_and_queried);
  RUN_TEST(test_command_run_analog_channel_identity_requires_diag_firmware);
#if defined(RGB_ENABLED)
  RUN_TEST(
      test_command_set_host_time_updates_runtime_clock_without_flash_write);
#endif
  return UNITY_END();
}
