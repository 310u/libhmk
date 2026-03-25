#include <unity.h>

#include "eeconfig.h"
#include "joystick.h"
#include "matrix.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "xinput.h"

key_state_t key_matrix[NUM_KEYS];
eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;
bool is_sniper_active = false;

static joystick_state_t mock_joystick_state;
static joystick_config_t mock_joystick_config;
static bool mock_usb_ready;
static bool mock_hid_ready;
static bool mock_xinput_busy;
static uint8_t hid_report_count;
static hid_gamepad_xbox_report_t hid_reports[8];

joystick_state_t joystick_get_state(void) { return mock_joystick_state; }
joystick_config_t joystick_get_config(void) { return mock_joystick_config; }

bool tud_ready(void) { return mock_usb_ready; }

bool tud_hid_n_ready(uint8_t instance) {
  return instance == USB_ITF_HID && mock_hid_ready;
}

bool tud_hid_n_report(uint8_t instance, uint8_t report_id, const void *report,
                      uint16_t len) {
  if (instance == USB_ITF_HID && report_id == REPORT_ID_GAMEPAD &&
      hid_report_count < 8 && len == sizeof(hid_gamepad_xbox_report_t)) {
    memcpy(&hid_reports[hid_report_count], report, len);
    hid_report_count++;
  }
  return true;
}

bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr) { return mock_xinput_busy; }
bool usbd_open_edpt_pair(uint8_t rhport, void const *desc_ep, uint8_t ep_count,
                         uint8_t xfer_type, uint8_t *ep_out, uint8_t *ep_in) {
  if (ep_out != NULL)
    *ep_out = 0x01;
  if (ep_in != NULL)
    *ep_in = 0x81;
  return true;
}
bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr) { return true; }
bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer,
                    uint16_t total_bytes) {
  return true;
}
bool usbd_edpt_release(uint8_t rhport, uint8_t ep_addr) { return true; }

void setUp(void) {
  memset(&mock_eeconfig, 0, sizeof(mock_eeconfig));
  memset(key_matrix, 0, sizeof(key_matrix));
  memset(&mock_joystick_state, 0, sizeof(mock_joystick_state));
  memset(&mock_joystick_config, 0, sizeof(mock_joystick_config));
  mock_usb_ready = true;
  mock_hid_ready = true;
  mock_xinput_busy = false;
  hid_report_count = 0;
  memset(hid_reports, 0, sizeof(hid_reports));
  xinput_init();
}

void tearDown(void) {}

void test_xinput_hid_gamepad_clears_physical_stick_button_on_release(void) {
  mock_eeconfig.options.xinput_enabled = false;
  mock_joystick_config.mode = JOYSTICK_MODE_XINPUT_RS;

  mock_joystick_state.sw = true;
  xinput_task();

  TEST_ASSERT_EQUAL_UINT8(1, hid_report_count);
  TEST_ASSERT_BITS_HIGH(1u << 9, hid_reports[0].buttons);

  mock_joystick_state.sw = false;
  xinput_task();

  TEST_ASSERT_EQUAL_UINT8(2, hid_report_count);
  TEST_ASSERT_BITS_LOW(1u << 9, hid_reports[1].buttons);
}

void test_xinput_hid_gamepad_preserves_transient_button_tap_while_busy(void) {
  mock_eeconfig.options.xinput_enabled = false;
  mock_eeconfig.profiles[0].gamepad_buttons[1] = GP_BUTTON_A;
  mock_hid_ready = false;

  key_matrix[1].is_pressed = true;
  xinput_process(1);
  xinput_task();

  key_matrix[1].is_pressed = false;
  xinput_process(1);
  xinput_task();

  TEST_ASSERT_EQUAL_UINT8(0, hid_report_count);

  mock_hid_ready = true;
  xinput_task();

  TEST_ASSERT_EQUAL_UINT8(1, hid_report_count);
  TEST_ASSERT_BITS_HIGH(1u << 0, hid_reports[0].buttons);

  xinput_task();

  TEST_ASSERT_EQUAL_UINT8(2, hid_report_count);
  TEST_ASSERT_BITS_LOW(1u << 0, hid_reports[1].buttons);
}

void test_xinput_hid_gamepad_does_not_double_circularize_physical_stick(void) {
  mock_eeconfig.options.xinput_enabled = false;
  mock_joystick_config.mode = JOYSTICK_MODE_XINPUT_RS;
  mock_joystick_state.out_x = 90;
  mock_joystick_state.out_y = 90;

  xinput_task();

  TEST_ASSERT_EQUAL_UINT8(1, hid_report_count);
  TEST_ASSERT_INT8_WITHIN(1, 90, hid_reports[0].rx);
  TEST_ASSERT_INT8_WITHIN(1, 90, hid_reports[0].ry);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_xinput_hid_gamepad_clears_physical_stick_button_on_release);
  RUN_TEST(test_xinput_hid_gamepad_preserves_transient_button_tap_while_busy);
  RUN_TEST(test_xinput_hid_gamepad_does_not_double_circularize_physical_stick);
  return UNITY_END();
}
