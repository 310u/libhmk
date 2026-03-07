#include "slider.h"
#include "eeconfig.h"
#include "matrix.h"
#include "hid.h"
#include "keycodes.h"
#include "hardware/hardware.h"

// Assume slider is mapped to the last key in the matrix for mochiko39he
// In a real implementation, we would extract this from board_def.h
#define SLIDER_KEY_INDEX (NUM_KEYS - 1)

static uint8_t last_slider_val = 0;
static uint32_t last_slider_tick = 0;

void slider_init(void) {
    last_slider_val = key_matrix[SLIDER_KEY_INDEX].distance;
}

void slider_task(void) {
    if (eeconfig->options.slider_mode == 0) {
        return; // Disabled
    }
    
    // throttle updates to 50Hz
    uint32_t tick = timer_read();
    if (tick - last_slider_tick < 20) {
        return;
    }
    last_slider_tick = tick;

    uint8_t current_val = key_matrix[SLIDER_KEY_INDEX].distance;

    if (eeconfig->options.slider_mode == 1) { // Volume mapping
        // We use the slider to trigger Volume Up / Volume Down relative keypresses
        // Since distance is 0-255, we dispatch 1 volume tick per 4 units of change
        uint8_t threshold = 8;

        // Clear previous volume keys sent in previous ticks
        hid_keycode_remove(KC_AUDIO_VOL_UP);
        hid_keycode_remove(KC_AUDIO_VOL_DOWN);

        if (current_val > last_slider_val + threshold) {
            hid_keycode_add(KC_AUDIO_VOL_UP);
            last_slider_val = current_val;
        } else if (current_val + threshold < last_slider_val) {
            hid_keycode_add(KC_AUDIO_VOL_DOWN);
            last_slider_val = current_val;
        }
    } else if (eeconfig->options.slider_mode == 2) { // Gamepad override
        // Gamepad override is applied inside xinput.c instead!
        // We do nothing here for mode 2, as xinput.c will read the slider mapping directly.
        last_slider_val = current_val;
    }
}
