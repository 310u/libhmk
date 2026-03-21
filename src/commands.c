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

#include "commands.h"

#include "advanced_keys.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "joystick.h"
#include "layout.h"
#include "matrix.h"
#include "metadata.h"
#include "profile_runtime.h"
#include "rgb.h"
#include "tusb.h"

// Helper macro to verify command parameters
#define COMMAND_VERIFY(cond)                                                   \
  if (!(cond)) {                                                               \
    success = false;                                                           \
    break;                                                                     \
  }

static uint8_t out_buf[RAW_HID_EP_SIZE];
static const uint8_t keyboard_metadata[] = {KEYBOARD_METADATA};

static bool command_validate_gamepad_options(
    const gamepad_options_t *gamepad_options) {
  for (uint8_t i = 1; i < 4; i++) {
    if (gamepad_options->analog_curve[i][0] <=
        gamepad_options->analog_curve[i - 1][0])
      return false;
  }
  return true;
}

static uint32_t command_profile_base_addr(uint8_t profile) {
  return offsetof(eeconfig_t, profiles) +
         (uint32_t)profile * sizeof(eeconfig_profile_t);
}

static bool command_write_profile_bytes(uint8_t profile, uint32_t field_offset,
                                        const void *data, uint32_t len) {
  return wear_leveling_write(command_profile_base_addr(profile) + field_offset,
                             data, len);
}

static void command_reset_if_current_profile(uint8_t profile) {
  if (profile == eeconfig->current_profile)
    layout_reset_runtime_state();
}

static void command_reload_if_current_profile(uint8_t profile) {
  if (profile == eeconfig->current_profile)
    profile_runtime_reload_current();
}

void command_init(void) {}

