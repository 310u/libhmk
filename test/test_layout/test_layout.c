#include <unity.h>
#include "layout.h"
#include "advanced_keys.h"
#include "deferred_actions.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "input_routing.h"
#include "keycodes.h"
#include "lib/bitmap.h"
#include "matrix.h"
#include "xinput.h"
#if defined(JOYSTICK_ENABLED)
#include "joystick.h"
#endif

// --- Mocks ---
key_state_t key_matrix[NUM_KEYS];
eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;
uint32_t mock_timer = 0;
uint32_t board_reset_count = 0;
uint32_t wear_leveling_write_count = 0;
uint32_t last_write_address = 0;
uint32_t last_write_len = 0;
uint32_t last_write_u32 = 0;
uint32_t rgb_apply_config_count = 0;
uint32_t deferred_action_clear_count = 0;
uint32_t hid_clear_runtime_state_count = 0;
uint32_t xinput_reset_runtime_state_count = 0;
uint8_t hid_added[8] = {0};
uint8_t hid_removed[8] = {0};
uint8_t hid_add_count = 0;
uint8_t hid_remove_count = 0;
uint8_t xinput_processed[8] = {0};
uint8_t xinput_process_count = 0;

void advanced_key_init(void) {}
void advanced_key_clear(void) {}
void advanced_key_process(const advanced_key_event_t *event) {}
void advanced_key_tick(bool has_non_tap_hold_press, bool has_non_tap_hold_release) {}
bool advanced_key_combo_process(uint8_t key, bool pressed, uint32_t time) { return false; }
bool advanced_key_combo_task(void) { return false; }
void advanced_key_combo_invalidate_cache(void) {}
void advanced_key_update_last_key_time(uint32_t time) {}
bool advanced_key_has_undecided(void) { return false; }
void advanced_key_abort_macros(void) {}

void deferred_action_process(void) {}
bool deferred_action_push(const deferred_action_t *action) { return true; }
void deferred_action_clear(void) { deferred_action_clear_count++; }

void board_enter_bootloader(void) {}
void board_reset(void) { board_reset_count++; }
uint32_t timer_read(void) { return mock_timer; }
bool wear_leveling_write(uint32_t address, const void *data, uint32_t len) {
    wear_leveling_write_count++;
    last_write_address = address;
    last_write_len = len;
    last_write_u32 = 0;
    memcpy(&last_write_u32, data, len > sizeof(last_write_u32) ? sizeof(last_write_u32) : len);
    return true;
}

void hid_keycode_add(uint8_t keycode) {
    if (hid_add_count < 8) {
        hid_added[hid_add_count++] = keycode;
    }
}
void hid_keycode_remove(uint8_t keycode) {
    if (hid_remove_count < 8) {
        hid_removed[hid_remove_count++] = keycode;
    }
}
void hid_send_reports(void) {}
void hid_clear_runtime_state(void) { hid_clear_runtime_state_count++; }

void xinput_process(uint8_t key) {
    if (xinput_process_count < 8) {
        xinput_processed[xinput_process_count++] = key;
    }
}
void xinput_reset_runtime_state(void) { xinput_reset_runtime_state_count++; }

#if defined(RGB_ENABLED)
static rgb_config_t mock_rgb_config;

rgb_config_t *rgb_get_config(void) { return &mock_rgb_config; }
void rgb_apply_config(void) { rgb_apply_config_count++; }
#endif

#if defined(JOYSTICK_ENABLED)
joystick_config_t mock_joystick_config = {
    .mode = JOYSTICK_MODE_MOUSE,
};

joystick_config_t joystick_get_config(void) { return mock_joystick_config; }
void joystick_apply_config(joystick_config_t config) { mock_joystick_config = config; }
void joystick_set_config(joystick_config_t config) { mock_joystick_config = config; }
#endif

