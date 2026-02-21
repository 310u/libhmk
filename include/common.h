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

//--------------------------------------------------------------------+
// Firmware Version
//--------------------------------------------------------------------+

#define FIRMWARE_VERSION 0x0106

//--------------------------------------------------------------------+
// Common Headers
//--------------------------------------------------------------------+

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#if __has_include("config.h")
// Include additional configuration if available
#include "config.h"
#endif
#endif

#if defined(__has_include)
#if __has_include("board_def.h")
// Include board-specific definitions if available
#include "board_def.h"
#endif
#endif

//--------------------------------------------------------------------+
// Common Macros
//--------------------------------------------------------------------+

#define M_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define M_MIN(a, b) ((a) < (b) ? (a) : (b))
#define M_MAX(a, b) ((a) > (b) ? (a) : (b))
#define M_DIV_CEIL(n, d) (((n) + (d) - 1) / (d))
#define M_BIT(n) (1UL << (n))
#define M_IS_POWER_OF_TWO(n) (((n) != 0) && (((n) & ((n) - 1)) == 0))
#define M_HEX(n) ((n) < 10 ? '0' + (n) : 'A' - 10 + (n))

//--------------------------------------------------------------------+
// Keyboard Configuration
//--------------------------------------------------------------------+

#if !defined(NUM_PROFILES)
#error "NUM_PROFILES is not defined"
#endif

_Static_assert(1 <= NUM_PROFILES && NUM_PROFILES <= 8,
               "NUM_PROFILES must be between 1 and 16");

#if !defined(NUM_LAYERS)
#error "NUM_LAYERS is not defined"
#endif

_Static_assert(1 <= NUM_LAYERS && NUM_LAYERS <= 8,
               "NUM_LAYERS must be between 1 and 8");

#if !defined(NUM_KEYS)
#error "NUM_KEYS is not defined"
#endif

_Static_assert(1 <= NUM_KEYS && NUM_KEYS <= 256,
               "NUM_KEYS must be between 1 and 256");

#if !defined(NUM_ADVANCED_KEYS)
#error "NUM_ADVANCED_KEYS is not defined"
#endif

_Static_assert(1 <= NUM_ADVANCED_KEYS && NUM_ADVANCED_KEYS <= 64,
               "NUM_ADVANCED_KEYS must be between 1 and 64");

//--------------------------------------------------------------------+
// Keyboard Types
//--------------------------------------------------------------------+

// Actuation configuration for a key. If `rt_down` is non-zero, Rapid Trigger is
// enabled. If `rt_up` is non-zero, both `rt_down` and `rt_up` are used to
// configure the Rapid Trigger press and release sensitivity, respectively.
typedef struct __attribute__((packed)) {
  // Actuation point (0-255)
  uint8_t actuation_point;
  // Rapid Trigger press sensitivity (0-255)
  uint8_t rt_down;
  // Rapid Trigger release sensitivity (0-255)
  uint8_t rt_up;
  // Whether Continuous Rapid Trigger is enabled
  bool continuous;
} actuation_t;

// Advanced key types
typedef enum {
  AK_TYPE_NONE = 0,
  AK_TYPE_NULL_BIND,
  AK_TYPE_DYNAMIC_KEYSTROKE,
  AK_TYPE_TAP_HOLD,
  AK_TYPE_TOGGLE,
  AK_TYPE_COMBO,
  AK_TYPE_MACRO,
  AK_TYPE_COUNT,
} ak_type_t;

// Null Bind resolution behavior when both primary and secondary keys are
// pressed at the same time
typedef enum {
  // Prioritize the last pressed key
  NB_BEHAVIOR_LAST = 0,
  // Prioritize the primary key
  NB_BEHAVIOR_PRIMARY,
  // Prioritize the secondary key
  NB_BEHAVIOR_SECONDARY,
  // Release both keys
  NB_BEHAVIOR_NEUTRAL,
  // Prioritize the key that is pressed further
  NB_BEHAVIOR_DISTANCE,
} nb_behavior_t;

// Null Bind configuration
typedef struct __attribute__((packed)) {
  uint8_t secondary_key;
  uint8_t behavior;
  // Bottom-out point (0-255). If non-zero, both keys will be registered if both
  // of them are pressed past this point, regardless of the behavior.
  uint8_t bottom_out_point;
} null_bind_t;

// Dynamic Keystroke actions for each part of the keystroke
typedef enum {
  DKS_ACTION_HOLD = 0,
  DKS_ACTION_PRESS,
  DKS_ACTION_RELEASE,
  DKS_ACTION_TAP,
} dks_action_t;

// Dynamic Keystroke configuration
typedef struct __attribute__((packed)) {
  // Bind up to 4 keycodes
  uint8_t keycodes[4];
  // For each keycode, bind up to 4 actions for each part of the keystroke
  // Bit 0-1: Action for key press
  // Bit 2-3: Action for key bottom-out
  // Bit 4-5: Action for key release from bottom-out
  // Bit 6-7: Action for key release
  uint8_t bitmap[4];
  // Bottom-out point (0-255)
  uint8_t bottom_out_point;
} dynamic_keystroke_t;

// Tap-Hold interrupt flavors (ZMK-style)
typedef enum {
  // Hold if tapping term expired OR another key is pressed
  TAP_HOLD_FLAVOR_HOLD_PREFERRED = 0,
  // Hold if tapping term expired OR another key is pressed AND released
  TAP_HOLD_FLAVOR_BALANCED,
  // Hold only if tapping term expired
  TAP_HOLD_FLAVOR_TAP_PREFERRED,
  // Hold only if another key is pressed BEFORE tapping term expires
  TAP_HOLD_FLAVOR_TAP_UNLESS_INTERRUPTED,
} tap_hold_flavor_t;

