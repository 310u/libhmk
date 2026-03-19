#include <unity.h>
#include "advanced_keys.h"
#include "deferred_actions.h"
#include "eeconfig.h"
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
static uint8_t processed_keys[8];
static bool processed_pressed[8];
static uint8_t processed_count;
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
bool deferred_action_push(const deferred_action_t *action) { return true; }
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
    processed_count = 0;
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
}
void layout_unregister(uint8_t key, uint8_t keycode) {}
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
    TEST_ASSERT_EQUAL_UINT8(255, last_registered_key); // Combos register on dummy key 255
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

void test_advanced_keys_clear_re_enables_dynamic_keystroke_rapid_trigger(void) {
    mock_eeconfig.profiles[0].advanced_keys[0].type = AK_TYPE_DYNAMIC_KEYSTROKE;
    mock_eeconfig.profiles[0].advanced_keys[0].dynamic_keystroke.keycodes[0] = KC_A;

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

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_advanced_keys_init);
    RUN_TEST(test_advanced_keys_combo);
    RUN_TEST(test_advanced_keys_combo_release_before_match_flushes_press_and_release);
    RUN_TEST(test_advanced_keys_clear_re_enables_dynamic_keystroke_rapid_trigger);
    RUN_TEST(test_advanced_keys_clear_drops_buffered_combo_events);
    UNITY_END();
    return 0;
}
