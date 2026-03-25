#include <unity.h>

#include "advanced_keys.h"
#include "deferred_actions.h"
#include "eeconfig.h"
#include "hid.h"
#include "keycodes.h"
#include "layout.h"
#include "matrix.h"

key_state_t key_matrix[NUM_KEYS];
eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;

static uint32_t mock_timer;
static uint8_t hid_added[16];
static uint8_t hid_removed[16];
static uint8_t hid_add_count;
static uint8_t hid_remove_count;

static void reset_hid_log(void) {
  memset(hid_added, 0, sizeof(hid_added));
  memset(hid_removed, 0, sizeof(hid_removed));
  hid_add_count = 0;
  hid_remove_count = 0;
}

static void prepare_pipeline(void) {
  layout_load_advanced_keys();
  layout_reset_runtime_state();
  reset_hid_log();
}

static void set_key_state(uint8_t key, bool pressed, uint32_t event_time,
                          uint8_t distance) {
  key_matrix[key].is_pressed = pressed;
  key_matrix[key].event_time = event_time;
  key_matrix[key].distance = distance;
}

static void run_layout_at(uint32_t time) {
  mock_timer = time;
  layout_task();
}

void board_enter_bootloader(void) {}
void board_reset(void) {}

void hid_clear_runtime_state(void) {}

void hid_keycode_add(uint8_t keycode) {
  if (hid_add_count < sizeof(hid_added))
    hid_added[hid_add_count++] = keycode;
}

void hid_keycode_remove(uint8_t keycode) {
  if (hid_remove_count < sizeof(hid_removed))
    hid_removed[hid_remove_count++] = keycode;
}

void hid_mouse_move(int8_t x, int8_t y, uint8_t buttons) {}
void hid_mouse_scroll(int8_t wheel, int8_t pan, uint8_t buttons) {}
void hid_send_reports(void) {}

void matrix_disable_rapid_trigger(uint8_t key, bool disable) {}

void profile_runtime_apply_current(void) {}
void profile_runtime_reload_current(void) {}

uint32_t timer_read(void) { return mock_timer; }
uint32_t timer_elapsed(uint32_t last) { return mock_timer - last; }

bool wear_leveling_write(uint32_t address, const void *data, uint32_t len) {
  return true;
}

void xinput_process(uint8_t key) {}
void xinput_reset_runtime_state(void) {}

void setUp(void) {
  memset(&mock_eeconfig, 0, sizeof(mock_eeconfig));
  memset(key_matrix, 0, sizeof(key_matrix));
  mock_timer = 0;
  mock_eeconfig.current_profile = 0;
  mock_eeconfig.profiles[0].gamepad_options.keyboard_enabled = true;
  mock_eeconfig.profiles[0].tick_rate = 1;
  advanced_key_init();
  deferred_action_init();
  prepare_pipeline();
}

void tearDown(void) {}

void test_event_pipeline_sorts_simultaneous_press_order_by_distance(void) {
  mock_eeconfig.profiles[0].keymap[0][1] = KC_B;
  mock_eeconfig.profiles[0].keymap[0][2] = KC_A;
  prepare_pipeline();

  set_key_state(1, true, 10, 120);
  set_key_state(2, true, 10, 200);

  run_layout_at(10);

  TEST_ASSERT_EQUAL_UINT8(2, hid_add_count);
  TEST_ASSERT_EQUAL_UINT8(KC_A, hid_added[0]);
  TEST_ASSERT_EQUAL_UINT8(KC_B, hid_added[1]);
}

void test_event_pipeline_buffers_non_tap_hold_press_until_hold_resolves(void) {
  advanced_key_t *tap_hold = &mock_eeconfig.profiles[0].advanced_keys[0];

  tap_hold->type = AK_TYPE_TAP_HOLD;
  tap_hold->layer = 0;
  tap_hold->key = 0;
  tap_hold->tap_hold.tap_keycode = KC_B;
  tap_hold->tap_hold.hold_keycode = KC_LSFT;
  tap_hold->tap_hold.tapping_term = 100;
  tap_hold->tap_hold.flags =
      TH_MAKE_FLAGS(TAP_HOLD_FLAVOR_HOLD_PREFERRED, false, false);
  mock_eeconfig.profiles[0].keymap[0][1] = KC_A;
  prepare_pipeline();

  set_key_state(0, true, 10, 180);
  run_layout_at(10);
  TEST_ASSERT_EQUAL_UINT8(0, hid_add_count);

  set_key_state(1, true, 20, 170);
  run_layout_at(20);

  TEST_ASSERT_EQUAL_UINT8(2, hid_add_count);
  TEST_ASSERT_EQUAL_UINT8(KC_LSFT, hid_added[0]);
  TEST_ASSERT_EQUAL_UINT8(KC_A, hid_added[1]);
  TEST_ASSERT_EQUAL_UINT8(0, hid_remove_count);
}

