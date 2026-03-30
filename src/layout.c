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
#include "event_trace.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "joystick.h"
#include "keycodes.h"
#include "lib/bitmap.h"
#include "matrix.h"
#include "profile_runtime.h"
#include "rgb.h"
#include "xinput.h"

// Layer mask. Each bit represents whether a layer is active or not.
static uint16_t layer_mask;
static uint8_t default_layer;
typedef uint16_t layout_event_count_t;

#if defined(RGB_ENABLED)
#define RGB_BRIGHTNESS_STEP 17
#endif

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
  if (layer < sizeof(layer_mask) * 8u) {
    layer_mask |= (uint16_t)(1u << layer);
  }
}

__attribute__((always_inline)) static inline void
layout_layer_off(uint8_t layer) {
  if (layer < sizeof(layer_mask) * 8u) {
    layer_mask &= (uint16_t)~(uint16_t)(1u << layer);
  }
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

// Whether the key is disabled by `SP_KEY_LOCK`
static bitmap_t key_disabled[BITMAP_SIZE(NUM_KEYS)] = {0};

// Track whether the key is currently pressed. Used to detect key events.
static bitmap_t key_press_states[BITMAP_SIZE(NUM_KEYS)] = {0};
// Store the keycodes of the currently pressed keys. Layer/profile may change so
// we need to remember the keycodes we pressed to release them correctly.
static uint8_t active_keycodes[NUM_KEYS];

// Store the indices of the advanced keys bind to each key. If no advanced key
// is bind to a key, the index is 0. Otherwise, the index is added by 1.
static uint8_t advanced_key_indices[NUM_LAYERS][NUM_KEYS];
// Same as `active_keycodes` but for advanced keys
static uint8_t active_advanced_keys[NUM_KEYS];

typedef struct {
  uint8_t key;
  bool pressed;
  uint32_t event_time;
  uint8_t distance;
} layout_event_t;

#if defined(DEBUG_EVENT_TRACE)
static void layout_trace_events(const char *stage, const layout_event_t *events,
                                layout_event_count_t event_count) {
  if (event_count == 0)
    return;

  EVENT_TRACE("[event] %s count=%u\n", stage, event_count);
  for (layout_event_count_t i = 0; i < event_count; i++) {
    EVENT_TRACE(
        "[event] %s[%u] key=%u action=%s time=%lu distance=%u\n", stage,
        (unsigned int)i, events[i].key,
        events[i].pressed ? "press" : "release",
        (unsigned long)events[i].event_time, events[i].distance);
  }
}
#else
#define layout_trace_events(stage, events, event_count) ((void)0)
#endif

// Pending events buffer for hold-tap input buffering.
// When a hold-tap key is undecided, non-hold-tap key events are buffered
// here and replayed after the hold-tap resolves.
#define MAX_PENDING_EVENTS 32
static struct {
  uint8_t key;
  bool pressed;
} pending_events[MAX_PENDING_EVENTS];
static uint8_t pending_count;

bool is_sniper_active = false;

#if defined(JOYSTICK_ENABLED)
static uint8_t joystick_scroll_mo_depth;
static uint8_t joystick_scroll_mo_restore_mode;

static void joystick_scroll_mo_register(void) {
  joystick_config_t jc = joystick_get_config();

  if (joystick_scroll_mo_depth == 0) {
    joystick_scroll_mo_restore_mode = jc.mode;
    if (jc.mode != JOYSTICK_MODE_SCROLL) {
      jc.mode = JOYSTICK_MODE_SCROLL;
      joystick_apply_config(jc);
    }
  }

  if (joystick_scroll_mo_depth < UINT8_MAX)
    joystick_scroll_mo_depth++;
}

static void joystick_scroll_mo_unregister(void) {
  if (joystick_scroll_mo_depth == 0)
    return;

  joystick_scroll_mo_depth--;
  if (joystick_scroll_mo_depth == 0) {
    joystick_config_t jc = joystick_get_config();
    if (jc.mode != joystick_scroll_mo_restore_mode) {
      jc.mode = joystick_scroll_mo_restore_mode;
      joystick_apply_config(jc);
    }
  }
}
#endif

#if defined(RGB_ENABLED)
static bool layout_write_current_profile_rgb_field(uint32_t field_offset,
                                                   const void *value,
                                                   uint32_t len) {
  const uint32_t addr =
      offsetof(eeconfig_t, profiles) +
      (uint32_t)eeconfig->current_profile * sizeof(eeconfig_profile_t) +
      offsetof(eeconfig_profile_t, rgb_config) + field_offset;
  return wear_leveling_write(addr, value, len);
}

static void layout_set_rgb_enabled(bool enabled) {
  const uint8_t value = enabled ? 1 : 0;
  if (!layout_write_current_profile_rgb_field(offsetof(rgb_config_t, enabled),
                                              &value, sizeof(value)))
    return;

  rgb_get_config()->enabled = value;
  rgb_apply_config();
}

static void layout_adjust_rgb_brightness(bool increase) {
  const rgb_config_t *current_config = rgb_get_config();
  const uint16_t current_brightness = current_config->global_brightness;
  uint8_t next_brightness = (uint8_t)current_brightness;

  if (increase) {
    if (current_brightness >= UINT8_MAX)
      return;
    next_brightness =
        (uint8_t)M_MIN((uint16_t)UINT8_MAX,
                       current_brightness + (uint16_t)RGB_BRIGHTNESS_STEP);
  } else {
    if (current_brightness == 0)
      return;
    next_brightness = current_brightness > RGB_BRIGHTNESS_STEP
                          ? (uint8_t)(current_brightness - RGB_BRIGHTNESS_STEP)
                          : 0;
  }

  if (!layout_write_current_profile_rgb_field(
          offsetof(rgb_config_t, global_brightness), &next_brightness,
          sizeof(next_brightness)))
    return;

  rgb_get_config()->global_brightness = next_brightness;
  rgb_apply_config();
}

static void layout_cycle_rgb_effect(bool forward) {
  uint8_t next_effect = rgb_get_config()->current_effect;

  if (RGB_EFFECT_MAX <= RGB_EFFECT_SOLID_COLOR)
    return;

  if (next_effect <= RGB_EFFECT_OFF || next_effect >= RGB_EFFECT_MAX) {
    next_effect = forward ? RGB_EFFECT_SOLID_COLOR : (uint8_t)(RGB_EFFECT_MAX - 1);
  } else if (forward) {
    next_effect++;
    if (next_effect >= RGB_EFFECT_MAX)
      next_effect = RGB_EFFECT_SOLID_COLOR;
  } else {
    next_effect = next_effect > RGB_EFFECT_SOLID_COLOR
                      ? (uint8_t)(next_effect - 1)
                      : (uint8_t)(RGB_EFFECT_MAX - 1);
  }

  if (!layout_write_current_profile_rgb_field(
          offsetof(rgb_config_t, current_effect), &next_effect,
          sizeof(next_effect)))
    return;

  rgb_get_config()->current_effect = next_effect;
  rgb_apply_config();
}
#endif

static void layout_toggle_joystick_mode(void) {
#if defined(JOYSTICK_ENABLED)
  joystick_config_t jc = joystick_get_config();
  if (jc.mode == JOYSTICK_MODE_SCROLL) {
    jc.mode = JOYSTICK_MODE_MOUSE;
  } else {
    jc.mode = JOYSTICK_MODE_SCROLL;
  }
  joystick_set_config(jc);
#endif
}

static void layout_select_next_joystick_preset(void) {
#if defined(JOYSTICK_ENABLED)
  joystick_config_t jc = joystick_get_config();
  joystick_select_mouse_preset(
      &jc,
      (uint8_t)((jc.active_mouse_preset + 1u) % JOYSTICK_MOUSE_PRESET_COUNT));
  joystick_set_config(jc);
#endif
}

static void layout_register_joystick_scroll_mode(void) {
#if defined(JOYSTICK_ENABLED)
  joystick_scroll_mo_register();
#endif
}

static void layout_unregister_joystick_scroll_mode(void) {
#if defined(JOYSTICK_ENABLED)
  joystick_scroll_mo_unregister();
#endif
}

static void layout_toggle_rgb(void) {
#if defined(RGB_ENABLED)
  layout_set_rgb_enabled(!rgb_get_config()->enabled);
#endif
}

static void layout_increase_rgb_brightness(void) {
#if defined(RGB_ENABLED)
  layout_adjust_rgb_brightness(true);
#endif
}

static void layout_decrease_rgb_brightness(void) {
#if defined(RGB_ENABLED)
  layout_adjust_rgb_brightness(false);
#endif
}

static void layout_select_next_rgb_effect(void) {
#if defined(RGB_ENABLED)
  layout_cycle_rgb_effect(true);
#endif
}

static void layout_select_previous_rgb_effect(void) {
#if defined(RGB_ENABLED)
  layout_cycle_rgb_effect(false);
#endif
}

static void layout_toggle_polling_rate(void) {
  eeconfig_options_t options = eeconfig->options;
  options.high_polling_rate_enabled = !options.high_polling_rate_enabled;
  if (EECONFIG_WRITE(options, &options))
    board_reset();
}

void layout_init(void) { profile_runtime_apply_current(); }

void layout_reset_runtime_state(void) {
  advanced_key_clear();
  deferred_action_clear();
  hid_clear_runtime_state();
  xinput_reset_runtime_state();

  memset(active_keycodes, KC_NO, sizeof(active_keycodes));
  memset(active_advanced_keys, 0, sizeof(active_advanced_keys));
  pending_count = 0;

  layer_mask = 0;
  default_layer = 0;
  is_sniper_active = false;

#if defined(JOYSTICK_ENABLED)
  joystick_scroll_mo_depth = 0;
  joystick_scroll_mo_restore_mode = 0;
  joystick_config_t joystick_config;
  memcpy(&joystick_config, &CURRENT_PROFILE.joystick_config,
         sizeof(joystick_config));
  joystick_apply_config(joystick_config);
#endif

  for (uint32_t i = 0; i < NUM_KEYS; i++)
    bitmap_set(key_press_states, i, key_matrix[i].is_pressed);
}

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
  bool has_non_tap_hold_event = false;

  if (pressed) {
    // Abort playing macros whenever a new key is pressed
    advanced_key_abort_macros();

    const uint8_t keycode = layout_get_keycode(current_layer, key);
    const uint8_t ak_index = advanced_key_indices[current_layer][key];

    if (ak_index) {
      EVENT_TRACE(
          "[event] process key=%u action=press layer=%u keycode=%u advanced=%u\n",
          key, current_layer, keycode, ak_index - 1);
      active_advanced_keys[key] = ak_index;
      advanced_key_event_t ak_event = (advanced_key_event_t){
          .type = AK_EVENT_TYPE_PRESS,
          .key = key,
          .keycode = keycode,
          .ak_index = ak_index - 1,
      };
      advanced_key_process(&ak_event);
      has_non_tap_hold_event |=
          (CURRENT_PROFILE.advanced_keys[ak_index - 1].type !=
           AK_TYPE_TAP_HOLD);
    } else {
      EVENT_TRACE(
          "[event] process key=%u action=press layer=%u keycode=%u advanced=none\n",
          key, current_layer, keycode);
      active_keycodes[key] = keycode;
      layout_register(key, keycode);
      has_non_tap_hold_event |= (keycode != KC_NO);
      // Update last key time for require_prior_idle_ms feature
      if (keycode != KC_NO)
        advanced_key_update_last_key_time(timer_read());
    }
  } else {
    const uint8_t keycode = active_keycodes[key];
    const uint8_t ak_index = active_advanced_keys[key];

    if (ak_index) {
      EVENT_TRACE(
          "[event] process key=%u action=release layer=%u keycode=%u advanced=%u\n",
          key, current_layer, keycode, ak_index - 1);
      active_advanced_keys[key] = 0;
      advanced_key_event_t ak_event = (advanced_key_event_t){
          .type = AK_EVENT_TYPE_RELEASE,
          .key = key,
          .keycode = keycode,
          .ak_index = ak_index - 1,
      };
      advanced_key_process(&ak_event);
      has_non_tap_hold_event |=
          (CURRENT_PROFILE.advanced_keys[ak_index - 1].type !=
           AK_TYPE_TAP_HOLD);
    } else {
      EVENT_TRACE(
          "[event] process key=%u action=release layer=%u keycode=%u "
          "advanced=none\n",
          key, current_layer, keycode);
      active_keycodes[key] = KC_NO;
      layout_unregister(key, keycode);
      has_non_tap_hold_event |= (keycode != KC_NO);
    }
  }

  return has_non_tap_hold_event;
}