// --- Tests ---
void setUp(void) {
    memset(&mock_eeconfig, 0, sizeof(eeconfig_t));
    memset(key_matrix, 0, sizeof(key_matrix));
    mock_timer = 0;
    board_reset_count = 0;
    wear_leveling_write_count = 0;
    last_write_address = 0;
    last_write_len = 0;
    last_write_u32 = 0;
    rgb_apply_config_count = 0;
    deferred_action_clear_count = 0;
    hid_clear_runtime_state_count = 0;
    xinput_reset_runtime_state_count = 0;
    memset(hid_added, 0, sizeof(hid_added));
    memset(hid_removed, 0, sizeof(hid_removed));
    hid_add_count = 0;
    hid_remove_count = 0;
    memset(xinput_processed, 0, sizeof(xinput_processed));
    xinput_process_count = 0;
    mock_eeconfig.profiles[0].gamepad_options.keyboard_enabled = true;
#if defined(RGB_ENABLED)
    memset(&mock_rgb_config, 0, sizeof(mock_rgb_config));
    mock_eeconfig.profiles[0].rgb_config.enabled = 1;
    mock_eeconfig.profiles[0].rgb_config.current_effect = RGB_EFFECT_SOLID_COLOR;
#endif
#if defined(JOYSTICK_ENABLED)
    mock_joystick_config.mode = JOYSTICK_MODE_MOUSE;
#endif
    layout_init();
}

void tearDown(void) {
}

void test_layout_process_key(void) {
    // Basic test: Register a normal key and see if layout processing goes through
    mock_eeconfig.profiles[0].keymap[0][1] = 0x04; // KC_A
    bool has_press = layout_process_key(1, true); // press key index 1
    
    // As it is KC_A (0x04) which is a regular key, not tap-hold
    TEST_ASSERT_TRUE(has_press);
}

void test_layout_process_key_release_counts_as_non_tap_hold_event(void) {
    mock_eeconfig.profiles[0].keymap[0][1] = KC_A;

    TEST_ASSERT_TRUE(layout_process_key(1, true));
    TEST_ASSERT_TRUE(layout_process_key(1, false));
}

void test_poll_rate_toggle_persists_options_and_resets(void) {
    const uint32_t initial_options = 0x0000120f;
    mock_eeconfig.options.raw = initial_options;

    layout_register(INPUT_ROUTING_VIRTUAL_KEY, SP_POLL_RATE_TOGGLE);

    TEST_ASSERT_EQUAL_UINT32(1, wear_leveling_write_count);
    TEST_ASSERT_EQUAL_UINT32(offsetof(eeconfig_t, options), last_write_address);
    TEST_ASSERT_EQUAL_UINT32(sizeof(eeconfig_options_t), last_write_len);
    TEST_ASSERT_EQUAL_UINT32(1, board_reset_count);

    eeconfig_options_t written_options = {.raw = last_write_u32};
    TEST_ASSERT_FALSE(written_options.high_polling_rate_enabled);
    TEST_ASSERT_EQUAL_UINT32(initial_options ^ ((uint32_t)1 << 2),
                             written_options.raw);
}

void test_profile_switch_resets_runtime_state(void) {
    layout_register(INPUT_ROUTING_VIRTUAL_KEY, PF(1));

    TEST_ASSERT_EQUAL_UINT32(2, wear_leveling_write_count);
    TEST_ASSERT_EQUAL_UINT32(1, deferred_action_clear_count);
    TEST_ASSERT_EQUAL_UINT32(1, hid_clear_runtime_state_count);
    TEST_ASSERT_EQUAL_UINT32(1, xinput_reset_runtime_state_count);
}

void test_layout_sorts_same_timestamp_presses_by_distance(void) {
    mock_eeconfig.profiles[0].keymap[0][1] = KC_B;
    mock_eeconfig.profiles[0].keymap[0][2] = KC_A;

    key_matrix[1].is_pressed = true;
    key_matrix[1].event_time = 10;
    key_matrix[1].distance = 120;

    key_matrix[2].is_pressed = true;
    key_matrix[2].event_time = 10;
    key_matrix[2].distance = 200;

    layout_task();

    TEST_ASSERT_EQUAL_UINT8(2, hid_add_count);
    TEST_ASSERT_EQUAL_UINT8(KC_A, hid_added[0]);
    TEST_ASSERT_EQUAL_UINT8(KC_B, hid_added[1]);
}

