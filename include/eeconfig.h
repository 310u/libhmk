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
#include "wear_leveling.h"
#include "rgb.h"
#include "joystick.h"

//--------------------------------------------------------------------+
// Keyboard Persistent Configuration
//--------------------------------------------------------------------+

// Magic number to identify the start of the configuration
#define EECONFIG_MAGIC_START 0x0B42494C
// Magic number to identify the end of the configuration
#define EECONFIG_MAGIC_END 0x0B4B4D48

// Keyboard calibration configuration
typedef struct __attribute__((packed)) {
  // Initial rest value of the key matrix. If the value is smaller than the
  // actual rest value, the key will have a dead zone at the beginning of the
  // keystroke. If the value is larger than the actual rest value, a longer
  // calibration process may be required.
  uint16_t initial_rest_value;
  // Minimum change in ADC values for the key to be considered bottom-out. If
  // the value is larger than the actual bottom-out threshold, the key will have
  // a dead zone at the end of the keystroke. If the value is smaller than the
  // actual bottom-out threshold, the distance calculation may be inaccurate
  // until the first bottom-out event.
  uint16_t initial_bottom_out_threshold;
} eeconfig_calibration_t;

// Keyboard options configuration
typedef union __attribute__((packed)) {
  struct __attribute__((packed)) {
    // Whether the XInput interface is enabled
    uint32_t xinput_enabled : 1;
    uint32_t _unused0 : 1;
    // Whether 8kHz polling rate is enabled. Only applicable if USB HS is
    // enabled. If disabled, the 1kHz polling rate is used instead.
    uint32_t high_polling_rate_enabled : 1;
    // Whether to continuously auto-calibrate the resting baseline
    uint32_t continuous_calibration : 1;
    // Sniper mode speed multiplier (e.g., 128 = 50%, 0 = 0%)
    uint32_t sniper_mode_multiplier : 8;
    // Reserved bits
    uint32_t reserved : 4;

    // Slider configuration
    uint32_t slider_mode : 2;   // 0: Disabled, 1: Volume, 2: Gamepad
    uint32_t slider_action : 4; // Gamepad Axis / Action mapping
    uint32_t _reserved32 : 10;
  };
  uint32_t raw;
} eeconfig_options_t;

_Static_assert(sizeof(eeconfig_options_t) == sizeof(uint32_t),
               "Invalid eeconfig_options_t size");

// Keyboard profile configuration
typedef struct __attribute__((packed)) {
  uint8_t keymap[NUM_LAYERS][NUM_KEYS];
  actuation_t actuation_map[NUM_KEYS];
  advanced_key_t advanced_keys[NUM_ADVANCED_KEYS];
  uint8_t gamepad_buttons[NUM_KEYS];
  gamepad_options_t gamepad_options;
  uint8_t tick_rate;
  macro_t macros[NUM_MACROS];
#if defined(RGB_ENABLED)
  rgb_config_t rgb_config;
#endif
#if defined(JOYSTICK_ENABLED)
  joystick_config_t joystick_config;
#endif
} eeconfig_profile_t;

// Persistent configuration version. The size of the configuration must be
// non-decreasing, so that the migration can assume that the new version is at
// least as large as the previous version.
#define EECONFIG_VERSION 0x010D

// Keyboard configuration
// Whenever there is a change in the configuration, `EECONFIG_VERSION` must be
// bumped. Make sure to update `eeconfig_reset()`, and add a migration function
// in `migration.c`.
typedef struct __attribute__((packed)) {
  // Global configurations
  // Magic number to identify the start of the configuration
  uint32_t magic_start;
  // Version of the configuration
  uint16_t version;

  // Calibration configuration
  eeconfig_calibration_t calibration;
  // Saved bottom-out threshold
  uint16_t bottom_out_threshold[NUM_KEYS];
  // Options configuration
  eeconfig_options_t options;

  // Current profile index
  uint8_t current_profile;
  // Last non-default profile index, used for profile swapping
  uint8_t last_non_default_profile;
  // End of global configurations

  // Profiles
  eeconfig_profile_t profiles[NUM_PROFILES];

  // Magic number to identify the end of the configuration
  uint32_t magic_end;
} eeconfig_t;