static void layout_flush_pending_events(void) {
  EVENT_TRACE("[event] pending flush count=%u\n", pending_count);
  for (uint8_t i = 0; i < pending_count; i++)
    EVENT_TRACE("[event] pending flush[%u] key=%u action=%s\n",
                (unsigned int)i, pending_events[i].key,
                pending_events[i].pressed ? "press" : "release");
  for (uint8_t i = 0; i < pending_count; i++)
    layout_process_key(pending_events[i].key, pending_events[i].pressed);
  pending_count = 0;
}

static void layout_buffer_pending_event(uint8_t key, bool pressed) {
  if (pending_count >= MAX_PENDING_EVENTS)
    layout_flush_pending_events();

  pending_events[pending_count++] =
      (typeof(pending_events[0])){.key = key, .pressed = pressed};
  EVENT_TRACE("[event] pending enqueue key=%u action=%s size=%u\n", key,
              pressed ? "press" : "release", pending_count);
}

static bool layout_pending_has_press(uint8_t key) {
  for (uint8_t i = 0; i < pending_count; i++) {
    if (pending_events[i].key == key && pending_events[i].pressed)
      return true;
  }

  return false;
}

static bool layout_key_is_tap_hold(uint8_t key) {
  const uint8_t current_layer = layout_get_current_layer();
  const uint8_t ak_index = advanced_key_indices[current_layer][key];

  return ak_index &&
         CURRENT_PROFILE.advanced_keys[ak_index - 1].type == AK_TYPE_TAP_HOLD;
}

