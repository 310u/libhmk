#include <unity.h>
#include "advanced_keys.h"
#include "deferred_actions.h"
#include "eeconfig.h"
#include "input_routing.h"
#include "keycodes.h"
#include "matrix.h"
#include "layout.h"

// --- Mocks ---
key_state_t key_matrix[NUM_KEYS];
eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;
uint32_t mock_timer = 0;
static uint8_t last_registered_key;
static uint8_t last_registered_keycode;
static uint8_t last_unregistered_key;
static uint8_t last_unregistered_keycode;
static uint8_t layout_event_keys[8];
static uint8_t layout_event_keycodes[8];
static bool layout_event_pressed[8];
static uint8_t layout_event_count;
static uint8_t processed_keys[8];
static bool processed_pressed[8];
static uint8_t processed_count;
static deferred_action_t pushed_actions[8];
static uint8_t pushed_action_count;
static uint8_t rt_keys[8];
static bool rt_disabled[8];
static uint8_t rt_call_count;

void matrix_disable_rapid_trigger(uint8_t key, bool disable) {
    if (rt_call_count < 8) {
        rt_keys[rt_call_count] = key;
        rt_disabled[rt_call_count] = disable;
        rt_call_count++;
    }
}
uint32_t timer_read(void) { return mock_timer; }
uint32_t timer_elapsed(uint32_t last) { return mock_timer - last; }
// --- Tests ---
void setUp(void) {
    memset(&mock_eeconfig, 0, sizeof(eeconfig_t));
    memset(key_matrix, 0, sizeof(key_matrix));
    memset(processed_keys, 0, sizeof(processed_keys));
    memset(processed_pressed, 0, sizeof(processed_pressed));
    mock_timer = 0;
    last_registered_key = 0xFF;
    last_registered_keycode = 0xFF;
    last_unregistered_key = 0xFF;
    last_unregistered_keycode = 0xFF;
    memset(layout_event_keys, 0, sizeof(layout_event_keys));
    memset(layout_event_keycodes, 0, sizeof(layout_event_keycodes));
    memset(layout_event_pressed, 0, sizeof(layout_event_pressed));
    layout_event_count = 0;
    processed_count = 0;
    memset(pushed_actions, 0, sizeof(pushed_actions));
    pushed_action_count = 0;
    memset(rt_keys, 0, sizeof(rt_keys));
    memset(rt_disabled, 0, sizeof(rt_disabled));
    rt_call_count = 0;
    advanced_key_clear();
}

void tearDown(void) {
}

void layout_register(uint8_t key, uint8_t keycode) {
    last_registered_key = key;
    last_registered_keycode = keycode;
    if (layout_event_count < 8) {
        layout_event_keys[layout_event_count] = key;
        layout_event_keycodes[layout_event_count] = keycode;
        layout_event_pressed[layout_event_count] = true;
        layout_event_count++;
    }
}
void layout_unregister(uint8_t key, uint8_t keycode) {
    last_unregistered_key = key;
    last_unregistered_keycode = keycode;
    if (layout_event_count < 8) {
        layout_event_keys[layout_event_count] = key;
        layout_event_keycodes[layout_event_count] = keycode;
        layout_event_pressed[layout_event_count] = false;
        layout_event_count++;
    }
}
bool deferred_action_push(const deferred_action_t *action) {
    if (pushed_action_count < 8)
        pushed_actions[pushed_action_count] = *action;
    pushed_action_count++;
    return true;
}
uint8_t layout_get_current_layer(void) { return 0; }
bool layout_process_key(uint8_t key, bool pressed) {
    if (processed_count < 8) {
        processed_keys[processed_count] = key;
        processed_pressed[processed_count] = pressed;
        processed_count++;
    }
    return true;
}