_Static_assert(
    sizeof(eeconfig_t) <= WL_VIRTUAL_SIZE,
    "Keyboard configuration size must be at most the virtual storage size.");

extern const eeconfig_t *eeconfig;

#define CURRENT_PROFILE (eeconfig->profiles[eeconfig->current_profile])

//--------------------------------------------------------------------+
// Default Keyboard Configuration
//--------------------------------------------------------------------+

#if !defined(DEFAULT_CALIBRATION)
#error "DEFAULT_CALIBRATION is not defined"
#endif

#if !defined(DEFAULT_OPTIONS)
// Default global options
#define DEFAULT_OPTIONS                                                        \
  {                                                                            \
      .xinput_enabled = false,                                                 \
      .high_polling_rate_enabled = true,                                       \
      .continuous_calibration = true,                                          \
      .sniper_mode_multiplier = 128, /* 50% speed */                           \
      .slider_mode = 0,                                                        \
      .slider_action = 0,                                                      \
  }
#endif

#if !defined(DEFAULT_KEYMAPS)
#error "DEFAULT_KEYMAPS is not defined"
#endif

#if !defined(DEFAULT_ACTUATION_POINT)
// Default actuation point
#define DEFAULT_ACTUATION_POINT 128
#endif

#if !defined(DEFAULT_GAMEPAD_OPTIONS)
// Default gamepad options
#define DEFAULT_GAMEPAD_OPTIONS                                                \
  {                                                                            \
      .analog_curve = {{4, 20}, {85, 95}, {165, 170}, {255, 255}},             \
      .keyboard_enabled = true,                                                \
      .snappy_joystick = true,                                                 \
  }
#endif

#if !defined(DEFAULT_TICK_RATE)
// Default tick rate
#define DEFAULT_TICK_RATE 30
#endif

#if !defined(DEFAULT_RGB_CONFIG)
#define DEFAULT_RGB_CONFIG                                                     \
  {                                                                            \
      .enabled = 1,                                                            \
      .global_brightness = 51,                                                 \
      .current_effect = 1,                                                     \
      .solid_color = {255, 0, 0},                                              \
      .secondary_color = {255, 255, 255},                                      \
      .effect_speed = 128,                                                     \
      .sleep_timeout = 0,                                                      \
      .layer_indicator_mode = 0,                                               \
      .layer_indicator_key = 0,                                                \
      .layer_colors = {0},                                                     \
      .per_key_colors = {0},                                                   \
  }
#endif

//--------------------------------------------------------------------+
// Persistent Configuration API
//--------------------------------------------------------------------+

/**
 * @brief Initialize the persistent configuration module
 *
 * @return None
 */
void eeconfig_init(void);

/**
 * @brief Reset the persistent configuration to default values
 *
 * @return true if successful, false otherwise
 */
bool eeconfig_reset(void);

/**
 * @brief Reset a specific profile to default values
 *
 * @param profile Profile index
 *
 * @return true if successful, false otherwise
 */
bool eeconfig_reset_profile(uint8_t profile);

/**
 * @brief Write a value to a field in the persistent configuration
 *
 * @param field Field to write to
 * @param value Value to write
 *
 * @return true if successful, false otherwise
 */
#define EECONFIG_WRITE(field, value)                                           \
  wear_leveling_write(offsetof(eeconfig_t, field), value,                      \
                      sizeof(((eeconfig_t *)0)->field))

/**
 * @brief Write a value to a field in the persistent configuration
 *
 * @param field Field to write to
 * @param value Value to write
 * @param len Length of the value in bytes
 *
 * @return true if successful, false otherwise
 */
#define EECONFIG_WRITE_N(field, value, len)                                    \
  wear_leveling_write(offsetof(eeconfig_t, field), value, len)
