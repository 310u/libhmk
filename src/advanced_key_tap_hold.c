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

#include "advanced_key_tap_hold.h"

#include "deferred_actions.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "keycodes.h"
#include "layout.h"

static uint32_t last_tap_hold_tap_time[NUM_ADVANCED_KEYS];
static uint32_t last_non_mod_key_time;

static void tap_hold_register_tap(uint8_t key, uint8_t keycode) {
  deferred_action_t deferred_action = {
      .type = DEFERRED_ACTION_TYPE_RELEASE,
      .key = key,
      .keycode = keycode,
  };

  if (deferred_action_push(&deferred_action))
    layout_register(key, keycode);
}

static void tap_hold_record_tap(uint8_t ak_index) {
  last_tap_hold_tap_time[ak_index] = timer_read();
}

static uint16_t tap_hold_double_tap_window(const tap_hold_t *tap_hold) {
  return tap_hold->quick_tap_ms > 0 ? tap_hold->quick_tap_ms
                                    : tap_hold->tapping_term;
}

static bool tap_hold_should_resolve_as_hold(const tap_hold_t *tap_hold,
                                            const ak_state_tap_hold_t *state,
                                            bool has_non_tap_hold_press) {
  const uint8_t flavor = TH_GET_FLAVOR(tap_hold->flags);
  const bool expired = timer_elapsed(state->since) >= tap_hold->tapping_term;

  switch (flavor) {
  case TAP_HOLD_FLAVOR_HOLD_PREFERRED:
    return expired || has_non_tap_hold_press;

  case TAP_HOLD_FLAVOR_BALANCED:
    return expired || (state->interrupted && state->other_key_released);

  case TAP_HOLD_FLAVOR_TAP_PREFERRED:
    return expired;

  case TAP_HOLD_FLAVOR_TAP_UNLESS_INTERRUPTED:
    return state->interrupted && !expired;

  default:
    return false;
  }
}

static void tap_hold_handle_press(const advanced_key_event_t *event,
                                  advanced_key_state_t *states) {
  const tap_hold_t *tap_hold =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].tap_hold;
  ak_state_tap_hold_t *state = &states[event->ak_index].tap_hold;
  const bool has_double_tap = tap_hold->double_tap_keycode != KC_NO;

  if (state->stage == TAP_HOLD_STAGE_DOUBLE_TAP_WAIT) {
    tap_hold_register_tap(event->key, tap_hold->double_tap_keycode);
    state->stage = TAP_HOLD_STAGE_QUICK_TAP;
    return;
  }

  if (tap_hold->require_prior_idle_ms > 0 &&
      (timer_read() - last_non_mod_key_time) < tap_hold->require_prior_idle_ms) {
    tap_hold_register_tap(event->key, tap_hold->tap_keycode);
    state->stage = TAP_HOLD_STAGE_NONE;
    return;
  }

  if (!has_double_tap && tap_hold->quick_tap_ms > 0 &&
      (timer_read() - last_tap_hold_tap_time[event->ak_index]) <
          tap_hold->quick_tap_ms) {
    layout_register(event->key, tap_hold->tap_keycode);
    state->stage = TAP_HOLD_STAGE_QUICK_TAP;
    return;
  }

  state->since = timer_read();
  state->stage = TAP_HOLD_STAGE_TAP;
  state->interrupted = false;
  state->other_key_released = false;

  if (TH_GET_HOLD_WHILE_UNDECIDED(tap_hold->flags))
    layout_register(event->key, tap_hold->hold_keycode);
}

