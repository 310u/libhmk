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

#include "migration.h"

#include "eeconfig.h"
#include "wear_leveling.h"

#define MIGRATION_GLOBAL_CONFIG_SIZE_V1_0 12
#define MIGRATION_GLOBAL_CONFIG_SIZE_V1_1 14
#define MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT                             \
  (MIGRATION_GLOBAL_CONFIG_SIZE_V1_1 + NUM_KEYS * 2)
#define MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32                             \
  (MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT + 2)

#define MIGRATION_PROFILE_BASE_SIZE(advanced_key_size)                          \
  (NUM_LAYERS * NUM_KEYS + NUM_KEYS * 4 +                                       \
   NUM_ADVANCED_KEYS * (advanced_key_size) + NUM_KEYS + 9 + 1)
#define MIGRATION_PROFILE_ADVANCED_KEYS_SIZE(advanced_key_size)                 \
  (NUM_ADVANCED_KEYS * (advanced_key_size))
#define MIGRATION_PROFILE_SIZE_WITH_MACROS(advanced_key_size)                   \
  (MIGRATION_PROFILE_BASE_SIZE(advanced_key_size) +                             \
   NUM_MACROS * sizeof(macro_t))
#define MIGRATION_PROFILE_TRAILING_SIZE_WITH_MACROS(advanced_key_size)          \
  (MIGRATION_PROFILE_SIZE_WITH_MACROS(advanced_key_size) -                      \
   (NUM_LAYERS * NUM_KEYS) - (NUM_KEYS * 4) -                                   \
   MIGRATION_PROFILE_ADVANCED_KEYS_SIZE(advanced_key_size))

#if defined(RGB_ENABLED)
#define MIGRATION_PROFILE_RGB_PER_KEY_COLORS_SIZE (3 * NUM_KEYS)
#define MIGRATION_PROFILE_RGB_SIZE_V1_8 (7 + MIGRATION_PROFILE_RGB_PER_KEY_COLORS_SIZE)
#define MIGRATION_PROFILE_RGB_SIZE_V1_A                                      \
  (7 + 1 + 2 + 3 * NUM_LAYERS + MIGRATION_PROFILE_RGB_PER_KEY_COLORS_SIZE)
#define MIGRATION_PROFILE_RGB_SIZE_V1_D                                      \
  (7 + 3 + 1 + 2 + 3 * NUM_LAYERS + MIGRATION_PROFILE_RGB_PER_KEY_COLORS_SIZE)
#define MIGRATION_PROFILE_RGB_V1_A_TAIL_SIZE                                 \
  (4 + 3 * NUM_LAYERS + MIGRATION_PROFILE_RGB_PER_KEY_COLORS_SIZE)
#else
#define MIGRATION_PROFILE_RGB_PER_KEY_COLORS_SIZE 0
#define MIGRATION_PROFILE_RGB_SIZE_V1_8 0
#define MIGRATION_PROFILE_RGB_SIZE_V1_A 0
#define MIGRATION_PROFILE_RGB_SIZE_V1_D 0
#define MIGRATION_PROFILE_RGB_V1_A_TAIL_SIZE 0
#endif

#if defined(JOYSTICK_ENABLED)
#define MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY JOYSTICK_CONFIG_LEGACY_SIZE
#define MIGRATION_PROFILE_JOYSTICK_SIZE_V1_F \
  offsetof(joystick_config_t, active_mouse_preset)
#define MIGRATION_PROFILE_JOYSTICK_SIZE_CURRENT sizeof(joystick_config_t)
#else
#define MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY 0
#define MIGRATION_PROFILE_JOYSTICK_SIZE_V1_F 0
#define MIGRATION_PROFILE_JOYSTICK_SIZE_CURRENT 0
#endif

#define MIGRATION_PROFILE_SIZE_V1_8_PLUS                                      \
  (MIGRATION_PROFILE_SIZE_WITH_MACROS(13) + MIGRATION_PROFILE_RGB_SIZE_V1_8 + \
   MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY)
