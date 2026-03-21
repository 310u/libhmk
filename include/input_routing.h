/*
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "hid.h"
#include "layout.h"

#define INPUT_ROUTING_VIRTUAL_KEY UINT8_MAX

// Key-backed inputs participate in the normal layout/keymap pipeline.
static inline bool input_key_press(uint8_t key) {
  return layout_process_key(key, true);
}

static inline bool input_key_release(uint8_t key) {
  return layout_process_key(key, false);
}

// Virtual keycodes bypass matrix indexing but still route through layout.
static inline void input_keycode_press(uint8_t keycode) {
  layout_register(INPUT_ROUTING_VIRTUAL_KEY, keycode);
}

static inline void input_keycode_release(uint8_t keycode) {
  layout_unregister(INPUT_ROUTING_VIRTUAL_KEY, keycode);
}

static inline void input_layout_press(uint8_t key, uint8_t keycode) {
  if (key == INPUT_ROUTING_VIRTUAL_KEY)
    input_keycode_press(keycode);
  else
    layout_register(key, keycode);
}

static inline void input_layout_release(uint8_t key, uint8_t keycode) {
  if (key == INPUT_ROUTING_VIRTUAL_KEY)
    input_keycode_release(keycode);
  else
    layout_unregister(key, keycode);
}

// Direct keyboard outputs bypass layout entirely and write to HID state.
static inline void input_keyboard_press(uint8_t keycode) {
  hid_keycode_add(keycode);
}

static inline void input_keyboard_release(uint8_t keycode) {
  hid_keycode_remove(keycode);
}