void command_process(const uint8_t *buf) {
  const command_in_buffer_t *in = (const command_in_buffer_t *)buf;
  command_out_buffer_t *out = (command_out_buffer_t *)out_buf;

  bool success = true;
  switch (in->command_id) {
  case COMMAND_FIRMWARE_VERSION: {
    out->firmware_version = FIRMWARE_VERSION;
    break;
  }
  case COMMAND_REBOOT: {
    board_reset();
    break;
  }
  case COMMAND_BOOTLOADER: {
    board_enter_bootloader();
    break;
  }
  case COMMAND_FACTORY_RESET: {
    success = eeconfig_reset();
    if (success)
      profile_runtime_reload_current();
    break;
  }
  case COMMAND_RECALIBRATE: {
    matrix_recalibrate(true);
    break;
  }
  case COMMAND_ANALOG_INFO: {
    const command_in_analog_info_t *p = &in->analog_info;
    command_out_analog_info_t *o = out->analog_info;

    COMMAND_VERIFY(p->offset < NUM_KEYS);

    for (uint32_t i = 0;
         i < M_ARRAY_SIZE(out->analog_info) && i + p->offset < NUM_KEYS; i++) {
      o[i].adc_value = key_matrix[i + p->offset].adc_filtered;
      o[i].distance = key_matrix[i + p->offset].distance;
    }
    break;
  }
  case COMMAND_ANALOG_INFO_RAW: {
    const command_in_analog_info_t *p = &in->analog_info;
    command_out_analog_info_t *o = out->analog_info;

    COMMAND_VERIFY(p->offset < NUM_KEYS);

    for (uint32_t i = 0;
         i < M_ARRAY_SIZE(out->analog_info) && i + p->offset < NUM_KEYS; i++) {
      o[i].adc_value = key_matrix[i + p->offset].adc_raw;
      o[i].distance = key_matrix[i + p->offset].distance;
    }
    break;
  }
  case COMMAND_GET_CALIBRATION: {
    out->calibration = eeconfig->calibration;
    break;
  }
  case COMMAND_SET_CALIBRATION: {
    success = EECONFIG_WRITE(calibration, &in->calibration);
    break;
  }
  case COMMAND_GET_PROFILE: {
    out->current_profile = eeconfig->current_profile;
    break;
  }
  case COMMAND_GET_OPTIONS: {
    out->options = eeconfig->options;
    break;
  }
  case COMMAND_SET_OPTIONS: {
    success = EECONFIG_WRITE(options, &in->options);
    break;
  }
  case COMMAND_RESET_PROFILE: {
    const command_in_reset_profile_t *p = &in->reset_profile;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);

    success = eeconfig_reset_profile(p->profile);
    if (success && p->profile == eeconfig->current_profile)
      profile_runtime_reload_current();
    break;
  }
  case COMMAND_DUPLICATE_PROFILE: {
    const command_in_duplicate_profile_t *p = &in->duplicate_profile;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->src_profile < NUM_PROFILES);

    success = EECONFIG_WRITE(profiles[p->profile],
                             &eeconfig->profiles[p->src_profile]);
    if (success && p->profile == eeconfig->current_profile)
      profile_runtime_reload_current();
    break;
  }
  case COMMAND_GET_KEYMAP: {
    const command_in_keymap_t *p = &in->keymap;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->layer < NUM_LAYERS);
    COMMAND_VERIFY(p->offset < NUM_KEYS);

    memcpy(out->keymap,
           eeconfig->profiles[p->profile].keymap[p->layer] + p->offset,
           M_MIN(M_ARRAY_SIZE(out->keymap), (uint32_t)(NUM_KEYS - p->offset)) *
               sizeof(uint8_t));
    break;
  }
  case COMMAND_GET_METADATA: {
    const command_in_metadata_t *p = &in->metadata;

    COMMAND_VERIFY(p->offset < sizeof(keyboard_metadata));

    out->metadata.len = sizeof(keyboard_metadata) - p->offset;
    memcpy(out->metadata.metadata, &keyboard_metadata[p->offset],
           M_MIN(sizeof(out->metadata.metadata), out->metadata.len));
    break;
  }
  case COMMAND_GET_SERIAL: {
    memset(out->serial, 0, sizeof(out->serial));
    board_serial(out->serial);
    break;
  }
  case COMMAND_SAVE_CALIBRATION_THRESHOLD: {
    uint16_t bottom_out_threshold[NUM_KEYS];

    for (uint32_t i = 0; i < NUM_KEYS; i++) {
      if (key_matrix[i].adc_bottom_out_value < key_matrix[i].adc_rest_value)
        bottom_out_threshold[i] = 0;
      else
        bottom_out_threshold[i] =
            key_matrix[i].adc_bottom_out_value - key_matrix[i].adc_rest_value;
    }
    success = EECONFIG_WRITE(bottom_out_threshold, bottom_out_threshold);
    break;
  }
  case COMMAND_SET_KEYMAP: {
    const command_in_keymap_t *p = &in->keymap;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->layer < NUM_LAYERS);
    COMMAND_VERIFY(p->offset < NUM_KEYS);
    COMMAND_VERIFY(p->len <= M_ARRAY_SIZE(p->keymap) &&
                   p->len <= NUM_KEYS - p->offset);

    const uint32_t field_offset = offsetof(eeconfig_profile_t, keymap) +
                                  p->layer *
                                      sizeof(eeconfig->profiles[0].keymap[0]) +
                                  p->offset * sizeof(uint8_t);
    success = command_write_profile_bytes(p->profile, field_offset, p->keymap,
                                          sizeof(uint8_t) * p->len);
    if (success)
      command_reset_if_current_profile(p->profile);
    break;
  }
  case COMMAND_GET_ACTUATION_MAP: {
    const command_in_actuation_map_t *p = &in->actuation_map;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < NUM_KEYS);

    memcpy(out->actuation_map,
           eeconfig->profiles[p->profile].actuation_map + p->offset,
           M_MIN(M_ARRAY_SIZE(out->actuation_map),
                 (uint32_t)(NUM_KEYS - p->offset)) *
               sizeof(actuation_t));
    break;
  }
  case COMMAND_SET_ACTUATION_MAP: {
    const command_in_actuation_map_t *p = &in->actuation_map;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < NUM_KEYS);
    COMMAND_VERIFY(p->len <= M_ARRAY_SIZE(p->actuation_map) &&
                   p->len <= NUM_KEYS - p->offset);

    const uint32_t field_offset = offsetof(eeconfig_profile_t, actuation_map) +
                                  p->offset * sizeof(actuation_t);
    success = command_write_profile_bytes(
        p->profile, field_offset, p->actuation_map,
        sizeof(actuation_t) * p->len);
    break;
  }
  case COMMAND_GET_ADVANCED_KEYS: {
    const command_in_advanced_keys_t *p = &in->advanced_keys;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < NUM_ADVANCED_KEYS);

    memcpy(out->advanced_keys,
           eeconfig->profiles[p->profile].advanced_keys + p->offset,
           M_MIN(M_ARRAY_SIZE(out->advanced_keys),
                 (uint32_t)(NUM_ADVANCED_KEYS - p->offset)) *
               sizeof(advanced_key_t));
    break;
  }
  case COMMAND_SET_ADVANCED_KEYS: {
    const command_in_advanced_keys_t *p = &in->advanced_keys;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < NUM_ADVANCED_KEYS);
    COMMAND_VERIFY(p->len <= M_ARRAY_SIZE(p->advanced_keys) &&
                   p->len <= NUM_ADVANCED_KEYS - p->offset);

    const uint32_t field_offset = offsetof(eeconfig_profile_t, advanced_keys) +
                                  p->offset * sizeof(advanced_key_t);
    success = command_write_profile_bytes(
        p->profile, field_offset, p->advanced_keys,
        sizeof(advanced_key_t) * p->len);
    if (success)
      command_reload_if_current_profile(p->profile);
    break;
  }
  case COMMAND_GET_TICK_RATE: {
    const command_in_tick_rate_t *p = &in->tick_rate;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);

    out->tick_rate = eeconfig->profiles[p->profile].tick_rate;
    break;
  }
  case COMMAND_SET_TICK_RATE: {
    const command_in_tick_rate_t *p = &in->tick_rate;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);

    success = command_write_profile_bytes(p->profile,
                                          offsetof(eeconfig_profile_t, tick_rate),
                                          &p->tick_rate, sizeof(p->tick_rate));
    break;
  }
  case COMMAND_GET_GAMEPAD_BUTTONS: {
    const command_in_gamepad_buttons_t *p = &in->gamepad_buttons;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < NUM_KEYS);

    memcpy(out->gamepad_buttons,
           eeconfig->profiles[p->profile].gamepad_buttons + p->offset,
           M_MIN(M_ARRAY_SIZE(out->gamepad_buttons),
                 (uint32_t)(NUM_KEYS - p->offset)) *
               sizeof(uint8_t));
    break;
  }
  case COMMAND_SET_GAMEPAD_BUTTONS: {
    const command_in_gamepad_buttons_t *p = &in->gamepad_buttons;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < NUM_KEYS);
    COMMAND_VERIFY(p->len <= M_ARRAY_SIZE(p->gamepad_buttons) &&
                   p->len <= NUM_KEYS - p->offset);

    const uint32_t field_offset = offsetof(eeconfig_profile_t, gamepad_buttons) +
                                  p->offset * sizeof(uint8_t);
    success = command_write_profile_bytes(
        p->profile, field_offset, p->gamepad_buttons,
        sizeof(uint8_t) * p->len);
    if (success)
      command_reset_if_current_profile(p->profile);
    break;
  }
  case COMMAND_GET_GAMEPAD_OPTIONS: {
    const command_in_gamepad_options_t *p = &in->gamepad_options;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);

    out->gamepad_options = eeconfig->profiles[p->profile].gamepad_options;
    break;
  }
  case COMMAND_SET_GAMEPAD_OPTIONS: {
    const command_in_gamepad_options_t *p = &in->gamepad_options;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(command_validate_gamepad_options(&p->gamepad_options));

    success = command_write_profile_bytes(
        p->profile, offsetof(eeconfig_profile_t, gamepad_options),
        &p->gamepad_options, sizeof(p->gamepad_options));
    if (success)
      command_reset_if_current_profile(p->profile);
    break;
  }
  case COMMAND_GET_MACROS: {
    const command_in_macros_t *p = &in->macros;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < NUM_MACROS);

    memcpy(out->macros, eeconfig->profiles[p->profile].macros + p->offset,
           M_MIN(M_ARRAY_SIZE(out->macros),
                 (uint32_t)(NUM_MACROS - p->offset)) *
               sizeof(macro_t));
    break;
  }
  case COMMAND_SET_MACROS: {
    const command_in_macros_t *p = &in->macros;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < NUM_MACROS);
    COMMAND_VERIFY(p->len <= M_ARRAY_SIZE(p->macros) &&
                   p->len <= NUM_MACROS - p->offset);

    // EECONFIG_WRITE_N cannot be used here because the field contains variables
    // (p->profile, p->offset), which are not allowed in offsetof().
    const uint32_t field_offset = offsetof(eeconfig_profile_t, macros) +
                                  p->offset * sizeof(macro_t);
    success = command_write_profile_bytes(p->profile, field_offset, p->macros,
                                          sizeof(macro_t) * p->len);
    if (success)
      command_reset_if_current_profile(p->profile);
    break;
  }