void test_advanced_keys_combo(void) {
    // Setup a Combo mapped to layer 0, keys 1 and 2, outputting 0x04 (KC_A)
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_COMBO;
    mock_eeconfig.profiles[0].advanced_keys[0].layer = 0;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[0] = 1;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[1] = 2;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[2] = 255;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[3] = 255;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.term = 50;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.output_keycode = 0x04;
    
    advanced_key_combo_invalidate_cache();
    
    // Press key 1
    bool consumed1 = advanced_key_combo_process(1, true, 100);
    // Press key 2 within combo term (50ms)
    bool consumed2 = advanced_key_combo_process(2, true, 110);
    
    advanced_key_combo_task();
    
    TEST_ASSERT_TRUE(consumed1);
    TEST_ASSERT_TRUE(consumed2);
    TEST_ASSERT_EQUAL_UINT8(INPUT_ROUTING_VIRTUAL_KEY, last_registered_key);
    TEST_ASSERT_EQUAL_UINT8(0x04, last_registered_keycode);
}

void test_advanced_keys_combo_release_before_match_flushes_press_and_release(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_COMBO;
    mock_eeconfig.profiles[0].advanced_keys[0].layer = 0;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[0] = 1;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[1] = 2;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[2] = 255;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[3] = 255;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.term = 50;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.output_keycode = 0x04;

    advanced_key_combo_invalidate_cache();

    bool consumed_press = advanced_key_combo_process(1, true, 100);
    bool consumed_release = advanced_key_combo_process(1, false, 120);

    TEST_ASSERT_TRUE(consumed_press);
    TEST_ASSERT_TRUE(consumed_release);
    TEST_ASSERT_EQUAL_UINT8(0xFF, last_registered_key);
    TEST_ASSERT_EQUAL_UINT8(0xFF, last_registered_keycode);
    TEST_ASSERT_EQUAL_UINT8(2, processed_count);
    TEST_ASSERT_EQUAL_UINT8(1, processed_keys[0]);
    TEST_ASSERT_TRUE(processed_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(1, processed_keys[1]);
    TEST_ASSERT_FALSE(processed_pressed[1]);
}

void test_advanced_keys_init(void) {
    advanced_key_init();
    TEST_ASSERT_TRUE(1);
}

void test_advanced_keys_null_bind_primary_transfers_to_secondary_on_release(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_NULL_BIND;
    mock_eeconfig.profiles[0].advanced_keys[0].key = 1;
    mock_eeconfig.profiles[0].advanced_keys[0].null_bind.secondary_key = 2;
    mock_eeconfig.profiles[0].advanced_keys[0].null_bind.behavior = NB_BEHAVIOR_PRIMARY;

    advanced_key_event_t primary_press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 1,
        .keycode = KC_A,
        .ak_index = 0,
    };
    advanced_key_event_t secondary_press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 2,
        .keycode = KC_B,
        .ak_index = 0,
    };
    advanced_key_event_t primary_release = {
        .type = AK_EVENT_TYPE_RELEASE,
        .key = 1,
        .keycode = KC_A,
        .ak_index = 0,
    };

    advanced_key_process(&primary_press);
    advanced_key_process(&secondary_press);
    advanced_key_process(&primary_release);

    TEST_ASSERT_EQUAL_UINT8(3, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(1, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_A, layout_event_keycodes[0]);
    TEST_ASSERT_FALSE(layout_event_pressed[1]);
    TEST_ASSERT_EQUAL_UINT8(1, layout_event_keys[1]);
    TEST_ASSERT_EQUAL_UINT8(KC_A, layout_event_keycodes[1]);
    TEST_ASSERT_TRUE(layout_event_pressed[2]);
    TEST_ASSERT_EQUAL_UINT8(2, layout_event_keys[2]);
    TEST_ASSERT_EQUAL_UINT8(KC_B, layout_event_keycodes[2]);
}

