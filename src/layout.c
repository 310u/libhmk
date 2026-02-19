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

#include "layout.h"

#include "advanced_keys.h"
#include "deferred_actions.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "keycodes.h"
#include "lib/bitmap.h"
#include "matrix.h"
#include "xinput.h"

// Layer mask. Each bit represents whether a layer is active or not.
static uint16_t layer_mask;
static uint8_t default_layer;

/**
 * @brief Get the current layer
 *
 * The current layer is the highest layer that is currently active in the layer
 * mask. If no layers are active, the default layer is returned.
 *
 * @return Current layer
 */
uint8_t layout_get_current_layer(void) {
  return layer_mask ? 31 - __builtin_clz(layer_mask) : default_layer;
}

__attribute__((always_inline)) static inline void
layout_layer_on(uint8_t layer) {
  layer_mask |= (1 << layer);
}

__attribute__((always_inline)) static inline void
layout_layer_off(uint8_t layer) {
  layer_mask &= ~(1 << layer);
}

/**
 * @brief Lock the current layer
 *
 * This function sets the default layer to the current layer if it is not
 * already set. Otherwise, it sets the default layer to 0.
 *
 * @return None
 */
__attribute__((always_inline)) static inline void layout_layer_lock(void) {
  const uint8_t current_layer = layout_get_current_layer();
  default_layer = current_layer == default_layer ? 0 : current_layer;
}

/**
 * @brief Get the keycode of a key
 *
 * This function returns the keycode of a key in the current layer. If the key
 * is transparent, the function will search for the highest active layer with a
 * non-transparent keycode.
 *
 * @param current_layer Current layer
 * @param key Key index
 *
 * @return Keycode
 */
uint8_t layout_get_keycode(uint8_t current_layer, uint8_t key) {
  // Find the first active layer with a non-transparent keycode
  for (uint32_t i = (uint32_t)current_layer + 1; i-- > 0;) {
    if (((layer_mask >> i) & 1) == 0)
      // Layer is not active
      continue;

    const uint8_t keycode = CURRENT_PROFILE.keymap[i][key];
    if (keycode != KC_TRANSPARENT)
      return keycode;
  }

  // No keycode found, use the default keycode
  return CURRENT_PROFILE.keymap[default_layer][key];
}

// Only send reports if they changed
static bool should_send_reports;
// Whether the key is disabled by `SP_KEY_LOCK`
static bitmap_t key_disabled[] = MAKE_BITMAP(NUM_KEYS);

// Track whether the key is currently pressed. Used to detect key events.
static bitmap_t key_press_states[] = MAKE_BITMAP(NUM_KEYS);
// Store the keycodes of the currently pressed keys. Layer/profile may change so
// we need to remember the keycodes we pressed to release them correctly.
static uint8_t active_keycodes[NUM_KEYS];

// Store the indices of the advanced keys bind to each key. If no advanced key
// is bind to a key, the index is 0. Otherwise, the index is added by 1.
static uint8_t advanced_key_indices[NUM_LAYERS][NUM_KEYS];
// Same as `active_keycodes` but for advanced keys
static uint8_t active_advanced_keys[NUM_KEYS];

// Pending events buffer for hold-tap input buffering.
// When a hold-tap key is undecided, non-hold-tap key events are buffered
// here and replayed after the hold-tap resolves.
#define MAX_PENDING_EVENTS 8
static struct {
  uint8_t key;
  bool pressed;
} pending_events[MAX_PENDING_EVENTS];
static uint8_t pending_count;

void layout_init(void) { layout_load_advanced_keys(); }

/**
 * @brief Reload advanced key indices from the current profile.
 *
 * DESIGN INVARIANT: All code paths that modify CURRENT_PROFILE.advanced_keys
 * (profile switch, reset, duplicate, hmkconf update) MUST call this function.
 * This is the sole gateway for config changes that affect combo bitmap cache.
 * If a new config-change path is added without calling this function,
 * the combo bitmap cache will become stale and produce incorrect behavior.
 */
void layout_load_advanced_keys(void) {
  memset(advanced_key_indices, 0, sizeof(advanced_key_indices));
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];

    if (ak->type == AK_TYPE_NONE || ak->type == AK_TYPE_COMBO ||
        ak->layer >= NUM_LAYERS || ak->key >= NUM_KEYS)
      continue;

    advanced_key_indices[ak->layer][ak->key] = i + 1;
    if (ak->type == AK_TYPE_NULL_BIND && ak->null_bind.secondary_key < NUM_KEYS)
      // Null Bind advanced keys also have a secondary key
      advanced_key_indices[ak->layer][ak->null_bind.secondary_key] = i + 1;
  }
  
  // Invalidate combo bitmap cache so it's rebuilt with updated definitions.
  // Layer changes are handled lazily by combo_key_bitmap_rebuild() checking
  // combo_key_bitmap_layer != current_layer. This invalidation covers the
  // case where definitions change but the layer stays the same.
  advanced_key_combo_invalidate_cache();
}

