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

#include "deferred_actions.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "keycodes.h"
#include "layout.h"
#include "matrix.h"

static advanced_key_state_t ak_states[NUM_ADVANCED_KEYS];

static void advanced_key_null_bind(const advanced_key_event_t *event) {
  const null_bind_t *null_bind =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].null_bind;
  ak_state_null_bind_t *state = &ak_states[event->ak_index].null_bind;

  const uint8_t keys[] = {
      CURRENT_PROFILE.advanced_keys[event->ak_index].key,
      null_bind->secondary_key,
  };
  const uint8_t index = event->key == keys[0] ? 0 : 1;

  // Update the active keycodes
  switch (event->type) {
  case AK_EVENT_TYPE_PRESS:
    state->keycodes[index] = event->keycode;
    break;

  case AK_EVENT_TYPE_RELEASE:
    if (state->is_pressed[index]) {
      // Also release the key if it is registered
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
  if (is_pressed[0] & is_pressed[1]) {
    // Both keys are pressed so we perform the Null Bind resolution.
    if ((null_bind->bottom_out_point > 0) &&
        ((key_matrix[keys[0]].distance >= null_bind->bottom_out_point) &
         (key_matrix[keys[1]].distance >= null_bind->bottom_out_point)))
      // Input on both bottom out is enabled and both keys are bottomed out so
      // we register both keys.
      is_pressed[0] = is_pressed[1] = true;
    else if (null_bind->behavior == NB_BEHAVIOR_DISTANCE) {
      // Always compare the distance, regardless of the event type. If there is
      // a tie between the travel distances, the last pressed key is
      // prioritized.
      is_pressed[index] = key_matrix[keys[index]].distance >=
                          key_matrix[keys[index ^ 1]].distance;
      is_pressed[index ^ 1] = !is_pressed[index];
    } else if (event->type == AK_EVENT_TYPE_PRESS) {
      // Other behaviors only require comparison on press events.
      is_pressed[index] =
          (null_bind->behavior != NB_BEHAVIOR_NEUTRAL) &
          ((null_bind->behavior == NB_BEHAVIOR_LAST) |
           ((null_bind->behavior == NB_BEHAVIOR_PRIMARY) & (index == 0)) |
           ((null_bind->behavior == NB_BEHAVIOR_SECONDARY) & (index == 1)));
      // Only one key can be registered at a time except for the
      // `NB_BEHAVIOR_NEUTRAL`.
      is_pressed[index ^ 1] =
          (null_bind->behavior != NB_BEHAVIOR_NEUTRAL) & !is_pressed[index];
    } else {
      // No action is required for other event types.
      is_pressed[0] = state->is_pressed[0];
      is_pressed[1] = state->is_pressed[1];
    }
  }

  // Update the key states. The only changes here are the results of the Null
  // Bind resolution.
  for (uint32_t i = 0; i < 2; i++) {
    if (is_pressed[i] & !state->is_pressed[i]) {
      layout_register(keys[i], state->keycodes[i]);
      state->is_pressed[i] = true;
    } else if (!is_pressed[i] & state->is_pressed[i]) {
      layout_unregister(keys[i], state->keycodes[i]);
      state->is_pressed[i] = false;
    }
  }
}

static void advanced_key_dynamic_keystroke(const advanced_key_event_t *event) {
  static deferred_action_t deferred_action = {0};

  const dynamic_keystroke_t *dks =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].dynamic_keystroke;
  ak_state_dynamic_keystroke_t *state =
      &ak_states[event->ak_index].dynamic_keystroke;

  const bool is_bottomed_out =
      (key_matrix[event->key].distance >= dks->bottom_out_point);
  uint8_t event_type = event->type;

  if (is_bottomed_out & !state->is_bottomed_out)
    event_type = AK_EVENT_TYPE_BOTTOM_OUT;
  else if ((event_type != AK_EVENT_TYPE_RELEASE) & !is_bottomed_out &
           state->is_bottomed_out)
    // Key release is prioritized over release from bottom out.
    event_type = AK_EVENT_TYPE_RELEASE_FROM_BOTTOM_OUT;
  state->is_bottomed_out = is_bottomed_out;

  if (event_type == AK_EVENT_TYPE_HOLD)
    // Nothing to do for hold events
    return;

  // Disable Rapid Trigger when the key is bound with Dynamic Keystroke
  matrix_disable_rapid_trigger(event->key, event_type != AK_EVENT_TYPE_RELEASE);
  for (uint32_t i = 0; i < 4; i++) {
    const uint8_t keycode = dks->keycodes[i];
    // We arrange the event types so that we can use the event type as an index
    // to the bitmap.
    const uint8_t action =
        (dks->bitmap[i] >> ((event_type - AK_EVENT_TYPE_PRESS) * 2)) & 3;

    if (keycode == KC_NO || action == DKS_ACTION_HOLD)
      continue;

    if (state->is_pressed[i]) {
      // All actions except for `DKS_ACTION_HOLD` require the key to be
      // unregistered first if it was registered.
      layout_unregister(event->key, keycode);
      state->is_pressed[i] = false;
    }

    if ((action == DKS_ACTION_PRESS) | (action == DKS_ACTION_TAP)) {
      // The report may have been modified in the previous step so we defer
      // the actual DKS action to the next matrix scan.
      deferred_action = (deferred_action_t){
          .type = action == DKS_ACTION_PRESS ? DEFERRED_ACTION_TYPE_PRESS
                                             : DEFERRED_ACTION_TYPE_TAP,
          .key = event->key,
          .keycode = keycode,
      };
      state->is_pressed[i] = (deferred_action_push(&deferred_action) &
                              (action == DKS_ACTION_PRESS));
    }
  }
}

