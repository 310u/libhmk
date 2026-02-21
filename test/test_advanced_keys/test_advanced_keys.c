#include <unity.h>
#include "advanced_keys.h"
#include "deferred_actions.h"
#include "eeconfig.h"
#include "matrix.h"
#include "layout.h"

// --- Mocks ---
key_state_t key_matrix[NUM_KEYS];
eeconfig_t mock_eeconfig;
const eeconfig_t *eeconfig = &mock_eeconfig;
uint32_t mock_timer = 0;

void matrix_disable_rapid_trigger(uint8_t key, bool disable) {}
bool deferred_action_push(const deferred_action_t *action) { return true; }
uint32_t timer_read(void) { return mock_timer; }
uint32_t timer_elapsed(uint32_t last) { return mock_timer - last; }
// --- Tests ---
void setUp(void) {
    memset(&mock_eeconfig, 0, sizeof(eeconfig_t));
    memset(key_matrix, 0, sizeof(key_matrix));
    mock_timer = 0;
    advanced_key_clear();
}

void tearDown(void) {
}

// Track what was registered layout_register mock
static uint8_t last_registered_key;
static uint8_t last_registered_keycode;

void layout_register(uint8_t key, uint8_t keycode) {
    last_registered_key = key;
    last_registered_keycode = keycode;
}
void layout_unregister(uint8_t key, uint8_t keycode) {}
uint8_t layout_get_current_layer(void) { return 0; }
bool layout_process_key(uint8_t key, bool pressed) { return true; }

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

void test_advanced_keys_init(void) {
    advanced_key_init();
    TEST_ASSERT_TRUE(1);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_advanced_keys_init);
    RUN_TEST(test_advanced_keys_combo);
    UNITY_END();
    return 0;
}
