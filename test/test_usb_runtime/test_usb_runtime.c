#include <unity.h>

#include "usb_runtime.h"

static uint32_t mock_timer;
static uint32_t hid_runtime_clear_count;
static uint32_t xinput_runtime_clear_count;
static uint32_t usb_disconnect_count;
static uint32_t usb_connect_count;
static bool mock_usb_suspended;

uint32_t timer_read(void) { return mock_timer; }

void hid_clear_runtime_state(void) { hid_runtime_clear_count++; }

void xinput_reset_runtime_state(void) { xinput_runtime_clear_count++; }

bool tud_disconnect(void) {
  usb_disconnect_count++;
  return true;
}

bool tud_connect(void) {
  usb_connect_count++;
  return true;
}

bool tud_suspended(void) { return mock_usb_suspended; }

void setUp(void) {
  mock_timer = 0;
  hid_runtime_clear_count = 0;
  xinput_runtime_clear_count = 0;
  usb_disconnect_count = 0;
  usb_connect_count = 0;
  mock_usb_suspended = false;
  usb_runtime_init();
}

void tearDown(void) {}

void test_usb_runtime_mount_resyncs_state(void) {
  usb_runtime_mount();

  TEST_ASSERT_EQUAL_UINT32(1, hid_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(1, xinput_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_connect_count);
}

void test_usb_runtime_short_suspend_does_not_reconnect(void) {
  usb_runtime_suspend();
  mock_timer = 4999;

  usb_runtime_resume();
  usb_runtime_task();
  mock_timer = 5020;
  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(1, hid_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(1, xinput_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_connect_count);
}

void test_usb_runtime_long_suspend_reconnects_after_delay(void) {
  usb_runtime_suspend();
  mock_timer = 5000;

  usb_runtime_resume();

  TEST_ASSERT_EQUAL_UINT32(0, hid_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(0, xinput_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_connect_count);

  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(1, hid_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(1, xinput_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(1, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_connect_count);

  mock_timer = 5019;
  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(1, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_connect_count);

  mock_timer = 5020;
  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(1, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(1, usb_connect_count);
}

void test_usb_runtime_mount_clears_pending_reconnect(void) {
  usb_runtime_suspend();
  mock_timer = 6000;
  usb_runtime_resume();

  usb_runtime_mount();
  mock_timer = 7000;
  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(1, hid_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(1, xinput_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_connect_count);
}

void test_usb_runtime_task_recovers_during_long_suspend_without_resume_callback(void) {
  usb_runtime_suspend();
  mock_usb_suspended = true;
  mock_timer = 5000;

  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(1, hid_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(1, xinput_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(1, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_connect_count);

  mock_timer = 5019;
  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(1, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(0, usb_connect_count);

  mock_timer = 5020;
  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(1, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(1, usb_connect_count);

  mock_usb_suspended = false;
  usb_runtime_task();

  TEST_ASSERT_EQUAL_UINT32(2, hid_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(2, xinput_runtime_clear_count);
  TEST_ASSERT_EQUAL_UINT32(1, usb_disconnect_count);
  TEST_ASSERT_EQUAL_UINT32(1, usb_connect_count);
}

void test_usb_runtime_reports_suspend_state_from_callbacks(void) {
  TEST_ASSERT_FALSE(usb_runtime_is_suspended());

  usb_runtime_suspend();
  TEST_ASSERT_TRUE(usb_runtime_is_suspended());

  usb_runtime_resume();
  TEST_ASSERT_FALSE(usb_runtime_is_suspended());
}

void test_usb_runtime_reports_suspend_state_from_tinyusb_flag(void) {
  mock_usb_suspended = true;
  TEST_ASSERT_TRUE(usb_runtime_is_suspended());

  mock_usb_suspended = false;
  TEST_ASSERT_FALSE(usb_runtime_is_suspended());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_usb_runtime_mount_resyncs_state);
  RUN_TEST(test_usb_runtime_short_suspend_does_not_reconnect);
  RUN_TEST(test_usb_runtime_long_suspend_reconnects_after_delay);
  RUN_TEST(test_usb_runtime_mount_clears_pending_reconnect);
  RUN_TEST(test_usb_runtime_task_recovers_during_long_suspend_without_resume_callback);
  RUN_TEST(test_usb_runtime_reports_suspend_state_from_callbacks);
  RUN_TEST(test_usb_runtime_reports_suspend_state_from_tinyusb_flag);
  return UNITY_END();
}
