#include <unity.h>

#include "commands.h"
#include "hid.h"
#include "keycodes.h"
#include "tusb.h"
#include "usb_descriptors.h"

static bool keyboard_ready;
static bool mouse_ready;
static bool hid_ready;
static bool raw_hid_ready;
static bool usb_suspended;
static uint32_t mock_timer;
static uint32_t mock_cycle;

static uint32_t report_count;
static uint32_t command_enqueue_count;
static uint32_t keyboard_ready_checks;
static uint32_t mouse_ready_checks;
static uint32_t hid_ready_checks;
static uint32_t raw_hid_ready_checks;
static uint32_t wakeup_count;
static uint8_t last_instance;
static uint8_t last_report_id;
static uint16_t last_report_len;
static uint16_t last_report_value;
static hid_nkro_kb_report_t keyboard_reports[8];
static uint8_t keyboard_report_count;
static hid_mouse_report_t mouse_reports[8];
static uint8_t mouse_report_count;
static uint8_t raw_hid_reports[8][RAW_HID_EP_SIZE];
static uint8_t raw_hid_report_count;
static uint8_t last_command_packet[RAW_HID_EP_SIZE];
static uint16_t last_command_packet_len;

const uint16_t keycode_to_hid[256] = {
    [KC_A] = 0x0004,
    [KC_AUDIO_MUTE] = 0x00E2,
};

void tud_hid_report_complete_cb(uint8_t instance, const uint8_t *report,
                                uint16_t len);
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           const uint8_t *buffer, uint16_t bufsize);

bool command_enqueue(const uint8_t *buffer, uint16_t len) {
  command_enqueue_count++;
  last_command_packet_len = len;
  if (len <= sizeof(last_command_packet))
    memcpy(last_command_packet, buffer, len);
  return true;
}

void command_process(const uint8_t *buffer) {
  (void)buffer;
}

uint32_t timer_read(void) { return mock_timer++; }

uint32_t board_cycle_count(void) {
  mock_cycle += 100;
  return mock_cycle;
}

void tud_task(void) {}

bool tud_hid_n_report(uint8_t instance, uint8_t report_id, const void *report,
                      uint16_t len) {
  report_count++;
  last_instance = instance;
  last_report_id = report_id;
  last_report_len = len;
  last_report_value = 0;
  memcpy(&last_report_value, report,
         len > sizeof(last_report_value) ? sizeof(last_report_value) : len);
  if (instance == USB_ITF_KEYBOARD && keyboard_report_count < 8 &&
      len == sizeof(hid_nkro_kb_report_t)) {
    memcpy(&keyboard_reports[keyboard_report_count], report, len);
    keyboard_report_count++;
  }
  if (instance == USB_ITF_MOUSE && report_id == 0 &&
      mouse_report_count < 8 && len == sizeof(hid_mouse_report_t)) {
    memcpy(&mouse_reports[mouse_report_count], report, len);
    mouse_report_count++;
  }
  if (instance == USB_ITF_RAW_HID && report_id == 0 &&
      raw_hid_report_count < 8 && len == RAW_HID_EP_SIZE) {
    memcpy(raw_hid_reports[raw_hid_report_count], report, len);
    raw_hid_report_count++;
  }
  return true;
}

bool tud_hid_n_ready(uint8_t instance) {
  switch (instance) {
  case USB_ITF_KEYBOARD:
    keyboard_ready_checks++;
    return keyboard_ready;

  case USB_ITF_MOUSE:
    mouse_ready_checks++;
    return mouse_ready;

  case USB_ITF_HID:
    hid_ready_checks++;
    return hid_ready;

  case USB_ITF_RAW_HID:
    raw_hid_ready_checks++;
    return raw_hid_ready;

  default:
    return false;
  }
}

bool tud_suspended(void) { return usb_suspended; }

void tud_remote_wakeup(void) {
  wakeup_count++;
  usb_suspended = false;
}