bool layout_process_key(uint8_t key, bool pressed) {
  const uint8_t current_layer = layout_get_current_layer();
  bool has_non_tap_hold_press = false;

  if (pressed) {
    const uint8_t keycode = layout_get_keycode(current_layer, key);
    const uint8_t ak_index = advanced_key_indices[current_layer][key];

    if (ak_index) {
      active_advanced_keys[key] = ak_index;
      advanced_key_event_t ak_event = (advanced_key_event_t){
          .type = AK_EVENT_TYPE_PRESS,
          .key = key,
          .keycode = keycode,
          .ak_index = ak_index - 1,
      };
      advanced_key_process(&ak_event);
      has_non_tap_hold_press |=
          (CURRENT_PROFILE.advanced_keys[ak_index - 1].type !=
           AK_TYPE_TAP_HOLD);
    } else {
      active_keycodes[key] = keycode;
      layout_register(key, keycode);
      has_non_tap_hold_press |= (keycode != KC_NO);
      // Update last key time for require_prior_idle_ms feature
      if (keycode != KC_NO)
        advanced_key_update_last_key_time(timer_read());
    }
  } else {
    const uint8_t keycode = active_keycodes[key];
    const uint8_t ak_index = active_advanced_keys[key];

    if (ak_index) {
      active_advanced_keys[key] = 0;
      advanced_key_event_t ak_event = (advanced_key_event_t){
          .type = AK_EVENT_TYPE_RELEASE,
          .key = key,
          .keycode = keycode,
          .ak_index = ak_index - 1,
      };
      advanced_key_process(&ak_event);
    } else {
      active_keycodes[key] = KC_NO;
      layout_unregister(key, keycode);
    }
  }

  return has_non_tap_hold_press;
}

void layout_task(void) {
  static uint32_t last_ak_tick = 0;

  const uint8_t current_layer = layout_get_current_layer();
  bool has_non_tap_hold_press = false;
  bool has_non_tap_hold_release = false;

  // Buffer for key press/release events to be sorted by event_time
  struct {
    uint8_t key;
    bool pressed;
    uint32_t event_time;
  } events[NUM_KEYS];
  uint8_t event_count = 0;

  // First pass: collect key events and process XInput/hold events
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    const key_state_t *k = &key_matrix[i];
    const bool last_key_press = bitmap_get(key_press_states, i);

    if ((current_layer == 0) & eeconfig->options.xinput_enabled) {
      // XInput key only applies to layer 0. We process it first since the
      // subsequent key processing may be skipped due to the gamepad options.
      if (CURRENT_PROFILE.gamepad_buttons[i] != GP_BUTTON_NONE) {
        xinput_process(i);

        if (CURRENT_PROFILE.gamepad_options.gamepad_override) {
          // If the key is mapped to a gamepad button, and the gamepad override
          // is enabled, we skip the key processing.
          bitmap_set(key_press_states, i, k->is_pressed);
          continue;
        }
      }

      if (!CURRENT_PROFILE.gamepad_options.keyboard_enabled) {
        // If the keyboard is disabled for this profile, we skip the key
        // processing.
        bitmap_set(key_press_states, i, k->is_pressed);
        continue;
      }
    }

    if ((current_layer == 0) & bitmap_get(key_disabled, i)) {
      // Only keys in layer 0 can be disabled.
      bitmap_set(key_press_states, i, k->is_pressed);
      continue;
    }

    if (k->is_pressed & !last_key_press) {
      // Key press event: buffer for sorted processing
      events[event_count++] = (typeof(events[0])){
          .key = i, .pressed = true, .event_time = k->event_time};
    } else if (!k->is_pressed & last_key_press) {
      // Key release event: buffer for sorted processing
      events[event_count++] = (typeof(events[0])){
          .key = i, .pressed = false, .event_time = k->event_time};
    } else if (k->is_pressed) {
      // Key hold event: process immediately (ordering doesn't matter)
      const uint8_t keycode = active_keycodes[i];
      const uint8_t ak_index = active_advanced_keys[i];

      if (ak_index) {
        advanced_key_event_t ak_event = (advanced_key_event_t){
            .type = AK_EVENT_TYPE_HOLD,
            .key = i,
            .keycode = keycode,
            .ak_index = ak_index - 1,
        };
        advanced_key_process(&ak_event);
      }
    }
  }

  // Sort events by event_time (insertion sort, small N)
  for (uint8_t i = 1; i < event_count; i++) {
    typeof(events[0]) tmp = events[i];
    uint8_t j = i;
    while (j > 0 && events[j - 1].event_time > tmp.event_time) {
      events[j] = events[j - 1];
      j--;
    }
    events[j] = tmp;
  }

  // Process events in chronological order
  for (uint8_t i = 0; i < event_count; i++) {
    const uint8_t key = events[i].key;
    const bool pressed = events[i].pressed;

    if (pressed) {
      if (advanced_key_combo_process(key, true, events[i].event_time))
        goto update_event_state;

      // Check if this is a non-hold-tap key and any hold-tap is undecided.
      // If so, buffer the event instead of processing it immediately.
      // This prevents keys from being registered before a modifier
      // (hold) is resolved.
      const uint8_t current_layer_for_key = layout_get_current_layer();
      const uint8_t ak_idx =
          advanced_key_indices[current_layer_for_key][key];
      const bool is_hold_tap =
          ak_idx && CURRENT_PROFILE.advanced_keys[ak_idx - 1].type ==
                        AK_TYPE_TAP_HOLD;

      if (!is_hold_tap && advanced_key_has_undecided() &&
          pending_count < MAX_PENDING_EVENTS) {
        pending_events[pending_count++] = (typeof(pending_events[0])){
            .key = key, .pressed = true};
        goto update_event_state;
      }

      if (layout_process_key(key, true))
        has_non_tap_hold_press = true;
    } else {
      if (advanced_key_combo_process(key, false, events[i].event_time))
        goto update_event_state;

      if (layout_process_key(key, false))
        has_non_tap_hold_release = true;
    }

  update_event_state:
    bitmap_set(key_press_states, key, key_matrix[key].is_pressed);
  }

  if (advanced_key_combo_task())
    has_non_tap_hold_press = true;

  if (has_non_tap_hold_press || timer_elapsed(last_ak_tick) > 0) {
    // We only need to tick the advanced keys every 1ms, or when there is a
    // non-Tap-Hold key press event since these are the only cases that
    // the advanced keys might perform an action.
    advanced_key_tick(has_non_tap_hold_press, has_non_tap_hold_release);
    last_ak_tick = timer_read();
  }

  // After tick, if no hold-tap is undecided anymore, flush pending events
  if (pending_count > 0 && !advanced_key_has_undecided()) {
    for (uint8_t i = 0; i < pending_count; i++) {
      if (pending_events[i].pressed)
        layout_process_key(pending_events[i].key, true);
      else
        layout_process_key(pending_events[i].key, false);
    }
    pending_count = 0;
  }

  if (should_send_reports) {
    hid_send_reports();
    should_send_reports = false;
  }

  // Process deferred actions for the next matrix scan
  deferred_action_process();
}