void test_advanced_keys_null_bind_distance_prefers_farther_press(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_NULL_BIND;
    mock_eeconfig.profiles[0].advanced_keys[0].key = 1;
    mock_eeconfig.profiles[0].advanced_keys[0].null_bind.secondary_key = 2;
    mock_eeconfig.profiles[0].advanced_keys[0].null_bind.behavior = NB_BEHAVIOR_DISTANCE;
    key_matrix[1].distance = 40;
    key_matrix[2].distance = 80;

    advanced_key_event_t primary_press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 1,
        .keycode = KC_C,
        .ak_index = 0,
    };
    advanced_key_event_t secondary_press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 2,
        .keycode = KC_D,
        .ak_index = 0,
    };

    advanced_key_process(&primary_press);
    advanced_key_process(&secondary_press);

    TEST_ASSERT_EQUAL_UINT8(3, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(1, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_C, layout_event_keycodes[0]);
    TEST_ASSERT_FALSE(layout_event_pressed[1]);
    TEST_ASSERT_EQUAL_UINT8(1, layout_event_keys[1]);
    TEST_ASSERT_EQUAL_UINT8(KC_C, layout_event_keycodes[1]);
    TEST_ASSERT_TRUE(layout_event_pressed[2]);
    TEST_ASSERT_EQUAL_UINT8(2, layout_event_keys[2]);
    TEST_ASSERT_EQUAL_UINT8(KC_D, layout_event_keycodes[2]);
}

void test_advanced_keys_toggle_second_press_turns_key_off_on_release(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_TOGGLE;
    mock_eeconfig.profiles[0].advanced_keys[0].toggle.keycode = KC_E;
    mock_eeconfig.profiles[0].advanced_keys[0].toggle.tapping_term = 100;

    advanced_key_event_t press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 4,
        .ak_index = 0,
    };
    advanced_key_event_t release = {
        .type = AK_EVENT_TYPE_RELEASE,
        .key = 4,
        .ak_index = 0,
    };

    advanced_key_process(&press);
    advanced_key_process(&release);
    advanced_key_process(&press);
    advanced_key_process(&release);

    TEST_ASSERT_EQUAL_UINT8(3, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(4, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_E, layout_event_keycodes[0]);
    TEST_ASSERT_TRUE(layout_event_pressed[1]);
    TEST_ASSERT_EQUAL_UINT8(4, layout_event_keys[1]);
    TEST_ASSERT_EQUAL_UINT8(KC_E, layout_event_keycodes[1]);
    TEST_ASSERT_FALSE(layout_event_pressed[2]);
    TEST_ASSERT_EQUAL_UINT8(4, layout_event_keys[2]);
    TEST_ASSERT_EQUAL_UINT8(KC_E, layout_event_keycodes[2]);
}

void test_advanced_keys_toggle_hold_past_tapping_term_falls_back_to_momentary(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_TOGGLE;
    mock_eeconfig.profiles[0].advanced_keys[0].toggle.keycode = KC_F;
    mock_eeconfig.profiles[0].advanced_keys[0].toggle.tapping_term = 100;

    advanced_key_event_t press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 5,
        .ak_index = 0,
    };
    advanced_key_event_t release = {
        .type = AK_EVENT_TYPE_RELEASE,
        .key = 5,
        .ak_index = 0,
    };

    advanced_key_process(&press);
    mock_timer = 100;
    advanced_key_tick(false, false);
    advanced_key_process(&release);

    TEST_ASSERT_EQUAL_UINT8(2, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(5, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_F, layout_event_keycodes[0]);
    TEST_ASSERT_FALSE(layout_event_pressed[1]);
    TEST_ASSERT_EQUAL_UINT8(5, layout_event_keys[1]);
    TEST_ASSERT_EQUAL_UINT8(KC_F, layout_event_keycodes[1]);
}