static void reset_observations(void) {
  report_count = 0;
  command_enqueue_count = 0;
  keyboard_ready_checks = 0;
  mouse_ready_checks = 0;
  hid_ready_checks = 0;
  raw_hid_ready_checks = 0;
  wakeup_count = 0;
  last_instance = UINT8_MAX;
  last_report_id = UINT8_MAX;
  last_report_len = 0;
  last_report_value = 0;
  memset(keyboard_reports, 0, sizeof(keyboard_reports));
  keyboard_report_count = 0;
  memset(mouse_reports, 0, sizeof(mouse_reports));
  mouse_report_count = 0;
  memset(raw_hid_reports, 0, sizeof(raw_hid_reports));
  raw_hid_report_count = 0;
  memset(last_command_packet, 0, sizeof(last_command_packet));
  last_command_packet_len = 0;
}

void setUp(void) {
  hid_init();
  keyboard_ready = true;
  mouse_ready = true;
  hid_ready = true;
  raw_hid_ready = false;
  usb_suspended = false;
  mock_timer = 0;
  mock_cycle = 0;
  reset_observations();
}

void tearDown(void) {}

void test_hid_send_reports_is_non_blocking_per_interface(void) {
  keyboard_ready = false;
  hid_ready = true;
  hid_keycode_add(KC_AUDIO_MUTE);

  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT8(USB_ITF_HID, last_instance);
  TEST_ASSERT_EQUAL_UINT8(REPORT_ID_CONSUMER_CONTROL, last_report_id);
  TEST_ASSERT_EQUAL_HEX16(0x00E2, last_report_value);

  keyboard_ready = true;
  hid_ready = true;
  hid_keycode_remove(KC_AUDIO_MUTE);
  hid_send_reports();

  reset_observations();
  keyboard_ready = true;
  hid_ready = false;
  hid_keycode_add(KC_A);

  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT8(USB_ITF_KEYBOARD, last_instance);
  TEST_ASSERT_EQUAL_UINT8(0, last_report_id);
  TEST_ASSERT_EQUAL_UINT16(sizeof(hid_nkro_kb_report_t), last_report_len);

  keyboard_ready = true;
  hid_ready = true;
  hid_keycode_remove(KC_A);
  hid_send_reports();

  reset_observations();
  keyboard_ready = false;
  mouse_ready = false;
  hid_ready = false;

  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(0, report_count);
  TEST_ASSERT_EQUAL_UINT32(1, keyboard_ready_checks);
  TEST_ASSERT_EQUAL_UINT32(1, mouse_ready_checks);
  TEST_ASSERT_EQUAL_UINT32(1, hid_ready_checks);
  TEST_ASSERT_EQUAL_UINT32(0, wakeup_count);
}

void test_hid_preserves_transient_keyboard_taps_while_interface_busy(void) {
  keyboard_ready = false;

  hid_keycode_add(KC_A);
  hid_send_reports();
  hid_keycode_remove(KC_A);
  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(0, report_count);

  keyboard_ready = true;
  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT8(1, keyboard_report_count);
  TEST_ASSERT_EQUAL_UINT8(0x04, keyboard_reports[0].keycodes[0]);
  TEST_ASSERT_BITS_HIGH(1u << 4, keyboard_reports[0].bitmap[0]);

  tud_hid_report_complete_cb(USB_ITF_KEYBOARD,
                             (const uint8_t *)&keyboard_reports[0],
                             sizeof(hid_nkro_kb_report_t));

  TEST_ASSERT_EQUAL_UINT32(2, report_count);
  TEST_ASSERT_EQUAL_UINT8(2, keyboard_report_count);
  TEST_ASSERT_EQUAL_UINT8(0, keyboard_reports[1].keycodes[0]);
  TEST_ASSERT_BITS_LOW(1u << 4, keyboard_reports[1].bitmap[0]);
}

