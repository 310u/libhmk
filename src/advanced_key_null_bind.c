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

#include "advanced_key_null_bind.h"

#include "eeconfig.h"
#include "keycodes.h"
#include "layout.h"
#include "matrix.h"

void advanced_key_null_bind_process(const advanced_key_event_t *event,
                                    advanced_key_state_t *states) {
  const null_bind_t *null_bind =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].null_bind;
  ak_state_null_bind_t *state = &states[event->ak_index].null_bind;

  const uint8_t keys[] = {
      CURRENT_PROFILE.advanced_keys[event->ak_index].key,
      null_bind->secondary_key,
  };
  const uint8_t index = event->key == keys[0] ? 0 : 1;

  switch (event->type) {
  case AK_EVENT_TYPE_PRESS:
    state->keycodes[index] = event->keycode;
    break;

  case AK_EVENT_TYPE_RELEASE:
    if (state->is_pressed[index]) {
      layout_unregister(keys[index], state->keycodes[index]);
      state->is_pressed[index] = false;
    }
    state->keycodes[index] = KC_NO;
    break;

  default:
    break;
  }

  bool is_pressed[] = {
      state->keycodes[0] != KC_NO,
      state->keycodes[1] != KC_NO,
  };
  if (is_pressed[0] && is_pressed[1]) {
    if ((null_bind->bottom_out_point > 0) &&
        ((key_matrix[keys[0]].distance >= null_bind->bottom_out_point) &&
         (key_matrix[keys[1]].distance >= null_bind->bottom_out_point)))
      is_pressed[0] = is_pressed[1] = true;
    else if (null_bind->behavior == NB_BEHAVIOR_DISTANCE) {
      is_pressed[index] = key_matrix[keys[index]].distance >=
                          key_matrix[keys[index ^ 1]].distance;
      is_pressed[index ^ 1] = !is_pressed[index];
    } else if (event->type == AK_EVENT_TYPE_PRESS) {
      is_pressed[index] =
          (null_bind->behavior != NB_BEHAVIOR_NEUTRAL) &&
          ((null_bind->behavior == NB_BEHAVIOR_LAST) ||
           ((null_bind->behavior == NB_BEHAVIOR_PRIMARY) && (index == 0)) ||
           ((null_bind->behavior == NB_BEHAVIOR_SECONDARY) && (index == 1)));
      is_pressed[index ^ 1] = (null_bind->behavior != NB_BEHAVIOR_NEUTRAL) &&
                              !is_pressed[index];
    } else {
      is_pressed[0] = state->is_pressed[0];
      is_pressed[1] = state->is_pressed[1];
    }
  }

  for (uint32_t i = 0; i < 2; i++) {
    if (is_pressed[i] && !state->is_pressed[i]) {
      layout_register(keys[i], state->keycodes[i]);
      state->is_pressed[i] = true;
    } else if (!is_pressed[i] && state->is_pressed[i]) {
      layout_unregister(keys[i], state->keycodes[i]);
      state->is_pressed[i] = false;
    }
  }
}
