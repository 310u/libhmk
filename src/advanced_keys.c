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

#include "advanced_keys.h"

#include "advanced_key_combo.h"
#include "advanced_key_dynamic_keystroke.h"
#include "advanced_key_macro.h"
#include "advanced_key_null_bind.h"
#include "advanced_key_tap_hold.h"
#include "advanced_key_toggle.h"
#include "eeconfig.h"

static advanced_key_state_t ak_states[NUM_ADVANCED_KEYS];

void advanced_key_init(void) {}

void advanced_key_abort_macros(void) { advanced_key_macro_abort_all(ak_states); }

void advanced_key_clear(void) {
  advanced_key_dynamic_keystroke_clear();
  memset(ak_states, 0, sizeof(ak_states));
  advanced_key_tap_hold_clear();
  advanced_key_combo_clear();
}

void advanced_key_process(const advanced_key_event_t *event) {
  if (event->ak_index >= NUM_ADVANCED_KEYS)
    return;

  switch (CURRENT_PROFILE.advanced_keys[event->ak_index].type) {
  case AK_TYPE_NULL_BIND:
    advanced_key_null_bind_process(event, ak_states);
    break;

  case AK_TYPE_DYNAMIC_KEYSTROKE:
    advanced_key_dynamic_keystroke_process(event, ak_states);
    break;

  case AK_TYPE_TAP_HOLD:
    advanced_key_tap_hold_process(event, ak_states);
    break;

  case AK_TYPE_TOGGLE:
    advanced_key_toggle_process(event, ak_states);
    break;

  case AK_TYPE_MACRO:
    advanced_key_macro_process(event, ak_states);
    break;

  default:
    break;
  }
}

void advanced_key_tick(bool has_non_tap_hold_press,
                       bool has_non_tap_hold_release) {
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
    advanced_key_state_t *state = &ak_states[i];

    switch (ak->type) {
    case AK_TYPE_TAP_HOLD:
      advanced_key_tap_hold_tick(ak, (uint8_t)i, &state->tap_hold,
                                 has_non_tap_hold_press,
                                 has_non_tap_hold_release);
      break;

    case AK_TYPE_TOGGLE:
      advanced_key_toggle_tick(ak, &state->toggle);
      break;

    case AK_TYPE_MACRO:
      advanced_key_macro_tick(ak, &state->macro);
      break;

    default:
      break;
    }
  }
}

void advanced_key_update_last_key_time(uint32_t time) {
  advanced_key_tap_hold_update_last_key_time(time);
}

bool advanced_key_has_undecided(void) {
  return advanced_key_tap_hold_has_undecided(ak_states);
}