static void tap_hold_handle_release(const advanced_key_event_t *event,
                                    advanced_key_state_t *states) {
  const tap_hold_t *tap_hold =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].tap_hold;
  ak_state_tap_hold_t *state = &states[event->ak_index].tap_hold;
  const bool has_double_tap = tap_hold->double_tap_keycode != KC_NO;
  const bool retro = TH_GET_RETRO_TAPPING(tap_hold->flags);

  if (state->stage == TAP_HOLD_STAGE_TAP) {
    if (TH_GET_HOLD_WHILE_UNDECIDED(tap_hold->flags))
      layout_unregister(event->key, tap_hold->hold_keycode);

    if (retro && !state->interrupted &&
        timer_elapsed(state->since) >= tap_hold->tapping_term) {
      tap_hold_register_tap(event->key, tap_hold->tap_keycode);
      tap_hold_record_tap(event->ak_index);
    } else if (!retro || timer_elapsed(state->since) < tap_hold->tapping_term) {
      if (has_double_tap) {
        state->stage = TAP_HOLD_STAGE_DOUBLE_TAP_WAIT;
        state->since = timer_read();
        tap_hold_record_tap(event->ak_index);
        return;
      }

      tap_hold_register_tap(event->key, tap_hold->tap_keycode);
      tap_hold_record_tap(event->ak_index);
    }
  } else if (state->stage == TAP_HOLD_STAGE_HOLD) {
    layout_unregister(event->key, tap_hold->hold_keycode);
  } else if (state->stage == TAP_HOLD_STAGE_QUICK_TAP) {
    layout_unregister(event->key, has_double_tap ? tap_hold->double_tap_keycode
                                                 : tap_hold->tap_keycode);
    tap_hold_record_tap(event->ak_index);
  }

  state->stage = TAP_HOLD_STAGE_NONE;
}

void advanced_key_tap_hold_clear(void) {
  memset(last_tap_hold_tap_time, 0, sizeof(last_tap_hold_tap_time));
  last_non_mod_key_time = 0;
}

void advanced_key_tap_hold_process(const advanced_key_event_t *event,
                                   advanced_key_state_t *states) {
  switch (event->type) {
  case AK_EVENT_TYPE_PRESS:
    tap_hold_handle_press(event, states);
    break;

  case AK_EVENT_TYPE_RELEASE:
    tap_hold_handle_release(event, states);
    break;

  default:
    break;
  }
}

void advanced_key_tap_hold_tick(const advanced_key_t *ak, uint8_t ak_index,
                                ak_state_tap_hold_t *state,
                                bool has_non_tap_hold_press,
                                bool has_non_tap_hold_release) {
  if (has_non_tap_hold_press)
    state->interrupted = true;
  if (has_non_tap_hold_release)
    state->other_key_released = true;

  if (state->stage == TAP_HOLD_STAGE_DOUBLE_TAP_WAIT) {
    if (timer_elapsed(state->since) >= tap_hold_double_tap_window(&ak->tap_hold)) {
      tap_hold_register_tap(ak->key, ak->tap_hold.tap_keycode);
      state->stage = TAP_HOLD_STAGE_NONE;
    }
    return;
  }

  if (state->stage != TAP_HOLD_STAGE_TAP)
    return;

  if (tap_hold_should_resolve_as_hold(&ak->tap_hold, state,
                                      has_non_tap_hold_press)) {
    if (!TH_GET_HOLD_WHILE_UNDECIDED(ak->tap_hold.flags))
      layout_register(ak->key, ak->tap_hold.hold_keycode);
    state->stage = TAP_HOLD_STAGE_HOLD;
    return;
  }

  if (TH_GET_FLAVOR(ak->tap_hold.flags) == TAP_HOLD_FLAVOR_TAP_UNLESS_INTERRUPTED &&
      timer_elapsed(state->since) >= ak->tap_hold.tapping_term &&
      !state->interrupted) {
    if (TH_GET_HOLD_WHILE_UNDECIDED(ak->tap_hold.flags))
      layout_unregister(ak->key, ak->tap_hold.hold_keycode);

    tap_hold_register_tap(ak->key, ak->tap_hold.tap_keycode);
    tap_hold_record_tap(ak_index);
    state->stage = TAP_HOLD_STAGE_NONE;
  }
}

void advanced_key_tap_hold_update_last_key_time(uint32_t time) {
  last_non_mod_key_time = time;
}

bool advanced_key_tap_hold_has_undecided(const advanced_key_state_t *states) {
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    if (CURRENT_PROFILE.advanced_keys[i].type == AK_TYPE_TAP_HOLD &&
        states[i].tap_hold.stage == TAP_HOLD_STAGE_TAP)
      return true;
  }

  return false;
}
