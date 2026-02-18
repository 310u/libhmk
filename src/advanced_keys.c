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
    state->interrupted = false;
    break;

  case AK_EVENT_TYPE_RELEASE:
    if (state->stage == TAP_HOLD_STAGE_TAP) {
      if (tap_hold->retro_tapping && !state->interrupted &&
          timer_elapsed(state->since) >= tap_hold->tapping_term) {
        // Retro Tapping: If the key is held longer than the tapping term, and
        // released without any other key being pressed, register the tap
        // action.
        deferred_action = (deferred_action_t){
            .type = DEFERRED_ACTION_TYPE_RELEASE,
            .key = event->key,
            .keycode = tap_hold->tap_keycode,
        };
        if (deferred_action_push(&deferred_action))
          layout_register(event->key, tap_hold->tap_keycode);
      } else {
        deferred_action = (deferred_action_t){
            .type = DEFERRED_ACTION_TYPE_RELEASE,
            .key = event->key,
            .keycode = tap_hold->tap_keycode,
        };
        if (deferred_action_push(&deferred_action))
          // We only perform the tap action if the release action was
          // successfully.
          layout_register(event->key, tap_hold->tap_keycode);
      }
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

void advanced_key_tick(bool has_non_tap_hold_press,
                       bool has_non_tap_hold_release) {
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
    advanced_key_state_t *state = &ak_states[i];

    switch (ak->type) {
    case AK_TYPE_TAP_HOLD:
      if (has_non_tap_hold_press)
        state->tap_hold.interrupted = true;

      if (state->tap_hold.stage == TAP_HOLD_STAGE_TAP &&
          // If hold on other key press is enabled, immediately register the
          // hold key when another non-Tap-Hold key is pressed.
          ((has_non_tap_hold_press & ak->tap_hold.hold_on_other_key_press) ||
           // If permissive hold is enabled, immediately register the hold key
           // when another non-Tap-Hold key is tapped (released).
           (has_non_tap_hold_release & ak->tap_hold.permissive_hold) ||
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

#define COMBO_QUEUE_SIZE 16
#define DEFAULT_COMBO_TERM 50
#define COMBO_KEY_NONE 255

typedef struct {
  uint8_t key;
  bool pressed;
  uint32_t time;
  // Whether the event has been consumed by a combo match
  bool consumed;
} combo_event_t;

// Event queue (Ring buffer)
static combo_event_t event_queue[COMBO_QUEUE_SIZE];
static uint8_t queue_head = 0; // Read index
static uint8_t queue_tail = 0; // Write index
static uint8_t queue_count = 0;

// Track if any activity occurred during processing (for layout task return)
static bool pending_activity = false;

// Bitmap cache: bit N is set if key N is part of any combo on the current layer.
// Rebuilt lazily when the layer changes.
static uint8_t combo_key_bitmap[(NUM_KEYS + 7) / 8];
static uint8_t combo_key_bitmap_layer = 255; // Invalid initial value to force rebuild

static void combo_key_bitmap_rebuild(uint8_t layer) {
  if (combo_key_bitmap_layer == layer) return;
  
  memset(combo_key_bitmap, 0, sizeof(combo_key_bitmap));
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
    if (ak->type != AK_TYPE_COMBO || ak->layer != layer) continue;
    for (int k = 0; k < 4; k++) {
      if (ak->combo.keys[k] < NUM_KEYS) {
        combo_key_bitmap[ak->combo.keys[k] / 8] |= (uint8_t)(1 << (ak->combo.keys[k] % 8));
      }
    }
  }
  combo_key_bitmap_layer = layer;
}

static inline bool is_key_in_any_combo(uint8_t key) {
  if (key >= NUM_KEYS) return false;
  return (combo_key_bitmap[key / 8] & (1 << (key % 8))) != 0;
}

static void flush_events(uint8_t count_to_flush);

// Recursion guard for flush_events
static bool flush_in_progress = false;

static void queue_push(uint8_t key, bool pressed, uint32_t time) {
  if (queue_count >= COMBO_QUEUE_SIZE) {
    // Queue full: Force flush the oldest event to make room
    flush_events(1);
    // If flush failed (e.g. recursion guard or logic error), we might still be full.
    // In this simple implementation, flush_events always pops, so space is guaranteed.
  }
  
  event_queue[queue_tail] = (combo_event_t){
      .key = key,
      .pressed = pressed,
      .time = time,
      .consumed = false,
  };
  queue_tail = (queue_tail + 1) % COMBO_QUEUE_SIZE;
  queue_count++;
}

static combo_event_t *queue_peek(uint8_t offset) {
  if (offset >= queue_count) return NULL;
  return &event_queue[(queue_head + offset) % COMBO_QUEUE_SIZE];
}

static void queue_pop(void) {
  if (queue_count == 0) return;
  queue_head = (queue_head + 1) % COMBO_QUEUE_SIZE;
  queue_count--;
}

// Flush unconsumed events up to a certain point.
// 
// SAFETY: The recursion guard (flush_in_progress) may cause a flush call to
// be skipped if re-entered (e.g. queue_push overflow during flush).
// This is safe because skipped events remain in the queue and will be
// processed on the next advanced_key_combo_task() tick or the next key
// event, so no input is permanently lost.
static void flush_events(uint8_t count_to_flush) {
  if (flush_in_progress) return; // Prevent recursion (events stay in queue)
  flush_in_progress = true;
  
  for (uint8_t i = 0; i < count_to_flush && queue_count > 0; i++) {
    combo_event_t *ev = queue_peek(0);
    
    if (!ev->consumed) {
       if (layout_process_key(ev->key, ev->pressed)) {
           pending_activity = true;
       }
    }
    
    queue_pop();
  }
  
  flush_in_progress = false;
}



// Evaluate status of a specific combo definition against the buffer
// Only press events in the queue are considered for combo matching.
// Returns: 
// 0 = No match
// 1 = Candidate (Partial match, still possible)
// 2 = Full Match (All keys pressed within term)
static int check_combo_match(const advanced_key_t *ak, uint32_t current_time) {
  int keys_found = 0;
  int keys_required = 0;
  bool active_part[4] = {0};
  uint32_t key_times[4] = {0};
  
  // Count required keys
  for (int k = 0; k < 4; k++) {
      if (ak->combo.keys[k] != COMBO_KEY_NONE && ak->combo.keys[k] < NUM_KEYS) {
          keys_required++;
      }
  }
  if (keys_required == 0) return 0;

  for (uint8_t i = 0; i < queue_count; i++) {
    combo_event_t *ev = queue_peek(i);
    if (!ev || ev->consumed) continue; 
    
    // Release events are not queued (handled immediately in combo_process),
    // but skip them defensively if present.
    if (!ev->pressed) continue;
    
    bool is_part = false;
    for (int k = 0; k < 4; k++) {
        if (ak->combo.keys[k] == ev->key) {
            is_part = true;
            if (!active_part[k]) {
               active_part[k] = true;
               key_times[k] = ev->time;
            }
            break;
        }
    }
    
    if (!is_part) {
        // Foreign key press detected in the queue
        // This interrupts the combo candidate
        return 0;
    }
  }
  
  keys_found = 0;
  for (int k=0; k<4; k++) {
      if (ak->combo.keys[k] < NUM_KEYS && active_part[k]) keys_found++;
  }
  
  uint16_t term = ak->combo.term > 0 ? ak->combo.term : DEFAULT_COMBO_TERM;

  if (keys_found == keys_required) {
      uint32_t min_t = 0;
      uint32_t max_t = 0;
      bool first = true;
      for (int k=0; k<4; k++) {
           if (active_part[k]) {
               if (first) { min_t = max_t = key_times[k]; first = false; }
               else {
                   if (key_times[k] < min_t) min_t = key_times[k];
                   if (key_times[k] > max_t) max_t = key_times[k];
               }
           }
      }
       
      if ((max_t - min_t) <= term) {
           return 2; // Full Match
      }
      return 0; // Timing failed
  }
  
  if (keys_found > 0) {
      uint32_t min_t = 0;
      bool first = true;
      for (int k=0; k<4; k++) {
          if (active_part[k]) {
             if (first) { min_t = key_times[k]; first = false; }
             else if (key_times[k] < min_t) min_t = key_times[k];
          }
      }
      
      if (!first && (current_time - min_t) <= term) {
          return 1; // Candidate
      }
  }
  
  return 0;
}

static void process_combo_logic(uint32_t current_time) {
  const uint8_t current_layer = layout_get_current_layer();
  combo_key_bitmap_rebuild(current_layer);
  
  int best_match_idx = -1;
  int best_match_len = 0;
  bool pending_candidates = false;
  uint16_t max_pending_term = DEFAULT_COMBO_TERM;
  
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
    if (ak->type != AK_TYPE_COMBO || ak->layer != current_layer)
      continue;
      
    int status = check_combo_match(ak, current_time);
    
    if (status == 2) { 
        int len = 0;
        for(int k=0; k<4; k++) if(ak->combo.keys[k] < NUM_KEYS) len++;
        
        if (len > best_match_len) {
            best_match_idx = (int)i;
            best_match_len = len;
        } else if (len == best_match_len) {
            if (best_match_idx == -1 || (int)i < best_match_idx) {
                best_match_idx = (int)i;
                best_match_len = len;
            }
        }
    } else if (status == 1) {
        pending_candidates = true;
        uint16_t term = ak->combo.term > 0 ? ak->combo.term : DEFAULT_COMBO_TERM;
        if (term > max_pending_term) max_pending_term = term;
    }
  }
  
  if (best_match_idx != -1) {
      if (pending_candidates) {
          combo_event_t *head = queue_peek(0);
          if (head && (current_time - head->time) > max_pending_term) {
             goto execute_match;
          }
          // Still waiting for potential longer combo
          return; 
      }
      
      execute_match:
      {
          const advanced_key_t *match_ak = &CURRENT_PROFILE.advanced_keys[best_match_idx];
          
          for (uint8_t q = 0; q < queue_count; q++) {
              combo_event_t *ev = queue_peek(q);
              if (!ev || ev->consumed) continue;
              
              for (int k=0; k<4; k++) {
                  if (match_ak->combo.keys[k] == ev->key) {
                      ev->consumed = true;
                      break; 
                  }
              }
          }
          
          layout_register(255, match_ak->combo.output_keycode);
          
          deferred_action_t da = {
              .type = DEFERRED_ACTION_TYPE_RELEASE,
              .key = 255,
              .keycode = match_ak->combo.output_keycode,
          };
          deferred_action_push(&da);

          pending_activity = true;
          
          flush_events(queue_count);
      }
  } else {
      if (pending_candidates) {
          combo_event_t *head = queue_peek(0);
          if (head && (current_time - head->time) > max_pending_term) {
             // Timed out: flush one event at a time
             flush_events(1);
          }
          // Else: still waiting
      } else {
          // No match, no candidates: flush everything immediately
          flush_events(queue_count);
      }
  }
}

bool advanced_key_combo_process(uint8_t key, bool pressed, uint32_t time) {
  const uint8_t current_layer = layout_get_current_layer();
  combo_key_bitmap_rebuild(current_layer);
  
  bool key_in_combo = is_key_in_any_combo(key);
  
  // Fast path: If queue is empty and this key is not in any combo, pass through
  if (queue_count == 0 && !key_in_combo) {
      return false; 
  }

  // ── Non-combo key handling ──
  // Press: flush all pending events, then let this key pass through normally.
  // Release: always pass through (its press was already handled normally).
  if (!key_in_combo) {
      if (pressed && queue_count > 0) {
          flush_events(queue_count);
      }
      return false; // Let caller process this key normally
  }

  // ── Release of a combo key ──
  // Don't queue release events. Pass them through immediately
  // so key-up reports are not delayed. Then re-evaluate combo state:
  // the release may have killed all candidates, triggering an
  // immediate flush inside process_combo_logic.
  if (!pressed) {
      if (layout_process_key(key, false)) {
          pending_activity = true;
      }
      // Re-evaluate: release may have killed all candidates
      if (queue_count > 0) {
          process_combo_logic(time);
      }
      return true; // We handled this event
  }

  // ── Press of a combo key: queue it ──
  queue_push(key, pressed, time);
  process_combo_logic(time);
  
  return true;
}

bool advanced_key_combo_task(void) {
  pending_activity = false;
  
  if (queue_count > 0) {
      process_combo_logic(timer_read());
  }
  
  return pending_activity;
}

// Called when layer changes or profile reloads to invalidate the bitmap cache
void advanced_key_combo_invalidate_cache(void) {
  combo_key_bitmap_layer = 255;
}
