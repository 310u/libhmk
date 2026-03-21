#include <unity.h>

#include "deferred_actions.h"
#include "eeconfig.h"
#include "input_routing.h"
#include "keycodes.h"

typedef struct {
  bool pressed;
  uint8_t key;
  uint8_t keycode;
} layout_event_t;

eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;
static layout_event_t events[8];
static uint8_t event_count;

void layout_register(uint8_t key, uint8_t keycode) {
  if (event_count < 8) {
    events[event_count++] = (layout_event_t){
        .pressed = true,
        .key = key,
        .keycode = keycode,
    };
  }
}

void layout_unregister(uint8_t key, uint8_t keycode) {
  if (event_count < 8) {
    events[event_count++] = (layout_event_t){
        .pressed = false,
        .key = key,
        .keycode = keycode,
    };
  }
}

void setUp(void) {
  memset(&mock_eeconfig, 0, sizeof(mock_eeconfig));
  memset(events, 0, sizeof(events));
  event_count = 0;
  deferred_action_clear();
}

void tearDown(void) {}

void test_deferred_action_process_press_and_release_real_key(void) {
  deferred_action_t press = {
      .type = DEFERRED_ACTION_TYPE_PRESS,
      .key = 3,
      .keycode = KC_A,
  };
  deferred_action_t release = {
      .type = DEFERRED_ACTION_TYPE_RELEASE,
      .key = 3,
      .keycode = KC_A,
  };

  TEST_ASSERT_TRUE(deferred_action_push(&press));
  deferred_action_process();
  TEST_ASSERT_EQUAL_UINT8(1, event_count);
  TEST_ASSERT_TRUE(events[0].pressed);
  TEST_ASSERT_EQUAL_UINT8(3, events[0].key);
  TEST_ASSERT_EQUAL_UINT8(KC_A, events[0].keycode);

  TEST_ASSERT_TRUE(deferred_action_push(&release));
  deferred_action_process();
  TEST_ASSERT_EQUAL_UINT8(2, event_count);
  TEST_ASSERT_FALSE(events[1].pressed);
  TEST_ASSERT_EQUAL_UINT8(3, events[1].key);
  TEST_ASSERT_EQUAL_UINT8(KC_A, events[1].keycode);
}

void test_deferred_action_process_virtual_tap_uses_virtual_key(void) {
  deferred_action_t tap = {
      .type = DEFERRED_ACTION_TYPE_TAP,
      .key = INPUT_ROUTING_VIRTUAL_KEY,
      .keycode = KC_B,
  };

  TEST_ASSERT_TRUE(deferred_action_push(&tap));

  deferred_action_process();
  TEST_ASSERT_EQUAL_UINT8(1, event_count);
  TEST_ASSERT_TRUE(events[0].pressed);
  TEST_ASSERT_EQUAL_UINT8(INPUT_ROUTING_VIRTUAL_KEY, events[0].key);
  TEST_ASSERT_EQUAL_UINT8(KC_B, events[0].keycode);

  deferred_action_process();
  TEST_ASSERT_EQUAL_UINT8(2, event_count);
  TEST_ASSERT_FALSE(events[1].pressed);
  TEST_ASSERT_EQUAL_UINT8(INPUT_ROUTING_VIRTUAL_KEY, events[1].key);
  TEST_ASSERT_EQUAL_UINT8(KC_B, events[1].keycode);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_deferred_action_process_press_and_release_real_key);
  RUN_TEST(test_deferred_action_process_virtual_tap_uses_virtual_key);
  return UNITY_END();
}
