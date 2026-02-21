#include <unity.h>
#include "layout.h"
#include "advanced_keys.h"
#include "deferred_actions.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "keycodes.h"
#include "lib/bitmap.h"
#include "matrix.h"
#include "xinput.h"

// --- Mocks ---
key_state_t key_matrix[NUM_KEYS];
eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;
uint32_t mock_timer = 0;

void advanced_key_init(void) {}
void advanced_key_clear(void) {}
void advanced_key_process(const advanced_key_event_t *event) {}
void advanced_key_tick(bool has_non_tap_hold_press, bool has_non_tap_hold_release) {}
bool advanced_key_combo_process(uint8_t key, bool pressed, uint32_t time) { return false; }
bool advanced_key_combo_task(void) { return false; }
void advanced_key_combo_invalidate_cache(void) {}
void advanced_key_update_last_key_time(uint32_t time) {}
bool advanced_key_has_undecided(void) { return false; }

void deferred_action_process(void) {}
bool deferred_action_push(const deferred_action_t *action) { return true; }

void board_enter_bootloader(void) {}
uint32_t timer_read(void) { return mock_timer; }
bool wear_leveling_write(uint32_t address, const void *data, uint32_t len) { return true; }

void hid_keycode_add(uint8_t keycode) {}
void hid_keycode_remove(uint8_t keycode) {}
void hid_send_reports(void) {}

void xinput_process(uint8_t key) {}

// --- Tests ---
void setUp(void) {
    memset(&mock_eeconfig, 0, sizeof(eeconfig_t));
    memset(key_matrix, 0, sizeof(key_matrix));
    mock_timer = 0;
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

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_layout_process_key);
    UNITY_END();
    return 0;
}