#define MIGRATION_PROFILE_SIZE_V1_A_PLUS                                      \
  (MIGRATION_PROFILE_SIZE_WITH_MACROS(13) + MIGRATION_PROFILE_RGB_SIZE_V1_A + \
   MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY)
#define MIGRATION_PROFILE_SIZE_V1_D_PLUS                                      \
  (MIGRATION_PROFILE_SIZE_WITH_MACROS(13) + MIGRATION_PROFILE_RGB_SIZE_V1_D + \
   MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY)
#define MIGRATION_PROFILE_SIZE_V1_F_PLUS                                      \
  (MIGRATION_PROFILE_SIZE_WITH_MACROS(13) + MIGRATION_PROFILE_RGB_SIZE_V1_D + \
   MIGRATION_PROFILE_JOYSTICK_SIZE_V1_F)
#define MIGRATION_PROFILE_SIZE_V1_10_PLUS                                     \
  (MIGRATION_PROFILE_SIZE_WITH_MACROS(13) + MIGRATION_PROFILE_RGB_SIZE_V1_D + \
   sizeof(joystick_config_t))

static bool v1_1_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_1_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_2_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_2_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_3_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_3_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_4_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_4_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_5_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_5_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_6_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_6_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_7_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_7_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_A_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_A_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_B_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_B_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_C_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_C_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);

static bool v1_D_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_D_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);
static bool v1_E_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_E_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);
static bool v1_F_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_F_profile_config_func(uint8_t profile, uint8_t *dst,
                                     const uint8_t *src);
static bool v1_10_global_config_func(uint8_t *dst, const uint8_t *src);
static bool v1_10_profile_config_func(uint8_t profile, uint8_t *dst,
                                      const uint8_t *src);
static void migration_copy_unchanged(uint8_t *dst, const uint8_t *src,
                                     uint32_t old_size, uint32_t new_size);

// Migration metadata for each configuration version. The first entry is
// reserved for the initial version (v1.0) which does not require migration.
static const migration_t migrations[] = {
    {
        .version = 0x0100,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_V1_0,
        .profile_config_size = NUM_LAYERS * NUM_KEYS    // Keymap
                               + NUM_KEYS * 4           // Actuation map
                               + NUM_ADVANCED_KEYS * 12 // Advanced keys
                               + 1                      // Tick rate
        ,
    },
    {
        .version = 0x0101,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_V1_1,
        .profile_config_size = MIGRATION_PROFILE_BASE_SIZE(12),
        .global_config_func = v1_1_global_config_func,
        .profile_config_func = v1_1_profile_config_func,
    },
    {
        .version = 0x0102,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_BASE_SIZE(12),
        .global_config_func = v1_2_global_config_func,
        .profile_config_func = v1_2_profile_config_func,
    },
    {
        .version = 0x0103,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_BASE_SIZE(12),
        .global_config_func = v1_3_global_config_func,
        .profile_config_func = v1_3_profile_config_func,
    },
    {
        .version = 0x0104,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_BASE_SIZE(12),
        .global_config_func = v1_4_global_config_func,
        .profile_config_func = v1_4_profile_config_func,
    },
    {
        .version = 0x0105,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_SIZE_WITH_MACROS(12),
        .global_config_func = v1_5_global_config_func,
        .profile_config_func = v1_5_profile_config_func,
    },
    {
        // v1.5 -> v1.6: Added TAP_DANCE (same advanced_key size = 12 bytes)
        .version = 0x0106,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_SIZE_WITH_MACROS(12),
        .global_config_func = v1_6_global_config_func,
        .profile_config_func = v1_6_profile_config_func,
    },
    {
        // v1.6 -> v1.7: Removed TAP_DANCE, added double_tap_keycode to tap_hold.
        //               Each advanced_key grew from 12 to 13 bytes.
        .version = 0x0107,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_SIZE_WITH_MACROS(13),
        .global_config_func = v1_7_global_config_func,
        .profile_config_func = v1_7_profile_config_func,
    },
    {
        .version = 0x0108,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_8_PLUS,
        .global_config_func = NULL,
        .profile_config_func = NULL,
    },
    {
        .version = 0x0109,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_8_PLUS,
        .global_config_func = NULL,
        .profile_config_func = NULL,
    },
    {
        .version = 0x010A,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_A_PLUS,
        .global_config_func = v1_A_global_config_func,
        .profile_config_func = v1_A_profile_config_func,
    },
    {
        .version = 0x010B,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_A_PLUS,
        .global_config_func = v1_B_global_config_func,
        .profile_config_func = v1_B_profile_config_func,
    },
    {
        .version = 0x010C,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_A_PLUS,
        .global_config_func = v1_C_global_config_func,
        .profile_config_func = v1_C_profile_config_func,
    },
    {
        .version = 0x010D,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_D_PLUS,
        .global_config_func = v1_D_global_config_func,
        .profile_config_func = v1_D_profile_config_func,
    },
    {
        .version = 0x010E,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_D_PLUS,
        .global_config_func = v1_E_global_config_func,
        .profile_config_func = v1_E_profile_config_func,
    },
    {
        .version = 0x010F,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_F_PLUS,
        .global_config_func = v1_F_global_config_func,
        .profile_config_func = v1_F_profile_config_func,
    },
    {
        .version = 0x0110,
        .global_config_size = MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32,
        .profile_config_size = MIGRATION_PROFILE_SIZE_V1_10_PLUS,
        .global_config_func = v1_10_global_config_func,
        .profile_config_func = v1_10_profile_config_func,
    },
};