static bool layout_should_skip_key_processing(uint8_t key,
                                              const key_state_t *state,
                                              uint8_t current_layer) {
  if (CURRENT_PROFILE.gamepad_buttons[key] != GP_BUTTON_NONE) {
    xinput_process(key);

    if (CURRENT_PROFILE.gamepad_options.gamepad_override) {
      bitmap_set(key_press_states, key, state->is_pressed);
      return true;
    }
  }

  if (current_layer == 0) {
    if (!CURRENT_PROFILE.gamepad_options.keyboard_enabled) {
      bitmap_set(key_press_states, key, state->is_pressed);
      return true;
    }
  }

  if (current_layer == 0 && bitmap_get(key_disabled, key)) {
    bitmap_set(key_press_states, key, state->is_pressed);
    return true;
  }

  return false;
}

static void layout_collect_events(layout_event_t *events,
                                  layout_event_count_t *event_count,
                                  uint8_t current_layer) {
  *event_count = 0;

  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    const key_state_t *state = &key_matrix[i];
    const bool last_key_press = bitmap_get(key_press_states, i);

    if (layout_should_skip_key_processing((uint8_t)i, state, current_layer))
      continue;

    if (state->is_pressed && !last_key_press) {
      if (*event_count >= NUM_KEYS) {
        continue;
      }
      events[(*event_count)++] = (layout_event_t){
          .key = (uint8_t)i,
          .pressed = true,
          .event_time = state->event_time,
          .distance = state->distance,
      };
      layout_trace_events("collected", &events[*event_count - 1], 1);
    } else if (!state->is_pressed && last_key_press) {
      if (*event_count >= NUM_KEYS) {
        continue;
      }
      events[(*event_count)++] = (layout_event_t){
          .key = (uint8_t)i,
          .pressed = false,
          .event_time = state->event_time,
          .distance = state->distance,
      };
      layout_trace_events("collected", &events[*event_count - 1], 1);
    } else if (state->is_pressed) {
      const uint8_t keycode = active_keycodes[i];
      const uint8_t ak_index = active_advanced_keys[i];

      if (ak_index) {
        advanced_key_event_t ak_event = (advanced_key_event_t){
            .type = AK_EVENT_TYPE_HOLD,
            .key = (uint8_t)i,
            .keycode = keycode,
            .ak_index = ak_index - 1,
        };
        advanced_key_process(&ak_event);
      }
    }
  }
}