static void advanced_key_tap_hold(const advanced_key_event_t *event) {
  static deferred_action_t deferred_action = {0};

  const tap_hold_t *tap_hold =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].tap_hold;
  ak_state_tap_hold_t *state = &ak_states[event->ak_index].tap_hold;

  switch (event->type) {
  case AK_EVENT_TYPE_PRESS:
    state->since = timer_read();
    state->stage = TAP_HOLD_STAGE_TAP;
    break;

  case AK_EVENT_TYPE_RELEASE:
    if (state->stage == TAP_HOLD_STAGE_TAP) {
      deferred_action = (deferred_action_t){
          .type = DEFERRED_ACTION_TYPE_RELEASE,
          .key = event->key,
          .keycode = tap_hold->tap_keycode,
      };
      if (deferred_action_push(&deferred_action))
        // We only perform the tap action if the release action was
        // successfully.
        layout_register(event->key, tap_hold->tap_keycode);
    } else if (state->stage == TAP_HOLD_STAGE_HOLD)
      layout_unregister(event->key, tap_hold->hold_keycode);
    state->stage = TAP_HOLD_STAGE_NONE;
    break;

  default:
    break;
  }
}

static void advanced_key_toggle(const advanced_key_event_t *event) {
  const toggle_t *toggle =
      &CURRENT_PROFILE.advanced_keys[event->ak_index].toggle;
  ak_state_toggle_t *state = &ak_states[event->ak_index].toggle;

  switch (event->type) {
  case AK_EVENT_TYPE_PRESS:
    layout_register(event->key, toggle->keycode);
    state->is_toggled = !state->is_toggled;
    if (state->is_toggled) {
      state->since = timer_read();
      state->stage = TOGGLE_STAGE_TOGGLE;
    } else
      // If the key is toggled off, we use the normal key behavior.
      state->stage = TOGGLE_STAGE_NORMAL;
    break;

  case AK_EVENT_TYPE_RELEASE:
    if (!state->is_toggled)
      layout_unregister(event->key, toggle->keycode);
    state->stage = TOGGLE_STAGE_NONE;
    break;

  default:
    break;
  }
}

void advanced_key_init(void) {}