bool migration_try_migrate(void) {
  if (eeconfig->magic_start != EECONFIG_MAGIC_START)
    // The magic start is always the same for any version.
    return false;

  const uint16_t config_version = eeconfig->version;
  // We alternate between two buffers to save the memory.
  uint8_t current_buf = 0;
  uint8_t bufs[2][sizeof(eeconfig_t)];

  // Let `bufs[0]` be the current configuration.
  memcpy(bufs[0], eeconfig, sizeof(eeconfig_t));
  // Skip v1.0 migration since it is the initial version
  for (uint32_t i = 1; i < M_ARRAY_SIZE(migrations); i++) {
    const migration_t *m = &migrations[i];
    const migration_t *prev_m = &migrations[i - 1];

    if (m->version <= config_version)
      // Skip migrations that are not applicable
      continue;

    const uint8_t *src = bufs[current_buf];
    uint8_t *dst = bufs[current_buf ^ 1];
    memset(dst, 0, sizeof(bufs[0]));

    if (m->global_config_func) {
      if (!m->global_config_func(dst, src))
        // Migration failed for the global configuration
        return false;
    } else {
      migration_copy_unchanged(dst, src, prev_m->global_config_size,
                               m->global_config_size);
    }

    for (uint8_t p = 0; p < NUM_PROFILES; p++) {
      // Move the pointers to the start of each profile configuration
      const uint8_t *profile_src =
          src + prev_m->global_config_size + p * prev_m->profile_config_size;
      uint8_t *profile_dst =
          dst + m->global_config_size + p * m->profile_config_size;

      if (m->profile_config_func) {
        if (!m->profile_config_func(p, profile_dst, profile_src))
          // Migration failed for the profile configuration
          return false;
      } else {
        migration_copy_unchanged(profile_dst, profile_src,
                                 prev_m->profile_config_size,
                                 m->profile_config_size);
      }
    }

    // Update the version in the destination buffer
    ((eeconfig_t *)dst)->version = m->version;
    // Switch to the next buffer for the next migration
    current_buf ^= 1;
  }

  // Make sure the configuration is valid after migration
  ((eeconfig_t *)bufs[current_buf])->magic_end = EECONFIG_MAGIC_END;
  // We reflect the update in the flash.
  return wear_leveling_write(0, &bufs[current_buf], sizeof(eeconfig_t));
}

//--------------------------------------------------------------------+
// Helper Functions
//--------------------------------------------------------------------+