void test_advanced_keys_dynamic_keystroke_press_enqueues_press_and_disables_rt(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_DYNAMIC_KEYSTROKE;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.keycodes[0] = KC_A;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.bitmap[0] =
        DKS_ACTION_PRESS;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.bottom_out_point = 1;

    advanced_key_event_t event = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 3,
        .ak_index = 0,
    };

    advanced_key_process(&event);

    TEST_ASSERT_EQUAL_UINT8(1, pushed_action_count);
    TEST_ASSERT_EQUAL_UINT8(DEFERRED_ACTION_TYPE_PRESS, pushed_actions[0].type);
    TEST_ASSERT_EQUAL_UINT8(3, pushed_actions[0].key);
    TEST_ASSERT_EQUAL_UINT8(KC_A, pushed_actions[0].keycode);
    TEST_ASSERT_EQUAL_UINT8(1, rt_call_count);
    TEST_ASSERT_EQUAL_UINT8(3, rt_keys[0]);
    TEST_ASSERT_TRUE(rt_disabled[0]);
}

void test_advanced_keys_dynamic_keystroke_bottom_out_uses_bottom_out_action(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_DYNAMIC_KEYSTROKE;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.keycodes[0] = KC_B;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.bitmap[0] =
        (uint8_t)(DKS_ACTION_TAP << 2);
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.bottom_out_point = 50;
    key_matrix[4].distance = 60;

    advanced_key_event_t event = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 4,
        .ak_index = 0,
    };

    advanced_key_process(&event);

    TEST_ASSERT_EQUAL_UINT8(1, pushed_action_count);
    TEST_ASSERT_EQUAL_UINT8(DEFERRED_ACTION_TYPE_TAP, pushed_actions[0].type);
    TEST_ASSERT_EQUAL_UINT8(4, pushed_actions[0].key);
    TEST_ASSERT_EQUAL_UINT8(KC_B, pushed_actions[0].keycode);
}

void test_advanced_keys_dynamic_keystroke_release_action_unregisters_pressed_key(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_DYNAMIC_KEYSTROKE;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.keycodes[0] = KC_C;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.bitmap[0] =
        (uint8_t)(DKS_ACTION_PRESS | (DKS_ACTION_RELEASE << 6));
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.bottom_out_point = 1;

    advanced_key_event_t press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 2,
        .ak_index = 0,
    };
    advanced_key_event_t release = {
        .type = AK_EVENT_TYPE_RELEASE,
        .key = 2,
        .ak_index = 0,
    };

    advanced_key_process(&press);
    advanced_key_process(&release);

    TEST_ASSERT_EQUAL_UINT8(1, layout_event_count);
    TEST_ASSERT_FALSE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(2, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_C, layout_event_keycodes[0]);
}

void test_advanced_keys_clear_re_enables_dynamic_keystroke_rapid_trigger(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_DYNAMIC_KEYSTROKE;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.keycodes[0] = KC_D;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.bitmap[0] =
        DKS_ACTION_PRESS;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.bottom_out_point = 1;

    advanced_key_event_t event = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 3,
        .ak_index = 0,
    };

    advanced_key_process(&event);
    advanced_key_clear();

    TEST_ASSERT_EQUAL_UINT8(2, rt_call_count);
    TEST_ASSERT_EQUAL_UINT8(3, rt_keys[0]);
    TEST_ASSERT_TRUE(rt_disabled[0]);
    TEST_ASSERT_EQUAL_UINT8(3, rt_keys[1]);
    TEST_ASSERT_FALSE(rt_disabled[1]);
}

void test_advanced_keys_clear_drops_buffered_combo_events(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_COMBO;
    mock_eeconfig.profiles[0].advanced_keys[0].layer = 0;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[0] = 1;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[1] = 2;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[2] = 255;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.keys[3] = 255;
    mock_eeconfig.profiles[0].advanced_keys[0].combo.term = 50;

    TEST_ASSERT_TRUE(advanced_key_combo_process(1, true, 100));

    advanced_key_clear();

    TEST_ASSERT_FALSE(advanced_key_combo_task());
    TEST_ASSERT_EQUAL_UINT8(0, processed_count);
}