void test_hid_replays_release_after_keyboard_recovers(void) {
  keyboard_ready = false;

  hid_keycode_add(KC_A);
  hid_send_reports();
  TEST_ASSERT_EQUAL_UINT32(0, report_count);

  keyboard_ready = true;
  hid_send_reports();
  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT8(1, keyboard_report_count);
  TEST_ASSERT_BITS_HIGH(1u << 4, keyboard_reports[0].bitmap[0]);

  keyboard_ready = false;
  hid_keycode_remove(KC_A);
  hid_send_reports();
  TEST_ASSERT_EQUAL_UINT32(1, report_count);

  keyboard_ready = true;
  hid_send_reports();
  TEST_ASSERT_EQUAL_UINT32(2, report_count);
  TEST_ASSERT_EQUAL_UINT8(2, keyboard_report_count);
  TEST_ASSERT_BITS_LOW(1u << 4, keyboard_reports[1].bitmap[0]);
}

void test_hid_sends_repeated_mouse_motion_reports(void) {
  hid_mouse_move(3, -2, 0);
  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT8(1, mouse_report_count);
  TEST_ASSERT_EQUAL_INT8(3, mouse_reports[0].x);
  TEST_ASSERT_EQUAL_INT8(-2, mouse_reports[0].y);
  TEST_ASSERT_EQUAL_INT8(0, mouse_reports[0].wheel);
  TEST_ASSERT_EQUAL_INT8(0, mouse_reports[0].pan);

  hid_mouse_move(3, -2, 0);
  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(2, report_count);
  TEST_ASSERT_EQUAL_UINT8(2, mouse_report_count);
  TEST_ASSERT_EQUAL_INT8(3, mouse_reports[1].x);
  TEST_ASSERT_EQUAL_INT8(-2, mouse_reports[1].y);
}

void test_hid_accumulates_mouse_motion_while_interface_busy(void) {
  mouse_ready = false;

  hid_mouse_move(4, -1, 0);
  hid_send_reports();
  hid_mouse_move(5, -2, 0);
  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(0, report_count);

  mouse_ready = true;
  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT8(1, mouse_report_count);
  TEST_ASSERT_EQUAL_INT8(9, mouse_reports[0].x);
  TEST_ASSERT_EQUAL_INT8(-3, mouse_reports[0].y);
}

void test_hid_accumulates_mouse_scroll_while_interface_busy(void) {
  mouse_ready = false;

  hid_mouse_scroll(1, 2, 0);
  hid_send_reports();
  hid_mouse_scroll(-3, 1, 0);
  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(0, report_count);

  mouse_ready = true;
  hid_send_reports();

  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT8(1, mouse_report_count);
  TEST_ASSERT_EQUAL_INT8(0, mouse_reports[0].x);
  TEST_ASSERT_EQUAL_INT8(0, mouse_reports[0].y);
  TEST_ASSERT_EQUAL_INT8(-2, mouse_reports[0].wheel);
  TEST_ASSERT_EQUAL_INT8(3, mouse_reports[0].pan);
}

