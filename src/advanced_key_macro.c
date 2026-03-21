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

#include "advanced_key_macro.h"

#include "eeconfig.h"
#include "hardware/hardware.h"
#include "input_routing.h"

#define MACRO_TAP_HOLD_MS 30U
#define MACRO_RELEASE_GAP_MS 15U
#define MACRO_DELAY_UNIT_MS 10U

static void advanced_key_macro_stop(ak_state_macro_t *state) {
  state->is_playing = false;
}

static void advanced_key_macro_start(ak_state_macro_t *state) {
  state->event_index = 0;
  state->delay_until = timer_read();
  state->is_playing = true;
  state->is_tapping = false;
}

static bool advanced_key_macro_release_tap(ak_state_macro_t *state) {
  if (!state->is_tapping)
    return false;

  input_keycode_release(state->tap_keycode);
  state->is_tapping = false;
  state->delay_until = timer_read() + MACRO_RELEASE_GAP_MS;
  return true;
}

static bool advanced_key_macro_execute_event(const macro_event_t *event,
                                             ak_state_macro_t *state) {
  switch (event->action) {
  case MACRO_ACTION_END:
    advanced_key_macro_stop(state);
    return true;

  case MACRO_ACTION_TAP:
    input_keycode_press(event->keycode);
    state->tap_keycode = event->keycode;
    state->is_tapping = true;
    state->delay_until = timer_read() + MACRO_TAP_HOLD_MS;
    return true;

  case MACRO_ACTION_PRESS:
    input_keycode_press(event->keycode);
    state->delay_until = timer_read() + MACRO_TAP_HOLD_MS;
    return true;

  case MACRO_ACTION_RELEASE:
    input_keycode_release(event->keycode);
    state->delay_until = timer_read() + MACRO_RELEASE_GAP_MS;
    return true;

  case MACRO_ACTION_DELAY:
    state->delay_until =
        timer_read() + ((uint32_t)event->keycode * MACRO_DELAY_UNIT_MS);
    return true;

  default:
    return false;
  }
}

void advanced_key_macro_abort_all(advanced_key_state_t *states) {
  for (uint8_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    if (CURRENT_PROFILE.advanced_keys[i].type != AK_TYPE_MACRO)
      continue;

    ak_state_macro_t *state = &states[i].macro;
    if (!state->is_playing)
      continue;

    advanced_key_macro_stop(state);
    if (state->is_tapping) {
      input_keycode_release(state->tap_keycode);
      state->is_tapping = false;
    }
  }
}

void advanced_key_macro_process(const advanced_key_event_t *event,
                                advanced_key_state_t *states) {
  if (event->type != AK_EVENT_TYPE_PRESS)
    return;

  ak_state_macro_t *state = &states[event->ak_index].macro;
  if (state->is_playing)
    return;

  const macro_key_t *macro_key =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].macro_key;
  if (macro_key->macro_index >= NUM_MACROS)
    return;

  advanced_key_macro_start(state);
}

void advanced_key_macro_tick(const advanced_key_t *ak, ak_state_macro_t *state) {
  if (!state->is_playing || timer_read() < state->delay_until)
    return;

  if (advanced_key_macro_release_tap(state))
    return;

  const macro_key_t *macro_key = &ak->macro_key;
  if (macro_key->macro_index >= NUM_MACROS) {
    advanced_key_macro_stop(state);
    return;
  }

  const macro_t *macro = &CURRENT_PROFILE.macros[macro_key->macro_index];
  while (state->is_playing) {
    if (state->event_index >= MAX_MACRO_EVENTS) {
      advanced_key_macro_stop(state);
      break;
    }

    const macro_event_t *event = &macro->events[state->event_index];
    state->event_index++;
    if (advanced_key_macro_execute_event(event, state))
      break;
  }
}
