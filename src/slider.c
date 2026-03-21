#include "slider.h"
#include "eeconfig.h"
#include "input_routing.h"
#include "matrix.h"
#include "keycodes.h"
#include "hardware/hardware.h"

#if !defined(SLIDER_KEY_INDEX)

void slider_init(void) {}

void slider_task(void) {}

#else

static uint8_t last_slider_val = 0;
static uint32_t last_slider_tick = 0;

void slider_init(void) {
  last_slider_val = key_matrix[SLIDER_KEY_INDEX].distance;
}

void slider_task(void) {
  if (eeconfig->options.slider_mode == 0) {
    return; // Disabled
  }

  // Throttle updates to 50Hz.
  uint32_t tick = timer_read();
  if (tick - last_slider_tick < 20) {
    return;
  }
  last_slider_tick = tick;

  uint8_t current_val = key_matrix[SLIDER_KEY_INDEX].distance;

  if (eeconfig->options.slider_mode == 1) { // Volume mapping
    // Dispatch one volume step for a sufficiently large analog delta.
    uint8_t threshold = 8;

    input_keyboard_release(KC_AUDIO_VOL_UP);
    input_keyboard_release(KC_AUDIO_VOL_DOWN);

    if (current_val > last_slider_val + threshold) {
      input_keyboard_press(KC_AUDIO_VOL_UP);
      last_slider_val = current_val;
    } else if (current_val + threshold < last_slider_val) {
      input_keyboard_press(KC_AUDIO_VOL_DOWN);
      last_slider_val = current_val;
    }
  } else if (eeconfig->options.slider_mode == 2) { // Gamepad override
    // xinput.c consumes the analog position directly in this mode.
    last_slider_val = current_val;
  }
}

#endif