void advanced_key_clear(void) {
  // Release any keys that are currently pressed
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
    const advanced_key_state_t *state = &ak_states[i];

    switch (ak->type) {
    case AK_TYPE_TAP_HOLD:
      if (state->tap_hold.stage == TAP_HOLD_STAGE_HOLD)
        layout_unregister(ak->key, ak->tap_hold.hold_keycode);
      break;

    case AK_TYPE_TOGGLE:
      if (state->toggle.stage != TOGGLE_STAGE_NONE || state->toggle.is_toggled)
        layout_unregister(ak->key, ak->toggle.keycode);
      break;

    default:
      break;
    }
  }
  // Clear the advanced key states
  memset(ak_states, 0, sizeof(ak_states));
}

void advanced_key_process(const advanced_key_event_t *event) {
  if (event->ak_index >= NUM_ADVANCED_KEYS)
    return;

  switch (CURRENT_PROFILE.advanced_keys[event->ak_index].type) {
  case AK_TYPE_NULL_BIND:
    advanced_key_null_bind(event);
    break;

  case AK_TYPE_DYNAMIC_KEYSTROKE:
    advanced_key_dynamic_keystroke(event);
    break;

  case AK_TYPE_TAP_HOLD:
    advanced_key_tap_hold(event);
    break;

  case AK_TYPE_TOGGLE:
    advanced_key_toggle(event);
    break;

  default:
    break;
  }
}

void advanced_key_tick(bool has_non_tap_hold_press) {
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
    advanced_key_state_t *state = &ak_states[i];

    switch (ak->type) {
    case AK_TYPE_TAP_HOLD:
      if (state->tap_hold.stage == TAP_HOLD_STAGE_TAP &&
          // If hold on other key press is enabled, immediately register the
          // hold key when another non-Tap-Hold key is pressed.
          ((has_non_tap_hold_press & ak->tap_hold.hold_on_other_key_press) ||
           // Otherwise, the key must be held for the tapping term.
           timer_elapsed(state->tap_hold.since) >= ak->tap_hold.tapping_term)) {
        layout_register(ak->key, ak->tap_hold.hold_keycode);
        state->tap_hold.stage = TAP_HOLD_STAGE_HOLD;
      }
      break;

    case AK_TYPE_TOGGLE:
      if (state->toggle.stage == TOGGLE_STAGE_TOGGLE &&
          timer_elapsed(state->toggle.since) >= ak->toggle.tapping_term) {
        // If the key is held for more than the tapping term, switch to the
        // normal key behavior.
        state->toggle.stage = TOGGLE_STAGE_NORMAL;
        // Always toggle the key off when in normal behavior
        state->toggle.is_toggled = false;
      }
      break;

    default:
      break;
    }
  }
}

//--------------------------------------------------------------------+
// Combo Implementation
//--------------------------------------------------------------------+

#define COMBO_BUFFER_SIZE 8
#define DEFAULT_COMBO_TERM 50

typedef struct {
  uint8_t key;
  bool pressed;
  uint32_t time;
} combo_event_t;

static combo_event_t combo_buffer[COMBO_BUFFER_SIZE];
static uint8_t combo_buffer_count = 0;
static uint32_t combo_timer = 0;
static bool combo_active = false;
static uint8_t active_combo_idx = 255; // 255 = No active combo

static const advanced_key_t *get_combo(uint8_t idx) {
    if (idx >= NUM_ADVANCED_KEYS) return NULL;
    return &CURRENT_PROFILE.advanced_keys[idx];
}

static bool process_buffered_keys(void) {
  bool has_non_tap_hold_press = false;
  for (uint8_t i = 0; i < combo_buffer_count; i++) {
    if (layout_process_key(combo_buffer[i].key, combo_buffer[i].pressed))
      has_non_tap_hold_press = true;
  }
  combo_buffer_count = 0;
  combo_active = false;
  active_combo_idx = 255;
  return has_non_tap_hold_press;
}