static void migration_memcpy(uint8_t **dst, const uint8_t **src, uint32_t len) {
  memcpy(*dst, *src, len);
  *dst += len;
  *src += len;
}

static void migration_memset(uint8_t **dst, uint8_t value, uint32_t len) {
  memset(*dst, value, len);
  *dst += len;
}

static void migration_copy_unchanged(uint8_t *dst, const uint8_t *src,
                                     uint32_t old_size, uint32_t new_size) {
  const uint32_t common_size = M_MIN(old_size, new_size);
  memcpy(dst, src, common_size);
  if (new_size > common_size) {
    memset(dst + common_size, 0, new_size - common_size);
  }
}

#define MAKE_MIGRATION_ASSIGN(type)                                            \
  static void migration_assign_##type(uint8_t **dst, type value) {             \
    *(type *)(*dst) = value;                                                   \
    *dst = (uint8_t *)((type *)(*dst) + 1);                                    \
  }

MAKE_MIGRATION_ASSIGN(uint8_t)
MAKE_MIGRATION_ASSIGN(uint16_t)

//--------------------------------------------------------------------+
// v1.0 -> v1.1 Migration
//--------------------------------------------------------------------+

bool v1_1_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x0100)
    // Expected version v1.0
    return false;

  // Copy `magic_start` to `calibration`
  migration_memcpy(&dst, &src, 10);
  // Default `options` to 0
  migration_assign_uint16_t(&dst, 0);
  // Copy `current_profile` and `last_non_default_profile`
  migration_memcpy(&dst, &src, 2);

  return true;
}

bool v1_1_profile_config_func(uint8_t profile, uint8_t *dst,
                              const uint8_t *src) {
  // Save the `keymap` offset
  uint8_t *keymap = dst;
  // Copy `keymap` to `actuation_map`
  migration_memcpy(&dst, &src, (NUM_LAYERS * NUM_KEYS) + (NUM_KEYS * 4));
  // Update keycodes to include `KC_INT1` ... `KC_LNG6`
  for (uint32_t i = 0; i < NUM_LAYERS * NUM_KEYS; i++) {
    if (0x70 <= keymap[i] && keymap[i] <= 0x71)
      // `KC_LNG1` and `KC_LNG2`
      keymap[i] += 0x06;
    else if (0x72 <= keymap[i] && keymap[i] <= 0x96)
      // `KC_LEFT_CTRL` ... `SP_MOUSE_BUTTON_5`
      keymap[i] += 0x09;
  }
  // Save the `advanced_keys` offset
  uint8_t *advanced_keys = dst;
  // Copy `advanced_keys`
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_ADVANCED_KEYS_SIZE(12));
  // Default `hold_on_other_key_press` to 0
  for (uint8_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    uint8_t *ak = advanced_keys + i * 12;
    if (ak[2] == AK_TYPE_TAP_HOLD)
      ak[7] = 0;
  }
  // Set `gamepad_buttons` to 0
  migration_memset(&dst, 0, NUM_KEYS);
  // Default `analog_curve` to linear
  migration_assign_uint8_t(&dst, 4), migration_assign_uint8_t(&dst, 20);
  migration_assign_uint8_t(&dst, 85), migration_assign_uint8_t(&dst, 95);
  migration_assign_uint8_t(&dst, 165), migration_assign_uint8_t(&dst, 170);
  migration_assign_uint8_t(&dst, 255), migration_assign_uint8_t(&dst, 255);
  // Default `keyboard_enabled` and `snappy_joystick` to true
  migration_assign_uint8_t(&dst, 0b00001001);
  // Copy `tick_rate`
  migration_memcpy(&dst, &src, 1);

  return true;
}

//--------------------------------------------------------------------+
// v1.1 -> v1.2 Migration
//--------------------------------------------------------------------+

bool v1_2_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x0101)
    // Expected version v1.1
    return false;

  // Copy `magic_start` to `calibration`
  migration_memcpy(&dst, &src, 10);
  // Set `bottom_out_threshold` to 0
  migration_memset(&dst, 0, NUM_KEYS * 2);
  // Copy `options` to `last_non_default_profile`
  migration_memcpy(&dst, &src, 4);

  return true;
}