static bool layout_event_should_swap(const layout_event_t *lhs,
                                     const layout_event_t *rhs) {
  if (lhs->event_time != rhs->event_time)
    return lhs->event_time > rhs->event_time;

  if (lhs->pressed != rhs->pressed)
    // Prefer releases before presses when two events collapse to the same
    // millisecond tick.
    return lhs->pressed && !rhs->pressed;

  if (lhs->pressed)
    // For equal timestamps, the key that is already deeper past actuation was
    // likely pressed first.
    return lhs->distance < rhs->distance;

  // For equal-timestamp releases, the key closer to the rest position was
  // likely released first.
  return lhs->distance > rhs->distance;
}

static void layout_sort_events(layout_event_t *events,
                               layout_event_count_t event_count) {
  for (layout_event_count_t i = 1; i < event_count; i++) {
    const layout_event_t tmp = events[i];
    layout_event_count_t j = i;
    while (j > 0 && layout_event_should_swap(&events[j - 1], &tmp)) {
      events[j] = events[j - 1];
      j--;
    }
    events[j] = tmp;
  }

  layout_trace_events("sorted", events, event_count);
}

static bool layout_handle_press_event(const layout_event_t *event) {
  if (advanced_key_combo_process(event->key, true, event->event_time))
    return false;

  if (!layout_key_is_tap_hold(event->key) && advanced_key_has_undecided()) {
    layout_buffer_pending_event(event->key, true);
    return true;
  }

  return layout_process_key(event->key, true);
}