// Tap-Hold flags bit layout
#define TH_FLAVOR_MASK 0x03
#define TH_RETRO_TAPPING_BIT 2
#define TH_HOLD_WHILE_UNDECIDED_BIT 3

#define TH_GET_FLAVOR(flags) ((flags) & TH_FLAVOR_MASK)
#define TH_GET_RETRO_TAPPING(flags) (((flags) >> TH_RETRO_TAPPING_BIT) & 1)
#define TH_GET_HOLD_WHILE_UNDECIDED(flags) \
  (((flags) >> TH_HOLD_WHILE_UNDECIDED_BIT) & 1)
#define TH_MAKE_FLAGS(flavor, retro, hwu) \
  (((flavor) & TH_FLAVOR_MASK) | ((retro) ? (1 << TH_RETRO_TAPPING_BIT) : 0) | \
   ((hwu) ? (1 << TH_HOLD_WHILE_UNDECIDED_BIT) : 0))

// Tap-Hold configuration
typedef struct __attribute__((packed)) {
  uint8_t tap_keycode;
  uint8_t hold_keycode;
  // Tapping term in milliseconds
  uint16_t tapping_term;
  // Bit 0-1: flavor (tap_hold_flavor_t)
  // Bit 2:   retro_tapping
  // Bit 3:   hold_while_undecided
  uint8_t flags;
  // If re-pressed within this time of the last tap, always produce tap (0 =
  // disabled)
  uint16_t quick_tap_ms;
  // If pressed within this time of another non-modifier key, always produce tap
  // (0 = disabled). Useful for home-row mods.
  uint16_t require_prior_idle_ms;
  // Keycode to send on double tap (0 = disabled; uses quick_tap_ms window if
  // set, otherwise uses tapping_term as detection window)
  uint8_t double_tap_keycode;
} tap_hold_t;

// Toggle configuration
typedef struct __attribute__((packed)) {
  uint8_t keycode;
  // Tapping term in milliseconds
  uint16_t tapping_term;
} toggle_t;

// Combo configuration
typedef struct __attribute__((packed)) {
  // Trigger key indices (0-255)
  uint8_t keys[4];
  // Resulting keycode
  uint8_t output_keycode;
  // Combo term in milliseconds (0 = use default)
  uint16_t term;
} combo_t;

// Macro event actions
typedef enum {
  MACRO_ACTION_END = 0,     // End of sequence
  MACRO_ACTION_TAP,         // Press + release
  MACRO_ACTION_PRESS,       // Press only
  MACRO_ACTION_RELEASE,     // Release only
  MACRO_ACTION_DELAY,       // Delay (keycode field = delay in 10ms units)
} macro_action_t;

// Macro event (2 bytes)
typedef struct __attribute__((packed)) {
  uint8_t keycode;
  uint8_t action;
} macro_event_t;

// Macro sequence
#define MAX_MACRO_EVENTS 16
typedef struct __attribute__((packed)) {
  macro_event_t events[MAX_MACRO_EVENTS];
} macro_t;

#define NUM_MACROS 16

// Macro key configuration (references a macro by index)
typedef struct __attribute__((packed)) {
  uint8_t macro_index;
} macro_key_t;

// Advanced key configuration
typedef struct __attribute__((packed)) {
  uint8_t layer;
  uint8_t key;
  uint8_t type;
  union __attribute__((packed)) {
    null_bind_t null_bind;
    dynamic_keystroke_t dynamic_keystroke;
    tap_hold_t tap_hold;
    toggle_t toggle;
    combo_t combo;
    macro_key_t macro_key;
  };
} advanced_key_t;

// Gamepad buttons
typedef enum {
  GP_BUTTON_NONE = 0,

  // Digital buttons
  GP_BUTTON_A,
  GP_BUTTON_B,
  GP_BUTTON_X,
  GP_BUTTON_Y,
  GP_BUTTON_UP,
  GP_BUTTON_DOWN,
  GP_BUTTON_LEFT,
  GP_BUTTON_RIGHT,
  GP_BUTTON_START,
  GP_BUTTON_BACK,
  GP_BUTTON_HOME,
  GP_BUTTON_LS,
  GP_BUTTON_RS,
  GP_BUTTON_LB,
  GP_BUTTON_RB,

  // Analog buttons
  GP_BUTTON_LS_UP,
  GP_BUTTON_LS_DOWN,
  GP_BUTTON_LS_LEFT,
  GP_BUTTON_LS_RIGHT,
  GP_BUTTON_RS_UP,
  GP_BUTTON_RS_DOWN,
  GP_BUTTON_RS_LEFT,
  GP_BUTTON_RS_RIGHT,
  GP_BUTTON_LT,
  GP_BUTTON_RT,
} gamepad_button_t;

// Gamepad options configuration
typedef struct __attribute__((packed)) {
  // 4 points that define the analog curve, representing the relationship
  // between the key position and the gamepad analog value
  uint8_t analog_curve[4][2];
  union __attribute__((packed)) {
    struct __attribute__((packed)) {
      // Whether to enable the layout processing for this profile
      bool keyboard_enabled : 1;
      // Whether the layout module should process the underlying key if the key
      // is mapped to a gamepad button
      bool gamepad_override : 1;
      // Whether the joystick output is square-shaped instead of circular
      bool square_joystick : 1;
      // Whether to use the maximum value of opposite axes for the joystick
      // output instead of combining them
      bool snappy_joystick : 1;
      // Reserved bits for future use
      uint8_t reserved : 4;
    };
    uint8_t options;
  };
} gamepad_options_t;
