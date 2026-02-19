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

#include "common.h"

//--------------------------------------------------------------------+
// Null Bind State
//--------------------------------------------------------------------+

// Null Bind state
typedef struct {
  // Whether the primary and secondary keys are registered
  bool is_pressed[2];
  // Active keycodes of the primary and secondary keys
  uint8_t keycodes[2];
} ak_state_null_bind_t;

//--------------------------------------------------------------------+
// Dynamic Keystroke State
//--------------------------------------------------------------------+

typedef struct {
  // Whether each key binding is registered
  bool is_pressed[4];
  // Whether the key is bottomed out
  bool is_bottomed_out;
} ak_state_dynamic_keystroke_t;

//--------------------------------------------------------------------+
// Tap-Hold State
//--------------------------------------------------------------------+

// Tap-Hold stage
typedef enum {
  TAP_HOLD_STAGE_NONE = 0,
  TAP_HOLD_STAGE_TAP,
  TAP_HOLD_STAGE_HOLD,
} ak_tap_hold_stage_t;

// Tap-Hold state
typedef struct {
  // Time when the key was pressed
  uint32_t since;
  // Tap-Hold stage
  uint8_t stage;
  // Whether another key was pressed during the hold
  bool interrupted;
  // Whether another key was released during the hold (for balanced flavor)
  bool other_key_released;
} ak_state_tap_hold_t;

//--------------------------------------------------------------------+
// Toggle State
//--------------------------------------------------------------------+

// Toggle stage
typedef enum {
  TOGGLE_STAGE_NONE = 0,
  TOGGLE_STAGE_TOGGLE,
  TOGGLE_STAGE_NORMAL,
} ak_toggle_stage_t;

// Toggle state
typedef struct {
  // Time when the key was pressed
  uint32_t since;
  // Toggle stage
  uint8_t stage;
  // Whether the key is toggled
  bool is_toggled;
} ak_state_toggle_t;

//--------------------------------------------------------------------+
// Combo State
//--------------------------------------------------------------------+

// Combo state
typedef struct {
  // Time when the first key in the combo was pressed
  uint32_t since;
  // Whether the combo is active
  bool is_active;
} ak_state_combo_t;

//--------------------------------------------------------------------+
// Macro State
//--------------------------------------------------------------------+

// Macro state
typedef struct {
  // Current event index in the macro sequence
  uint8_t event_index;
  // Timestamp for delay tracking
  uint32_t delay_until;
  // Whether the macro is currently playing
  bool is_playing;
} ak_state_macro_t;

//--------------------------------------------------------------------+
// Advanced Key State
//--------------------------------------------------------------------+

// Advanced key state
typedef union {
  ak_state_null_bind_t null_bind;
  ak_state_dynamic_keystroke_t dynamic_keystroke;
  ak_state_tap_hold_t tap_hold;
  ak_state_toggle_t toggle;
  ak_state_combo_t combo;
  ak_state_macro_t macro;
} advanced_key_state_t;

//--------------------------------------------------------------------+
// Advanced Key Event
//--------------------------------------------------------------------+

// Key event type. The events are arranged in this order to allow for easy
// access to the DKS action bitmaps.
typedef enum {
  AK_EVENT_TYPE_HOLD = 0,
  AK_EVENT_TYPE_PRESS,
  AK_EVENT_TYPE_BOTTOM_OUT,
  AK_EVENT_TYPE_RELEASE_FROM_BOTTOM_OUT,
  AK_EVENT_TYPE_RELEASE,
} ak_event_type_t;

// Advanced key event
typedef struct {
  // Key event type
  uint8_t type;
  // Key index
  uint8_t key;
  // Underlying keycode. Only for Null Bind advanced keys
  uint8_t keycode;
  // Advanced key index associated with the key
  uint8_t ak_index;
} advanced_key_event_t;

//--------------------------------------------------------------------+
// Advanced Key API
//--------------------------------------------------------------------+

/**
 * @brief Initialize the advanced key module
 *
 * @return None
 */
void advanced_key_init(void);

/**
 * @brief Clear advanced key states
 *
 * This function clears the advanced key states. It should be called before the
 * profile changes or the advanced keys are updated.
 *
 * @return None
 */
void advanced_key_clear(void);

/**
 * @brief Process an advanced key event
 *
 * @param event Advanced key event to process
 *
 * @return None
 */
void advanced_key_process(const advanced_key_event_t *event);

/**
 * @brief Advanced key tick
 *
 * This function is called periodically to update the time-based advanced keys
 * (e.g., Tap-Hold and Toggle keys).
 *
 * @param has_non_tap_hold_press Whether there is a non-Tap-Hold key press
 *
 * @return None
 */
void advanced_key_tick(bool has_non_tap_hold_press,
                       bool has_non_tap_hold_release);

/**
 * @brief Process a key event for combo detection
 *
 * @param key Key index
 * @param pressed Whether the key is pressed
 * @param time Time of the event
 *
 * @return true if the event is consumed (buffered), false otherwise
 */
bool advanced_key_combo_process(uint8_t key, bool pressed, uint32_t time);

/**
 * @brief Combo task
 *
 * This function should be called periodically to update the combo state.
 *
 * @return None
 */
bool advanced_key_combo_task(void);
void advanced_key_combo_invalidate_cache(void);

/**
 * @brief Update the last non-modifier key press time
 *
 * This function should be called when a non-modifier key is pressed. It is used
 * by the Tap-Hold `require_prior_idle_ms` feature to determine if the hold-tap
 * should be bypassed.
 *
 * @param time Time of the key press
 *
 * @return None
 */
void advanced_key_update_last_key_time(uint32_t time);

/**
 * @brief Check if any Tap-Hold key is in undecided (TAP) stage
 *
 * @return true if any Tap-Hold key is undecided
 */
bool advanced_key_has_undecided(void);