void test_advanced_keys_macro_tap_presses_and_releases_virtual_key(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_MACRO;
    mock_eeconfig.profiles[0].advanced_keys[0].macro_key.macro_index = 0;
    mock_eeconfig.profiles[0].macros[0].events[0] =
        (macro_event_t){.keycode = KC_A, .action = MACRO_ACTION_TAP};
    mock_eeconfig.profiles[0].macros[0].events[1] =
        (macro_event_t){.keycode = KC_NO, .action = MACRO_ACTION_END};

    advanced_key_event_t event = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 4,
        .ak_index = 0,
    };

    advanced_key_process(&event);
    advanced_key_tick(false, false);

    TEST_ASSERT_EQUAL_UINT8(1, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(INPUT_ROUTING_VIRTUAL_KEY, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_A, layout_event_keycodes[0]);

    mock_timer = 30;
    advanced_key_tick(false, false);

    TEST_ASSERT_EQUAL_UINT8(2, layout_event_count);
    TEST_ASSERT_FALSE(layout_event_pressed[1]);
    TEST_ASSERT_EQUAL_UINT8(INPUT_ROUTING_VIRTUAL_KEY, layout_event_keys[1]);
    TEST_ASSERT_EQUAL_UINT8(KC_A, layout_event_keycodes[1]);
    TEST_ASSERT_EQUAL_UINT8(INPUT_ROUTING_VIRTUAL_KEY, last_unregistered_key);
    TEST_ASSERT_EQUAL_UINT8(KC_A, last_unregistered_keycode);
}

void test_advanced_keys_abort_macros_releases_tapping_key(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_MACRO;
    mock_eeconfig.profiles[0].advanced_keys[0].macro_key.macro_index = 0;
    mock_eeconfig.profiles[0].macros[0].events[0] =
        (macro_event_t){.keycode = KC_B, .action = MACRO_ACTION_TAP};
    mock_eeconfig.profiles[0].macros[0].events[1] =
        (macro_event_t){.keycode = KC_NO, .action = MACRO_ACTION_END};

    advanced_key_event_t event = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 5,
        .ak_index = 0,
    };

    advanced_key_process(&event);
    advanced_key_tick(false, false);
    advanced_key_abort_macros();

    TEST_ASSERT_EQUAL_UINT8(2, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_FALSE(layout_event_pressed[1]);
    TEST_ASSERT_EQUAL_UINT8(INPUT_ROUTING_VIRTUAL_KEY, layout_event_keys[1]);
    TEST_ASSERT_EQUAL_UINT8(KC_B, layout_event_keycodes[1]);
}

void test_advanced_keys_tap_hold_hold_registers_and_releases_hold_key(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_TAP_HOLD;
    mock_eeconfig.profiles[0].advanced_keys[0].key = 6;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.tap_keycode = KC_A;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.hold_keycode = KC_B;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.tapping_term = 100;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.flags =
        TH_MAKE_FLAGS(TAP_HOLD_FLAVOR_HOLD_PREFERRED, false, false);

    advanced_key_event_t press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 6,
        .ak_index = 0,
    };
    advanced_key_event_t release = {
        .type = AK_EVENT_TYPE_RELEASE,
        .key = 6,
        .ak_index = 0,
    };

    advanced_key_process(&press);
    mock_timer = 100;
    advanced_key_tick(false, false);

    TEST_ASSERT_EQUAL_UINT8(1, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(6, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_B, layout_event_keycodes[0]);

    advanced_key_process(&release);

    TEST_ASSERT_EQUAL_UINT8(2, layout_event_count);
    TEST_ASSERT_FALSE(layout_event_pressed[1]);
    TEST_ASSERT_EQUAL_UINT8(6, layout_event_keys[1]);
    TEST_ASSERT_EQUAL_UINT8(KC_B, layout_event_keycodes[1]);
}