bool v1_2_profile_config_func(uint8_t profile, uint8_t *dst,
                              const uint8_t *src) {
  // Copy the entire profile
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_BASE_SIZE(12));

  return true;
}

//--------------------------------------------------------------------+
// v1.2 -> v1.3 Migration
//--------------------------------------------------------------------+

bool v1_3_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x0102)
    // Expected version v1.2
    return false;

  // Copy `magic_start` to `bottom_out_threshold`
  migration_memcpy(&dst, &src, 10 + NUM_KEYS * 2);
  // Default `save_bottom_out_threshold` to true
  uint16_t options = *((uint16_t *)src) | (1 << 1);
  migration_assign_uint16_t(&dst, options);
  src += sizeof(options);
  // Copy `current_profile` to `last_non_default_profile`
  migration_memcpy(&dst, &src, 2);

  return true;
}

bool v1_3_profile_config_func(uint8_t profile, uint8_t *dst,
                              const uint8_t *src) {
  // Copy the entire profile
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_BASE_SIZE(12));

  return true;
}

//--------------------------------------------------------------------+
// v1.3 -> v1.4 Migration
//--------------------------------------------------------------------+

bool v1_4_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x0103)
    // Expected version v1.3
    return false;

  // Copy `magic_start` to `bottom_out_threshold`
  migration_memcpy(&dst, &src, 10 + NUM_KEYS * 2);
  // Default `high_polling_rate_enabled` to true
  uint16_t options = *((uint16_t *)src) | (1 << 2);
  migration_assign_uint16_t(&dst, options);
  src += sizeof(options);
  // Copy `current_profile` to `last_non_default_profile`
  migration_memcpy(&dst, &src, 2);

  return true;
}

bool v1_4_profile_config_func(uint8_t profile, uint8_t *dst,
                               const uint8_t *src) {
  // Copy the entire profile
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_BASE_SIZE(12));

  return true;
}

//--------------------------------------------------------------------+
// v1.4 -> v1.5 Migration
//--------------------------------------------------------------------+

bool v1_5_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x0104)
    // Expected version v1.4
    return false;

  // Global config unchanged, copy everything
  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT);

  return true;
}

bool v1_5_profile_config_func(uint8_t profile, uint8_t *dst,
                               const uint8_t *src) {
  // Copy existing profile data (keymap + actuation + advanced_keys +
  // gamepad_buttons + gamepad_options + tick_rate)
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_BASE_SIZE(12));
  // Initialize macros to zero (MACRO_ACTION_END)
  migration_memset(&dst, 0, NUM_MACROS * sizeof(macro_t));

  return true;
}

//--------------------------------------------------------------------+
// v1.5 -> v1.6 Migration
//--------------------------------------------------------------------+

bool v1_6_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x0105)
    return false;

  // Global config unchanged
  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT);
  return true;
}

bool v1_6_profile_config_func(uint8_t profile, uint8_t *dst,
                               const uint8_t *src) {
  // Profile layout unchanged: advanced_key size is still 12 bytes
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_SIZE_WITH_MACROS(12));
  return true;
}

//--------------------------------------------------------------------+
// v1.6 -> v1.7 Migration
//--------------------------------------------------------------------+

bool v1_7_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x0106)
    return false;

  // Global config unchanged
  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT);
  return true;
}

bool v1_7_profile_config_func(uint8_t profile, uint8_t *dst,
                               const uint8_t *src) {
  // Copy keymap and actuation map (unchanged)
  migration_memcpy(&dst, &src, NUM_LAYERS * NUM_KEYS + NUM_KEYS * 4);

  // Expand each advanced key from 12 bytes to 13 bytes.
  // The extra byte is double_tap_keycode (appended at the end of tap_hold data),
  // defaulted to 0 (KC_NO).
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    // Copy 12 bytes of old entry as-is
    migration_memcpy(&dst, &src, 12);
    // Append 1 byte of zero for the new double_tap_keycode field
    migration_memset(&dst, 0, 1);
  }

  // Copy remaining profile data unchanged
  migration_memcpy(
      &dst, &src, MIGRATION_PROFILE_TRAILING_SIZE_WITH_MACROS(13));
  return true;
}