#if defined(RGB_ENABLED)
  case COMMAND_GET_RGB_CONFIG: {
    const command_in_rgb_config_t *p = &in->rgb_config;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < sizeof(rgb_config_t));

    const rgb_config_t *config = &eeconfig->profiles[p->profile].rgb_config;
    memcpy(out->rgb_config_data, ((const uint8_t *)config) + p->offset,
           M_MIN(M_ARRAY_SIZE(out->rgb_config_data),
                 (uint32_t)(sizeof(rgb_config_t) - p->offset)));
    break;
  }
  case COMMAND_SET_RGB_CONFIG: {
    const command_in_rgb_config_t *p = &in->rgb_config;

    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    COMMAND_VERIFY(p->offset < sizeof(rgb_config_t));
    COMMAND_VERIFY(p->len <= M_ARRAY_SIZE(p->data) &&
                   p->len <= sizeof(rgb_config_t) - p->offset);

    const uint32_t field_offset = offsetof(eeconfig_profile_t, rgb_config) +
                                  p->offset * sizeof(uint8_t);
    success = command_write_profile_bytes(p->profile, field_offset, p->data,
                                          sizeof(uint8_t) * p->len);

    if (success && p->profile == eeconfig->current_profile) {
      memcpy(rgb_get_config(), &eeconfig->profiles[p->profile].rgb_config,
             sizeof(rgb_config_t));
      rgb_apply_config();
    }
    break;
  }