/**
 * @brief Set the current profile
 *
 * This function also refreshes the advanced keys, and saves the last
 * non-default profile for profile swapping.
 *
 * @param profile Profile index
 *
 * @return true if successful, false otherwise
 */
static bool layout_set_profile(uint8_t profile) {
  if (profile >= NUM_PROFILES)
    return false;

  advanced_key_clear();
  bool status = EECONFIG_WRITE(current_profile, &profile);
  if (status && profile != 0)
    status = EECONFIG_WRITE(last_non_default_profile, &profile);
  layout_load_advanced_keys();

  return status;
}

void layout_register(uint8_t key, uint8_t keycode) {
  if (keycode == KC_NO)
    return;

  switch (keycode) {
  case HID_KEYCODE_RANGE:
    hid_keycode_add(keycode);
    should_send_reports = true;
    break;

  case MOMENTARY_LAYER_RANGE:
    layout_layer_on(MO_GET_LAYER(keycode));
    break;

  case PROFILE_RANGE:
    layout_set_profile(PF_GET_PROFILE(keycode));
    break;

  case SP_KEY_LOCK:
    bitmap_toggle(key_disabled, key);
    break;

  case SP_LAYER_LOCK:
    layout_layer_lock();
    break;

  case SP_PROFILE_SWAP:
    layout_set_profile(
        eeconfig->current_profile ? 0 : eeconfig->last_non_default_profile);
    break;

  case SP_PROFILE_NEXT:
    layout_set_profile((eeconfig->current_profile + 1) % NUM_PROFILES);
    break;

  case SP_BOOT:
    board_enter_bootloader();
    break;

  default:
    break;
  }
}

void layout_unregister(uint8_t key, uint8_t keycode) {
  if (keycode == KC_NO)
    return;

  switch (keycode) {
  case HID_KEYCODE_RANGE:
    hid_keycode_remove(keycode);
    should_send_reports = true;
    break;

  case MOMENTARY_LAYER_RANGE:
    layout_layer_off(MO_GET_LAYER(keycode));
    break;

  default:
    break;
  }
}