bool advanced_key_combo_process(uint8_t key, bool pressed, uint32_t time) {
  // If a combo is already active
  if (active_combo_idx != 255) {
      const advanced_key_t *ak = get_combo(active_combo_idx);
      // Check if this key is part of the combo
      bool is_combo_part = false;
      for(int i=0; i<4; i++) {
          if (ak->combo.keys[i] == key) {
              is_combo_part = true;
              break;
          }
      }
      
      if (is_combo_part) {
          if (!pressed) {
              // Release the combo
              layout_unregister(255, ak->combo.output_keycode);
              active_combo_idx = 255;
              // We don't release the constituent keys as they were consumed by the combo
              
              // However, if there are other keys still held from the combo, we might need to handle them?
              // Simple logic: First release breaks the combo.
              // Remaining keys will be released naturally?
              // If A+B -> C. Release A -> Release C. B is still held.
              // When B is released later, it enters here.
              // active_combo_idx is 255. B is not part of active combo.
              // It goes to "buffer processing".
              // But B was already "consummed" physically.
              // If we forward Release B, and B was never Registered, it's fine (unregister is safe).
          }
          return true; // Consume event
      } else {
          // Key press not part of combo.
          // Allow it to pass through?
          // Yes.
          return false;
      }
  }

  // If buffer is full, process immediately (fallback)
  if (combo_buffer_count >= COMBO_BUFFER_SIZE) {
    process_buffered_keys();
    // Fall through to process current key normally?
    // No, current key should probably start a new buffer if pressed.
    // For simplicity, just return false (process immediately).
    return false;
  }

  if (!combo_active && pressed) {
    combo_active = true;
    combo_timer = time;
  }

  if (combo_active) {
    combo_buffer[combo_buffer_count++] = (combo_event_t){.key = key, .pressed = pressed, .time = time};
    
    const uint8_t current_layer = layout_get_current_layer();
    // Check for combos
    for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
        const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
        if (ak->type != AK_TYPE_COMBO) continue;
        if (ak->layer != current_layer) continue;
        
        // Check if all combo keys are pressed in the buffer
        // And ensure no defined keys are missing
        
        int match_count = 0;
        int required_count = 0;
        
        for (int k=0; k<4; k++) {
            uint8_t c_key = ak->combo.keys[k];
            // Assuming 0 is a valid key, we need a way to mark empty. 
            // In C initialization, 0 is often default.
            // But 0 is KC_A usually or similar? No, key index 0.
            // Let's assume keys are dense packed and we check all 4.
            // But user might only define 2 keys.
            // Since we can't easily distinguish "Key 0" from "Empty", 
            // maybe we should use a specific value or count?
            // HMK_MAX_NUM_KEYS is 256. key is uint8_t.
            // So we can't use 255 as empty if 255 is a valid key.
            // But typically keyboards have < 255 keys. MAX_KEYS define check?
            // checking common.h: NUM_KEYS <= 256. 
            // If NUM_KEYS < 255, we can use 255.
            // Safest: Check against NUM_KEYS.
            
            if (c_key < NUM_KEYS && c_key != 255) {
                required_count++;
                bool found = false;
                for(int j=0; j<combo_buffer_count; j++) {
                    if (combo_buffer[j].pressed && combo_buffer[j].key == c_key) {
                        found = true;
                        break;
                    }
                }
                if (found) match_count++;
            }
        }
        
        if (required_count > 0 && match_count == required_count) {
             // Match found!
             active_combo_idx = i;
             layout_register(255, ak->combo.output_keycode);
             
             // Clear buffer (events consumed)
             combo_buffer_count = 0;
             combo_active = false;
             return true; 
        }
    }
    
    return true; // Buffer and consume
  }

  return false;
}

bool advanced_key_combo_task(void) {
  if (combo_active && timer_elapsed(combo_timer) > DEFAULT_COMBO_TERM) {
    return process_buffered_keys();
  }
  return false;
}
