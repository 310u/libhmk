#include <unity.h>

#include "commands.h"
#include "layout.h"
#include "matrix.h"
#include "rgb.h"
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
static uint32_t layout_reset_count;
static uint32_t profile_reload_count;
static uint32_t recalibrate_count;
static uint32_t board_reset_count;
static uint32_t board_bootloader_count;
static uint32_t rgb_apply_count;
static bool host_time_synced;
static uint8_t host_time_hours;
static uint8_t host_time_minutes;
static uint8_t host_time_seconds;

#if defined(RGB_ENABLED)
static rgb_config_t mock_rgb_config;
#endif

bool wear_leveling_write(uint32_t addr, const void *buf, uint32_t len) {
  (void)addr;
  (void)buf;
  (void)len;
  wear_leveling_write_count++;
  return true;
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
  if (instance == USB_ITF_RAW_HID && report_id == 0 &&
      len == RAW_HID_EP_SIZE && raw_hid_report_count < M_ARRAY_SIZE(raw_hid_reports)) {
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
  layout_reset_count = 0;
  profile_reload_count = 0;
  recalibrate_count = 0;
  board_reset_count = 0;
  board_bootloader_count = 0;
  rgb_apply_count = 0;
  host_time_synced = false;
  host_time_hours = 0;
  host_time_minutes = 0;
  host_time_seconds = 0;
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

  TEST_ASSERT_TRUE(command_enqueue((const uint8_t *)&get_profile, RAW_HID_EP_SIZE));
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

  TEST_ASSERT_TRUE(command_enqueue((const uint8_t *)&get_profile, RAW_HID_EP_SIZE));
  TEST_ASSERT_FALSE(command_enqueue((const uint8_t *)&get_profile, RAW_HID_EP_SIZE));

  command_task();

  TEST_ASSERT_EQUAL_UINT32(1, raw_hid_report_count);
}

#if defined(RGB_ENABLED)
void test_command_set_host_time_updates_runtime_clock_without_flash_write(void) {
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
#if defined(RGB_ENABLED)
  RUN_TEST(test_command_set_host_time_updates_runtime_clock_without_flash_write);
#endif
  return UNITY_END();
}