void test_event_pipeline_keeps_pending_press_and_release_paired(void) {
  advanced_key_t *tap_hold = &mock_eeconfig.profiles[0].advanced_keys[0];

  tap_hold->type = AK_TYPE_TAP_HOLD;
  tap_hold->layer = 0;
  tap_hold->key = 0;
  tap_hold->tap_hold.tap_keycode = KC_B;
  tap_hold->tap_hold.hold_keycode = KC_LSFT;
  tap_hold->tap_hold.tapping_term = 100;
  tap_hold->tap_hold.flags =
      TH_MAKE_FLAGS(TAP_HOLD_FLAVOR_TAP_PREFERRED, false, false);
  mock_eeconfig.profiles[0].keymap[0][1] = KC_A;
  prepare_pipeline();

  set_key_state(0, true, 10, 180);
  run_layout_at(10);

  set_key_state(1, true, 20, 170);
  run_layout_at(20);
  TEST_ASSERT_EQUAL_UINT8(0, hid_add_count);
  TEST_ASSERT_EQUAL_UINT8(0, hid_remove_count);

  set_key_state(1, false, 30, 0);
  run_layout_at(30);
  TEST_ASSERT_EQUAL_UINT8(0, hid_add_count);
  TEST_ASSERT_EQUAL_UINT8(0, hid_remove_count);

  set_key_state(0, false, 40, 0);
  run_layout_at(40);

  TEST_ASSERT_EQUAL_UINT8(2, hid_add_count);
  TEST_ASSERT_EQUAL_UINT8(KC_B, hid_added[0]);
  TEST_ASSERT_EQUAL_UINT8(KC_A, hid_added[1]);
  TEST_ASSERT_EQUAL_UINT8(1, hid_remove_count);
  TEST_ASSERT_EQUAL_UINT8(KC_A, hid_removed[0]);

  run_layout_at(41);

  TEST_ASSERT_EQUAL_UINT8(2, hid_remove_count);
  TEST_ASSERT_EQUAL_UINT8(KC_A, hid_removed[0]);
  TEST_ASSERT_EQUAL_UINT8(KC_B, hid_removed[1]);
}

void test_event_pipeline_flushes_unmatched_combo_as_normal_input(void) {
  advanced_key_t *combo = &mock_eeconfig.profiles[0].advanced_keys[0];

  combo->type = AK_TYPE_COMBO;
  combo->layer = 0;
  combo->combo.keys[0] = 1;
  combo->combo.keys[1] = 2;
  combo->combo.keys[2] = 255;
  combo->combo.keys[3] = 255;
  combo->combo.term = 50;
  combo->combo.output_keycode = KC_C;
  mock_eeconfig.profiles[0].keymap[0][1] = KC_A;
  mock_eeconfig.profiles[0].keymap[0][2] = KC_B;
  prepare_pipeline();

  set_key_state(1, true, 10, 180);
  run_layout_at(10);
  TEST_ASSERT_EQUAL_UINT8(0, hid_add_count);

  run_layout_at(70);
  TEST_ASSERT_EQUAL_UINT8(1, hid_add_count);
  TEST_ASSERT_EQUAL_UINT8(KC_A, hid_added[0]);

  set_key_state(1, false, 80, 0);
  run_layout_at(80);

  TEST_ASSERT_EQUAL_UINT8(1, hid_remove_count);
  TEST_ASSERT_EQUAL_UINT8(KC_A, hid_removed[0]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_event_pipeline_sorts_simultaneous_press_order_by_distance);
  RUN_TEST(test_event_pipeline_buffers_non_tap_hold_press_until_hold_resolves);
  RUN_TEST(test_event_pipeline_keeps_pending_press_and_release_paired);
  RUN_TEST(test_event_pipeline_flushes_unmatched_combo_as_normal_input);
  return UNITY_END();
}