//--------------------------------------------------------------------+
// v1.9 -> v1.A Migration
//--------------------------------------------------------------------+

bool v1_A_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x0109)
    return false;

  // Global config unchanged
  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT);
  return true;
}

bool v1_A_profile_config_func(uint8_t profile, uint8_t *dst,
                               const uint8_t *src) {
  // Pre-RGB stuff remains the same
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_SIZE_WITH_MACROS(13));

#if defined(RGB_ENABLED)
  // At v1.9 rgb_config was: enabled(1)+brightness(1)+effect(1)+solid(3)+speed(1) = 7 bytes
  migration_memcpy(&dst, &src, 7);

  // New fields at v1.A: sleep_timeout(1) + layer_indicator_mode(1) + layer_indicator_key(1) + layer_colors(3*NUM_LAYERS)
  migration_memset(&dst, 0, 1 + 2 + 3 * NUM_LAYERS);

  // Copy per_key_colors (3*NUM_KEYS)
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_RGB_PER_KEY_COLORS_SIZE);
#endif

#if defined(JOYSTICK_ENABLED)
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY);
#endif

  return true;
}

//--------------------------------------------------------------------+
// v1.A -> v1.B Migration
//--------------------------------------------------------------------+

bool v1_B_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x010A)
    return false;

  eeconfig_t *new_config = (eeconfig_t *)dst;

  // Global config size remains unchanged
  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_BOTTOM_OUT);

  // Initialize new sniper_mode_multiplier to 50% (128)
  new_config->options.sniper_mode_multiplier = 128;

  return true;
}

bool v1_B_profile_config_func(uint8_t profile, uint8_t *dst,
                               const uint8_t *src) {
  // Entire profile config size remains unchanged
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_SIZE_V1_A_PLUS);
  return true;
}

//--------------------------------------------------------------------+
// v1.B -> v1.C Migration
//--------------------------------------------------------------------+

bool v1_C_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x010B)
    return false;

  eeconfig_t *new_config = (eeconfig_t *)dst;

  // Copy everything before options (10 + NUM_KEYS * 2 bytes)
  migration_memcpy(&dst, &src, 10 + NUM_KEYS * 2);

  // Copy old options (2 bytes)
  uint16_t old_options;
  memcpy(&old_options, src, 2);
  src += 2;

  // Assign to new 32-bit options
  new_config->options.raw = (uint32_t)old_options;
  dst += 4;

  // Copy remaining global fields: current_profile and last_non_default_profile
  migration_memcpy(&dst, &src, 2);

  return true;
}

bool v1_C_profile_config_func(uint8_t profile, uint8_t *dst,
                               const uint8_t *src) {
  // Entire profile config size remains unchanged
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_SIZE_V1_A_PLUS);
  return true;
}

//--------------------------------------------------------------------+
// v1.C -> v1.D Migration
//--------------------------------------------------------------------+

bool v1_D_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x010C)
    return false;

  // Global config unchanged
  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32);
  return true;
}

bool v1_D_profile_config_func(uint8_t profile, uint8_t *dst,
                              const uint8_t *src) {
  // Profile fields before rgb_config are unchanged.
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_SIZE_WITH_MACROS(13));

#if defined(RGB_ENABLED)
  // v1.C RGB layout:
  // enabled(1), brightness(1), effect(1), solid(3), effect_speed(1),
  // sleep_timeout(1), layer_indicator_mode(1), layer_indicator_key(1),
  // layer_colors(3*NUM_LAYERS), per_key_colors(3*NUM_KEYS)
  //
  // v1.D inserts secondary_color(3) after solid_color.
  migration_memcpy(&dst, &src, 6); // enabled..solid_color
  migration_assign_uint8_t(&dst, 255);
  migration_assign_uint8_t(&dst, 255);
  migration_assign_uint8_t(&dst, 255);
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_RGB_V1_A_TAIL_SIZE);
#endif