void test_advanced_keys_tap_hold_hwu_tap_unregisters_hold_then_registers_tap(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_TAP_HOLD;
    mock_eeconfig.profiles[0].advanced_keys[0].key = 7;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.tap_keycode = KC_C;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.hold_keycode = KC_D;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.tapping_term = 100;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.flags =
        TH_MAKE_FLAGS(TAP_HOLD_FLAVOR_TAP_PREFERRED, false, true);

    advanced_key_event_t press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 7,
        .ak_index = 0,
    };
    advanced_key_event_t release = {
        .type = AK_EVENT_TYPE_RELEASE,
        .key = 7,
        .ak_index = 0,
    };

    advanced_key_process(&press);
    advanced_key_process(&release);

    TEST_ASSERT_EQUAL_UINT8(3, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(7, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_D, layout_event_keycodes[0]);
    TEST_ASSERT_FALSE(layout_event_pressed[1]);
    TEST_ASSERT_EQUAL_UINT8(7, layout_event_keys[1]);
    TEST_ASSERT_EQUAL_UINT8(KC_D, layout_event_keycodes[1]);
    TEST_ASSERT_TRUE(layout_event_pressed[2]);
    TEST_ASSERT_EQUAL_UINT8(7, layout_event_keys[2]);
    TEST_ASSERT_EQUAL_UINT8(KC_C, layout_event_keycodes[2]);
}

void test_advanced_keys_tap_hold_double_tap_wait_timeout_emits_tap(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_TAP_HOLD;
    mock_eeconfig.profiles[0].advanced_keys[0].key = 8;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.tap_keycode = KC_E;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.hold_keycode = KC_F;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.double_tap_keycode = KC_G;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.tapping_term = 100;
    mock_eeconfig.profiles[0].advanced_keys[0].tap_hold.flags =
        TH_MAKE_FLAGS(TAP_HOLD_FLAVOR_TAP_PREFERRED, false, false);

    advanced_key_event_t press = {
        .type = AK_EVENT_TYPE_PRESS,
        .key = 8,
        .ak_index = 0,
    };
    advanced_key_event_t release = {
        .type = AK_EVENT_TYPE_RELEASE,
        .key = 8,
        .ak_index = 0,
    };

    advanced_key_process(&press);
    mock_timer = 10;
    advanced_key_process(&release);

    TEST_ASSERT_EQUAL_UINT8(0, layout_event_count);

    mock_timer = 110;
    advanced_key_tick(false, false);

    TEST_ASSERT_EQUAL_UINT8(1, layout_event_count);
    TEST_ASSERT_TRUE(layout_event_pressed[0]);
    TEST_ASSERT_EQUAL_UINT8(8, layout_event_keys[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_E, layout_event_keycodes[0]);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_advanced_keys_init);
    RUN_TEST(test_advanced_keys_combo);
    RUN_TEST(test_advanced_keys_combo_release_before_match_flushes_press_and_release);
    RUN_TEST(test_advanced_keys_null_bind_primary_transfers_to_secondary_on_release);
    RUN_TEST(test_advanced_keys_null_bind_distance_prefers_farther_press);
    RUN_TEST(test_advanced_keys_toggle_second_press_turns_key_off_on_release);
    RUN_TEST(test_advanced_keys_toggle_hold_past_tapping_term_falls_back_to_momentary);
    RUN_TEST(test_advanced_keys_dynamic_keystroke_press_enqueues_press_and_disables_rt);
    RUN_TEST(test_advanced_keys_dynamic_keystroke_bottom_out_uses_bottom_out_action);
    RUN_TEST(test_advanced_keys_dynamic_keystroke_release_action_unregisters_pressed_key);
    RUN_TEST(test_advanced_keys_clear_re_enables_dynamic_keystroke_rapid_trigger);
    RUN_TEST(test_advanced_keys_clear_drops_buffered_combo_events);
    RUN_TEST(test_advanced_keys_macro_tap_presses_and_releases_virtual_key);
    RUN_TEST(test_advanced_keys_abort_macros_releases_tapping_key);
    RUN_TEST(test_advanced_keys_tap_hold_hold_registers_and_releases_hold_key);
    RUN_TEST(test_advanced_keys_tap_hold_hwu_tap_unregisters_hold_then_registers_tap);
    RUN_TEST(test_advanced_keys_tap_hold_double_tap_wait_timeout_emits_tap);
    UNITY_END();
    return 0;
}