#if defined(USBMON_DIAGNOSTIC_RAW_HID_STREAM)
void test_hid_usbmon_diagnostic_stream_chains_raw_hid_reports(void) {
  uint8_t control_packet[RAW_HID_EP_SIZE] = {0};
  memcpy(control_packet, "UMON", 4);
  control_packet[4] = 1;

  raw_hid_ready = true;
  tud_hid_set_report_cb(USB_ITF_RAW_HID, 0, HID_REPORT_TYPE_OUTPUT,
                        control_packet, sizeof(control_packet));

  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT32(0, command_enqueue_count);
  TEST_ASSERT_EQUAL_UINT8(USB_ITF_RAW_HID, last_instance);
  TEST_ASSERT_EQUAL_UINT16(RAW_HID_EP_SIZE, last_report_len);
  TEST_ASSERT_EQUAL_UINT8(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT8('U', raw_hid_reports[0][0]);
  TEST_ASSERT_EQUAL_UINT8('M', raw_hid_reports[0][1]);
  TEST_ASSERT_EQUAL_UINT8('O', raw_hid_reports[0][2]);
  TEST_ASSERT_EQUAL_UINT8('N', raw_hid_reports[0][3]);

  uint32_t sequence = UINT32_MAX;
  uint32_t completion_cycles = UINT32_MAX;
  uint32_t rearm_cycles = UINT32_MAX;
  uint32_t send_interval_cycles = UINT32_MAX;
  memcpy(&sequence, &raw_hid_reports[0][4], sizeof(sequence));
  TEST_ASSERT_EQUAL_UINT32(0, sequence);
  memcpy(&completion_cycles, &raw_hid_reports[0][12], sizeof(completion_cycles));
  memcpy(&rearm_cycles, &raw_hid_reports[0][16], sizeof(rearm_cycles));
  memcpy(&send_interval_cycles, &raw_hid_reports[0][20], sizeof(send_interval_cycles));
  TEST_ASSERT_EQUAL_UINT32(0, completion_cycles);
  TEST_ASSERT_EQUAL_UINT32(0, rearm_cycles);
  TEST_ASSERT_EQUAL_UINT32(0, send_interval_cycles);

  tud_hid_report_complete_cb(USB_ITF_RAW_HID, raw_hid_reports[0], RAW_HID_EP_SIZE);

  TEST_ASSERT_EQUAL_UINT32(2, report_count);
  TEST_ASSERT_EQUAL_UINT8(2, raw_hid_report_count);
  memcpy(&sequence, &raw_hid_reports[1][4], sizeof(sequence));
  TEST_ASSERT_EQUAL_UINT32(1, sequence);
  memcpy(&completion_cycles, &raw_hid_reports[1][12], sizeof(completion_cycles));
  memcpy(&rearm_cycles, &raw_hid_reports[1][16], sizeof(rearm_cycles));
  memcpy(&send_interval_cycles, &raw_hid_reports[1][20], sizeof(send_interval_cycles));
  TEST_ASSERT_EQUAL_UINT32(100, completion_cycles);
  TEST_ASSERT_EQUAL_UINT32(100, rearm_cycles);
  TEST_ASSERT_EQUAL_UINT32(200, send_interval_cycles);
}

void test_hid_usbmon_diagnostic_stream_stops_for_regular_commands(void) {
  uint8_t control_packet[RAW_HID_EP_SIZE] = {0};
  uint8_t command_packet[RAW_HID_EP_SIZE] = {0};
  memcpy(control_packet, "UMON", 4);
  control_packet[4] = 1;
  command_packet[0] = 0x42;

  raw_hid_ready = true;
  tud_hid_set_report_cb(USB_ITF_RAW_HID, 0, HID_REPORT_TYPE_OUTPUT,
                        control_packet, sizeof(control_packet));
  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT32(0, command_enqueue_count);

  tud_hid_set_report_cb(USB_ITF_RAW_HID, 0, HID_REPORT_TYPE_OUTPUT,
                        command_packet, sizeof(command_packet));
  tud_hid_report_complete_cb(USB_ITF_RAW_HID, raw_hid_reports[0], RAW_HID_EP_SIZE);

  TEST_ASSERT_EQUAL_UINT32(1, report_count);
  TEST_ASSERT_EQUAL_UINT8(1, raw_hid_report_count);
  TEST_ASSERT_EQUAL_UINT32(1, command_enqueue_count);
  TEST_ASSERT_EQUAL_UINT16(RAW_HID_EP_SIZE, last_command_packet_len);
  TEST_ASSERT_EQUAL_UINT8(0x42, last_command_packet[0]);
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hid_send_reports_is_non_blocking_per_interface);
  RUN_TEST(test_hid_preserves_transient_keyboard_taps_while_interface_busy);
  RUN_TEST(test_hid_replays_release_after_keyboard_recovers);
  RUN_TEST(test_hid_sends_repeated_mouse_motion_reports);
  RUN_TEST(test_hid_accumulates_mouse_motion_while_interface_busy);
  RUN_TEST(test_hid_accumulates_mouse_scroll_while_interface_busy);
#if defined(USBMON_DIAGNOSTIC_RAW_HID_STREAM)
  RUN_TEST(test_hid_usbmon_diagnostic_stream_chains_raw_hid_reports);
  RUN_TEST(test_hid_usbmon_diagnostic_stream_stops_for_regular_commands);
#endif
  return UNITY_END();
}