#endif
#if defined(JOYSTICK_ENABLED)
  case COMMAND_GET_JOYSTICK_STATE: {
    joystick_state_t state = joystick_get_state();
    out->joystick_state.profile = eeconfig->current_profile;
    out->joystick_state.raw_x = state.raw_x;
    out->joystick_state.raw_y = state.raw_y;
    out->joystick_state.out_x = state.out_x;
    out->joystick_state.out_y = state.out_y;
    out->joystick_state.sw = state.sw;
    break;
  }
  case COMMAND_GET_JOYSTICK_CONFIG: {
    const command_in_joystick_config_t *p = &in->joystick_config;
    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    joystick_config_t config = eeconfig->profiles[p->profile].joystick_config;
    memcpy(out->joystick_config.data, &config, sizeof(joystick_config_t));
    break;
  }
  case COMMAND_SET_JOYSTICK_CONFIG: {
    const command_in_joystick_config_t *p = &in->joystick_config;
    COMMAND_VERIFY(p->profile < NUM_PROFILES);
    
    success = command_write_profile_bytes(
        p->profile, offsetof(eeconfig_profile_t, joystick_config),
        &p->joystick_config, sizeof(joystick_config_t));

    if (success)
      command_reset_if_current_profile(p->profile);
    break;
  }
#endif
  default: {
    // Unknown command
    success = false;
    break;
  }
  }

  // Echo the command ID back to the host if successful
  out->command_id = success ? in->command_id : COMMAND_UNKNOWN;

  while (!tud_hid_n_ready(USB_ITF_RAW_HID))
    // Wait for the raw HID interface to be ready
    tud_task();
  tud_hid_n_report(USB_ITF_RAW_HID, 0, out_buf, RAW_HID_EP_SIZE);
}