static bool layout_handle_release_event(const layout_event_t *event) {
  if (advanced_key_combo_process(event->key, false, event->event_time))
    return false;

  if (layout_pending_has_press(event->key)) {
    layout_buffer_pending_event(event->key, false);
    return true;
  }

  return layout_process_key(event->key, false);
}

static void layout_process_events(const layout_event_t *events,
                                  layout_event_count_t event_count,
                                  bool *has_non_tap_hold_press,
                                  bool *has_non_tap_hold_release) {
  for (layout_event_count_t i = 0; i < event_count; i++) {
    const layout_event_t *event = &events[i];

    if (event->pressed) {
      if (layout_handle_press_event(event))
        *has_non_tap_hold_press = true;
    } else {
      if (layout_handle_release_event(event))
        *has_non_tap_hold_release = true;
    }

    bitmap_set(key_press_states, event->key, key_matrix[event->key].is_pressed);
  }
}

void layout_task(void) {
  static uint32_t last_ak_tick = 0;

  const uint8_t current_layer = layout_get_current_layer();
  bool has_non_tap_hold_press = false;
  bool has_non_tap_hold_release = false;
  static layout_event_t events[NUM_KEYS];
  layout_event_count_t event_count = 0;

  layout_collect_events(events, &event_count, current_layer);
  layout_sort_events(events, event_count);
  layout_process_events(events, event_count, &has_non_tap_hold_press,
                        &has_non_tap_hold_release);

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
  if (pending_count > 0 && !advanced_key_has_undecided())
    layout_flush_pending_events();

  hid_send_reports();

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

  const bool current_profile_written = EECONFIG_WRITE(current_profile, &profile);
  bool status = current_profile_written;
  if (status && profile != 0)
    status = EECONFIG_WRITE(last_non_default_profile, &profile);

  if (current_profile_written) {
    profile_runtime_reload_current();
  }

  return status;
}

void layout_register(uint8_t key, uint8_t keycode) {
  if (keycode == KC_NO)
    return;

  switch (keycode) {
  case HID_KEYCODE_RANGE:
    hid_keycode_add(keycode);

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

  case SP_JOY_MODE_NEXT: {
    layout_toggle_joystick_mode();
    break;
  }

  case SP_JOY_PRESET_NEXT: {
    layout_select_next_joystick_preset();
    break;
  }

  case SP_JOY_SCROLL_MO:
    layout_register_joystick_scroll_mode();
    break;

  case SP_RGB_TOGGLE:
    layout_toggle_rgb();
    break;

  case SP_RGB_BRIGHTNESS_UP:
    layout_increase_rgb_brightness();
    break;

  case SP_RGB_BRIGHTNESS_DOWN:
    layout_decrease_rgb_brightness();
    break;

  case SP_RGB_EFFECT_NEXT:
    layout_select_next_rgb_effect();
    break;

  case SP_RGB_EFFECT_PREV:
    layout_select_previous_rgb_effect();
    break;

  case SP_POLL_RATE_TOGGLE:
    layout_toggle_polling_rate();
    break;

  case SP_SNIPER:
    is_sniper_active = true;
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

    break;

  case MOMENTARY_LAYER_RANGE:
    layout_layer_off(MO_GET_LAYER(keycode));
    break;

  case SP_SNIPER:
    is_sniper_active = false;
    break;

  case SP_JOY_SCROLL_MO:
    layout_unregister_joystick_scroll_mode();
    break;

  default:
    break;
  }
}