#if defined(JOYSTICK_ENABLED)
  // Joystick config unchanged.
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY);
#endif

  return true;
}

//--------------------------------------------------------------------+
// v1.D -> v1.E Migration
//--------------------------------------------------------------------+

bool v1_E_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x010D)
    return false;

  // Global config unchanged
  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32);
  return true;
}

bool v1_E_profile_config_func(uint8_t profile, uint8_t *dst,
                              const uint8_t *src) {
  // Profile fields before rgb_config are unchanged.
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_SIZE_WITH_MACROS(13));

#if defined(RGB_ENABLED)
  // RGB config unchanged.
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_RGB_SIZE_V1_D);
#endif

#if defined(JOYSTICK_ENABLED)
  // Preserve joystick config, but initialize the new debounce field from the
  // old reserved byte if it was zero.
  uint8_t joystick_config[MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY];
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY);
  memcpy(joystick_config, dst - MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY,
         sizeof(joystick_config));
  if (joystick_config[offsetof(joystick_config_t, sw_debounce_ms)] == 0)
    joystick_config[offsetof(joystick_config_t, sw_debounce_ms)] = 5;
  memcpy(dst - MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY, joystick_config,
         sizeof(joystick_config));
#endif

  return true;
}

//--------------------------------------------------------------------+
// v1.E -> v1.F Migration
//--------------------------------------------------------------------+

bool v1_F_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x010E)
    return false;

  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32);
  return true;
}

bool v1_F_profile_config_func(uint8_t profile, uint8_t *dst,
                              const uint8_t *src) {
  (void)profile;

  migration_memcpy(&dst, &src, MIGRATION_PROFILE_SIZE_WITH_MACROS(13));

#if defined(RGB_ENABLED)
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_RGB_SIZE_V1_D);
#endif

#if defined(JOYSTICK_ENABLED)
  joystick_config_t joystick_config;
  joystick_init_default_config(&joystick_config);
  memcpy(&joystick_config, src, MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY);
  memcpy(dst, &joystick_config, sizeof(joystick_config));
  dst += sizeof(joystick_config);
  src += MIGRATION_PROFILE_JOYSTICK_SIZE_LEGACY;
#endif

  return true;
}

//--------------------------------------------------------------------+
// v1.F -> v1.10 Migration
//--------------------------------------------------------------------+

bool v1_10_global_config_func(uint8_t *dst, const uint8_t *src) {
  if (((eeconfig_t *)src)->version != 0x010F)
    return false;

  migration_memcpy(&dst, &src, MIGRATION_GLOBAL_CONFIG_SIZE_WITH_OPTIONS32);
  return true;
}

bool v1_10_profile_config_func(uint8_t profile, uint8_t *dst,
                               const uint8_t *src) {
  (void)profile;

  migration_memcpy(&dst, &src, MIGRATION_PROFILE_SIZE_WITH_MACROS(13));

#if defined(RGB_ENABLED)
  migration_memcpy(&dst, &src, MIGRATION_PROFILE_RGB_SIZE_V1_D);
#endif

#if defined(JOYSTICK_ENABLED)
  joystick_config_t joystick_config;
  joystick_init_default_config(&joystick_config);
  memcpy(&joystick_config, src, MIGRATION_PROFILE_JOYSTICK_SIZE_V1_F);
  joystick_fill_default_mouse_presets(joystick_config.mouse_presets,
                                      joystick_config.mouse_speed,
                                      joystick_config.mouse_acceleration);
  joystick_config.active_mouse_preset = 0u;
  memcpy(dst, &joystick_config, sizeof(joystick_config));
  dst += sizeof(joystick_config);
  src += MIGRATION_PROFILE_JOYSTICK_SIZE_V1_F;
#endif

  return true;
}