void test_layout_processes_gamepad_keys_when_xinput_disabled(void) {
    mock_eeconfig.options.xinput_enabled = false;
    mock_eeconfig.profiles[0].keymap[0][1] = KC_A;
    mock_eeconfig.profiles[0].gamepad_buttons[1] = GP_BUTTON_A;
    mock_eeconfig.profiles[0].gamepad_options.keyboard_enabled = false;
    mock_eeconfig.profiles[0].gamepad_options.gamepad_override = true;

    key_matrix[1].is_pressed = true;
    key_matrix[1].event_time = 5;

    layout_task();

    TEST_ASSERT_EQUAL_UINT8(1, xinput_process_count);
    TEST_ASSERT_EQUAL_UINT8(1, xinput_processed[0]);
    TEST_ASSERT_EQUAL_UINT8(0, hid_add_count);
}

#if defined(RGB_ENABLED)
void test_rgb_effect_next_persists_and_updates_live_config(void) {
    mock_eeconfig.profiles[0].rgb_config.current_effect = RGB_EFFECT_SOLID_COLOR;
    layout_init();
    wear_leveling_write_count = 0;
    rgb_apply_config_count = 0;

    layout_register(INPUT_ROUTING_VIRTUAL_KEY, SP_RGB_EFFECT_NEXT);

    TEST_ASSERT_EQUAL_UINT32(1, wear_leveling_write_count);
    TEST_ASSERT_EQUAL_UINT32(
        offsetof(eeconfig_t, profiles) +
            offsetof(eeconfig_profile_t, rgb_config) +
            offsetof(rgb_config_t, current_effect),
        last_write_address);
    TEST_ASSERT_EQUAL_UINT32(sizeof(uint8_t), last_write_len);
    TEST_ASSERT_EQUAL_UINT8(RGB_EFFECT_ALPHAS_MODS, last_write_u32);
    TEST_ASSERT_EQUAL_UINT8(RGB_EFFECT_ALPHAS_MODS, mock_rgb_config.current_effect);
    TEST_ASSERT_EQUAL_UINT32(1, rgb_apply_config_count);
}

void test_rgb_effect_prev_wraps_without_hitting_off(void) {
    mock_eeconfig.profiles[0].rgb_config.current_effect = RGB_EFFECT_SOLID_COLOR;
    layout_init();
    wear_leveling_write_count = 0;
    rgb_apply_config_count = 0;

    layout_register(INPUT_ROUTING_VIRTUAL_KEY, SP_RGB_EFFECT_PREV);

    TEST_ASSERT_EQUAL_UINT8((uint8_t)(RGB_EFFECT_MAX - 1), last_write_u32);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(RGB_EFFECT_MAX - 1),
                            mock_rgb_config.current_effect);
    TEST_ASSERT_EQUAL_UINT32(1, rgb_apply_config_count);
}
#endif

#if defined(JOYSTICK_ENABLED)
void test_joystick_scroll_mo_restores_previous_mode(void) {
    mock_joystick_config.mode = JOYSTICK_MODE_XINPUT_RS;

    layout_register(INPUT_ROUTING_VIRTUAL_KEY, SP_JOY_SCROLL_MO);
    TEST_ASSERT_EQUAL_UINT8(JOYSTICK_MODE_SCROLL, mock_joystick_config.mode);

    layout_unregister(INPUT_ROUTING_VIRTUAL_KEY, SP_JOY_SCROLL_MO);
    TEST_ASSERT_EQUAL_UINT8(JOYSTICK_MODE_XINPUT_RS, mock_joystick_config.mode);
}
#endif

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_layout_process_key);
    RUN_TEST(test_layout_process_key_release_counts_as_non_tap_hold_event);
    RUN_TEST(test_poll_rate_toggle_persists_options_and_resets);
    RUN_TEST(test_profile_switch_resets_runtime_state);
    RUN_TEST(test_layout_sorts_same_timestamp_presses_by_distance);
    RUN_TEST(test_layout_processes_gamepad_keys_when_xinput_disabled);
#if defined(RGB_ENABLED)
    RUN_TEST(test_rgb_effect_next_persists_and_updates_live_config);
    RUN_TEST(test_rgb_effect_prev_wraps_without_hitting_off);
#endif
#if defined(JOYSTICK_ENABLED)
    RUN_TEST(test_joystick_scroll_mo_restores_previous_mode);
#endif
    UNITY_END();
    return 0;
}
