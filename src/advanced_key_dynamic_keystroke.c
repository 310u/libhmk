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

#include "advanced_key_dynamic_keystroke.h"

#include "deferred_actions.h"
#include "eeconfig.h"
#include "keycodes.h"
#include "layout.h"
#include "lib/bitmap.h"
#include "matrix.h"

static bitmap_t dks_rt_disabled_keys[BITMAP_SIZE(NUM_KEYS)] = {0};

void advanced_key_dynamic_keystroke_clear(void) {
  for (uint32_t key = 0; key < NUM_KEYS; key++) {
    if (bitmap_get(dks_rt_disabled_keys, key))
      matrix_disable_rapid_trigger((uint8_t)key, false);
  }

  memset(dks_rt_disabled_keys, 0, sizeof(dks_rt_disabled_keys));
}

void advanced_key_dynamic_keystroke_process(const advanced_key_event_t *event,
                                            advanced_key_state_t *states) {
  const dynamic_keystroke_t *dks =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].dynamic_keystroke;
  ak_state_dynamic_keystroke_t *state =
      &states[event->ak_index].dynamic_keystroke;
  const bool is_bottomed_out =
      key_matrix[event->key].distance >= dks->bottom_out_point;
  uint8_t event_type = event->type;

  if (is_bottomed_out && !state->is_bottomed_out)
    event_type = AK_EVENT_TYPE_BOTTOM_OUT;
  else if (event_type != AK_EVENT_TYPE_RELEASE && !is_bottomed_out &&
           state->is_bottomed_out)
    event_type = AK_EVENT_TYPE_RELEASE_FROM_BOTTOM_OUT;
  state->is_bottomed_out = is_bottomed_out;

  if (event_type == AK_EVENT_TYPE_HOLD)
    return;

  const bool rt_disabled = event_type != AK_EVENT_TYPE_RELEASE;
  matrix_disable_rapid_trigger(event->key, rt_disabled);
  bitmap_set(dks_rt_disabled_keys, event->key, rt_disabled);

  for (uint32_t i = 0; i < 4; i++) {
    const uint8_t keycode = dks->keycodes[i];
    const uint8_t event_offset = (uint8_t)(event_type - AK_EVENT_TYPE_PRESS);
    const uint8_t action =
        (uint8_t)((dks->bitmap[i] >> (event_offset * 2U)) & 3);

    if (keycode == KC_NO || action == DKS_ACTION_HOLD)
      continue;

    if (state->is_pressed[i]) {
      layout_unregister(event->key, keycode);
      state->is_pressed[i] = false;
    }

    if (action == DKS_ACTION_PRESS || action == DKS_ACTION_TAP) {
      const deferred_action_t deferred_action = {
          .type = action == DKS_ACTION_PRESS ? DEFERRED_ACTION_TYPE_PRESS
                                             : DEFERRED_ACTION_TYPE_TAP,
          .key = event->key,
          .keycode = keycode,
      };
      state->is_pressed[i] =
          deferred_action_push(&deferred_action) && action == DKS_ACTION_PRESS;
    }
  }
}
