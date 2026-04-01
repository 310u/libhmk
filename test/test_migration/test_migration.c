#include <unity.h>

#include "eeconfig.h"
#include "migration.h"

static uint8_t legacy_config[sizeof(eeconfig_t)];
static eeconfig_t written_config;
static uint32_t write_addr;
static uint32_t write_len;
static uint32_t write_count;

const eeconfig_t *eeconfig = (const eeconfig_t *)legacy_config;

static void write_u8(uint8_t **dst, uint8_t value) { *(*dst)++ = value; }

static void write_u16(uint8_t **dst, uint16_t value) {
  memcpy(*dst, &value, sizeof(value));
  *dst += sizeof(value);
}

static void write_u32(uint8_t **dst, uint32_t value) {
  memcpy(*dst, &value, sizeof(value));
  *dst += sizeof(value);
}

static void write_bytes(uint8_t **dst, const void *src, uint32_t len) {
  memcpy(*dst, src, len);
  *dst += len;
}

static void write_fill(uint8_t **dst, uint8_t value, uint32_t len) {
  memset(*dst, value, len);
  *dst += len;
}

static void write_legacy_keymap(uint8_t **dst, uint8_t seed) {
  for (uint32_t i = 0; i < NUM_LAYERS * NUM_KEYS; i++) {
    write_u8(dst, (uint8_t)(seed + i));
  }
}

static void write_legacy_actuation(uint8_t **dst, uint8_t seed) {
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    write_u8(dst, (uint8_t)(seed + i));
    write_u8(dst, (uint8_t)(10 + i));
    write_u8(dst, (uint8_t)(20 + i));
    write_u8(dst, (uint8_t)(i & 1u));
  }
}

static void write_legacy_advanced_keys(uint8_t **dst, uint8_t size,
                                       uint8_t seed) {
  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    for (uint8_t j = 0; j < size; j++) {
      write_u8(dst, (uint8_t)(seed + i + j));
    }
  }
}

static void write_legacy_macros(uint8_t **dst, uint8_t seed) {
  for (uint32_t macro = 0; macro < NUM_MACROS; macro++) {
    for (uint32_t event = 0; event < MAX_MACRO_EVENTS; event++) {
      write_u8(dst, (uint8_t)(seed + macro + event));
      write_u8(dst, MACRO_ACTION_TAP);
    }
  }
}

static void write_legacy_gamepad_options(uint8_t **dst, uint8_t options) {
  static const uint8_t curve[4][2] = {
      {4, 20},
      {85, 95},
      {165, 170},
      {255, 255},
  };

  write_bytes(dst, curve, sizeof(curve));
  write_u8(dst, options);
}

static joystick_config_t make_legacy_joystick_config(uint8_t seed,
                                                     uint8_t mouse_acceleration,
                                                     uint8_t sw_debounce_ms) {
  joystick_config_t config = {
      .x = {(uint16_t)(100 + seed), (uint16_t)(200 + seed),
            (uint16_t)(300 + seed)},
      .y = {(uint16_t)(400 + seed), (uint16_t)(500 + seed),
            (uint16_t)(600 + seed)},
      .deadzone = (uint8_t)(10 + seed),
      .mode = JOYSTICK_MODE_SCROLL,
      .mouse_speed = (uint8_t)(20 + seed),
      .mouse_acceleration = mouse_acceleration,
      .sw_debounce_ms = sw_debounce_ms,
  };

  config.reserved[0] = (uint8_t)(0xA0u + seed);
  config.reserved[1] = (uint8_t)(0xB0u + seed);
  config.reserved[2] = (uint8_t)(0xC0u + seed);
  return config;
}

static void write_legacy_rgb_per_key(uint8_t **dst, uint8_t seed) {
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    write_u8(dst, (uint8_t)(seed + i));
    write_u8(dst, (uint8_t)(seed + i + 1));
    write_u8(dst, (uint8_t)(seed + i + 2));
  }
}

static void write_legacy_rgb_layer_colors(uint8_t **dst, uint8_t seed) {
  for (uint32_t layer = 0; layer < NUM_LAYERS; layer++) {
    write_u8(dst, (uint8_t)(seed + layer * 3));
    write_u8(dst, (uint8_t)(seed + layer * 3 + 1));
    write_u8(dst, (uint8_t)(seed + layer * 3 + 2));
  }
}

static void write_legacy_profile_v1_0(uint8_t **dst, uint8_t seed) {
  write_legacy_keymap(dst, seed);
  write_legacy_actuation(dst, (uint8_t)(seed + 32));
  write_legacy_advanced_keys(dst, 12, (uint8_t)(seed + 64));
  write_u8(dst, (uint8_t)(30 + seed));
}

static void write_legacy_profile_prefix_v1_8_plus(uint8_t **dst, uint8_t seed) {
  write_legacy_keymap(dst, seed);
  write_legacy_actuation(dst, (uint8_t)(seed + 32));
  write_legacy_advanced_keys(dst, 13, (uint8_t)(seed + 64));
  write_fill(dst, (uint8_t)(seed + 96), NUM_KEYS);
  write_legacy_gamepad_options(dst, 0b00001001);
  write_u8(dst, (uint8_t)(24 + seed));
  write_legacy_macros(dst, (uint8_t)(seed + 112));
}

static void write_legacy_profile_v1_8(uint8_t **dst, uint8_t seed) {
  joystick_config_t joystick_config =
      make_legacy_joystick_config(seed, (uint8_t)(180 + seed), 9);

  write_legacy_profile_prefix_v1_8_plus(dst, seed);
  write_u8(dst, 1);
  write_u8(dst, (uint8_t)(40 + seed));
  write_u8(dst, RGB_EFFECT_PIXEL_FLOW);
  write_u8(dst, (uint8_t)(10 + seed));
  write_u8(dst, (uint8_t)(20 + seed));
  write_u8(dst, (uint8_t)(30 + seed));
  write_u8(dst, (uint8_t)(90 + seed));
  write_legacy_rgb_per_key(dst, seed);
  write_bytes(dst, &joystick_config, JOYSTICK_CONFIG_LEGACY_SIZE);
}

static void write_legacy_profile_v1_9(uint8_t **dst, uint8_t seed) {
  write_legacy_profile_prefix_v1_8_plus(dst, seed);
  write_u8(dst, 1);
  write_u8(dst, (uint8_t)(40 + seed));
  write_u8(dst, RGB_EFFECT_ALPHAS_MODS);
  write_u8(dst, (uint8_t)(10 + seed));
  write_u8(dst, (uint8_t)(20 + seed));
  write_u8(dst, (uint8_t)(30 + seed));
  write_u8(dst, (uint8_t)(90 + seed));
  write_legacy_rgb_per_key(dst, seed);
  write_fill(dst, 0, JOYSTICK_CONFIG_LEGACY_SIZE);
}

static void write_legacy_profile_v1_A(uint8_t **dst, uint8_t seed) {
  joystick_config_t joystick_config =
      make_legacy_joystick_config(seed, (uint8_t)(170 + seed), 7);

  write_legacy_profile_prefix_v1_8_plus(dst, seed);
  write_u8(dst, 1);
  write_u8(dst, (uint8_t)(50 + seed));
  write_u8(dst, RGB_EFFECT_RAINBOW_BEACON);
  write_u8(dst, (uint8_t)(11 + seed));
  write_u8(dst, (uint8_t)(21 + seed));
  write_u8(dst, (uint8_t)(31 + seed));
  write_u8(dst, (uint8_t)(80 + seed));
  write_u8(dst, (uint8_t)(5 + seed));
  write_u8(dst, 1);
  write_u8(dst, (uint8_t)(2 + seed));
  write_legacy_rgb_layer_colors(dst, (uint8_t)(70 + seed));
  write_legacy_rgb_per_key(dst, (uint8_t)(5 + seed));
  write_bytes(dst, &joystick_config, JOYSTICK_CONFIG_LEGACY_SIZE);
}

static void write_legacy_profile_v1_D(uint8_t **dst, uint8_t seed) {
  joystick_config_t joystick_config =
      make_legacy_joystick_config(seed, (uint8_t)(200 + seed), 0);

  write_legacy_profile_prefix_v1_8_plus(dst, seed);
  write_u8(dst, 1);
  write_u8(dst, (uint8_t)(60 + seed));
  write_u8(dst, RGB_EFFECT_DIGITAL_RAIN);
  write_u8(dst, (uint8_t)(10 + seed));
  write_u8(dst, (uint8_t)(20 + seed));
  write_u8(dst, (uint8_t)(30 + seed));
  write_u8(dst, (uint8_t)(70 + seed));
  write_u8(dst, (uint8_t)(80 + seed));
  write_u8(dst, (uint8_t)(90 + seed));
  write_u8(dst, (uint8_t)(100 + seed));
  write_u8(dst, (uint8_t)(6 + seed));
  write_u8(dst, 2);
  write_u8(dst, (uint8_t)(3 + seed));
  write_legacy_rgb_layer_colors(dst, (uint8_t)(90 + seed));
  write_legacy_rgb_per_key(dst, (uint8_t)(10 + seed));
  write_bytes(dst, &joystick_config, JOYSTICK_CONFIG_LEGACY_SIZE);
}

static void write_legacy_profile_v1_10(uint8_t **dst, uint8_t seed) {
  joystick_config_t joystick_config;
  joystick_init_default_config(&joystick_config);

  joystick_config.x.min = (uint16_t)(500 + seed);
  joystick_config.x.center = (uint16_t)(1500 + seed);
  joystick_config.x.max = (uint16_t)(3500 + seed);
  joystick_config.y.min = (uint16_t)(600 + seed);
  joystick_config.y.center = (uint16_t)(1600 + seed);
  joystick_config.y.max = (uint16_t)(3600 + seed);
  joystick_config.deadzone = (uint8_t)(12 + seed);
  joystick_config.mode = JOYSTICK_MODE_CURSOR_8;
  joystick_config.mouse_speed = (uint8_t)(40 + seed);
  joystick_config.mouse_acceleration = (uint8_t)(90 + seed);
  joystick_config.sw_debounce_ms = (uint8_t)(6 + seed);
  joystick_config.active_mouse_preset = 2;
  joystick_config.mouse_presets[2].mouse_speed = (uint8_t)(70 + seed);
  joystick_config.mouse_presets[2].mouse_acceleration = (uint8_t)(140 + seed);

  write_legacy_profile_prefix_v1_8_plus(dst, seed);
  write_u8(dst, 1);
  write_u8(dst, (uint8_t)(65 + seed));
  write_u8(dst, RGB_EFFECT_TRIGGER_STATE);
  write_u8(dst, (uint8_t)(15 + seed));
  write_u8(dst, (uint8_t)(25 + seed));
  write_u8(dst, (uint8_t)(35 + seed));
  write_u8(dst, (uint8_t)(75 + seed));
  write_u8(dst, (uint8_t)(85 + seed));
  write_u8(dst, (uint8_t)(95 + seed));
  write_u8(dst, (uint8_t)(110 + seed));
  write_u8(dst, (uint8_t)(7 + seed));
  write_u8(dst, 2);
  write_u8(dst, (uint8_t)(4 + seed));
  write_legacy_rgb_layer_colors(dst, (uint8_t)(100 + seed));
  write_legacy_rgb_per_key(dst, (uint8_t)(20 + seed));
  write_bytes(dst, &joystick_config, sizeof(joystick_config));
}

static void build_legacy_config_v1_0(void) {
  uint8_t *dst = legacy_config;

  write_u32(&dst, EECONFIG_MAGIC_START);
  write_u16(&dst, 0x0100);
  write_u16(&dst, 1000);
  write_u16(&dst, 500);
  write_u8(&dst, 1);
  write_u8(&dst, 2);

  for (uint32_t profile = 0; profile < NUM_PROFILES; profile++) {
    write_legacy_profile_v1_0(&dst, (uint8_t)(profile * 16));
  }

  legacy_config[12] = 0x70;
  legacy_config[13] = 0x72;
}

static void build_legacy_config_v1_8(void) {
  uint8_t *dst = legacy_config;

  write_u32(&dst, EECONFIG_MAGIC_START);
  write_u16(&dst, 0x0108);
  write_u16(&dst, 1080);
  write_u16(&dst, 580);
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    write_u16(&dst, (uint16_t)(650 + i));
  }
  write_u16(&dst, 0x0009);
  write_u8(&dst, 2);
  write_u8(&dst, 1);

  for (uint32_t profile = 0; profile < NUM_PROFILES; profile++) {
    write_legacy_profile_v1_8(&dst, (uint8_t)(profile * 16));
  }
}

static void build_legacy_config_v1_9(void) {
  uint8_t *dst = legacy_config;

  write_u32(&dst, EECONFIG_MAGIC_START);
  write_u16(&dst, 0x0109);
  write_u16(&dst, 1100);
  write_u16(&dst, 600);
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    write_u16(&dst, (uint16_t)(700 + i));
  }
  write_u16(&dst, 0x0009);
  write_u8(&dst, 0);
  write_u8(&dst, 0);

  for (uint32_t profile = 0; profile < NUM_PROFILES; profile++) {
    write_legacy_profile_v1_9(&dst, (uint8_t)(profile * 16));
  }
}

static void build_legacy_config_v1_B(uint16_t options) {
  uint8_t *dst = legacy_config;

  write_u32(&dst, EECONFIG_MAGIC_START);
  write_u16(&dst, 0x010B);
  write_u16(&dst, 1200);
  write_u16(&dst, 620);
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    write_u16(&dst, (uint16_t)(710 + i));
  }
  write_u16(&dst, options);
  write_u8(&dst, 1);
  write_u8(&dst, 2);

  for (uint32_t profile = 0; profile < NUM_PROFILES; profile++) {
    write_legacy_profile_v1_A(&dst, (uint8_t)(profile * 16));
  }
}

static void build_legacy_config_v1_D(uint32_t options) {
  uint8_t *dst = legacy_config;

  write_u32(&dst, EECONFIG_MAGIC_START);
  write_u16(&dst, 0x010D);
  write_u16(&dst, 1300);
  write_u16(&dst, 630);
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    write_u16(&dst, (uint16_t)(720 + i));
  }
  write_u32(&dst, options);
  write_u8(&dst, 0);
  write_u8(&dst, 1);

  for (uint32_t profile = 0; profile < NUM_PROFILES; profile++) {
    write_legacy_profile_v1_D(&dst, (uint8_t)(profile * 16));
  }
}

static void build_legacy_config_v1_10(uint32_t options) {
  uint8_t *dst = legacy_config;

  write_u32(&dst, EECONFIG_MAGIC_START);
  write_u16(&dst, 0x0110);
  write_u16(&dst, 1400);
  write_u16(&dst, 640);
  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    write_u16(&dst, (uint16_t)(730 + i));
  }
  write_u32(&dst, options);
  write_u8(&dst, 2);
  write_u8(&dst, 0);

  for (uint32_t profile = 0; profile < NUM_PROFILES; profile++) {
    write_legacy_profile_v1_10(&dst, (uint8_t)(profile * 16));
  }
}

static void assert_rgb_per_key_color(const rgb_config_t *config, uint8_t seed,
                                     uint8_t index) {
  TEST_ASSERT_EQUAL_UINT8((uint8_t)(seed + index), config->per_key_colors[index].r);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)(seed + index + 1),
                          config->per_key_colors[index].g);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)(seed + index + 2),
                          config->per_key_colors[index].b);
}

static void assert_trigger_state_defaults_from_legacy(
    const rgb_config_t *config, rgb_color_t solid_color,
    rgb_color_t secondary_color) {
  TEST_ASSERT_EQUAL_UINT8(secondary_color.r >> 2,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_IDLE].r);
  TEST_ASSERT_EQUAL_UINT8(secondary_color.g >> 2,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_IDLE].g);
  TEST_ASSERT_EQUAL_UINT8(secondary_color.b >> 2,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_IDLE].b);
  TEST_ASSERT_EQUAL_UINT8(
      secondary_color.r,
      config->trigger_state_colors[RGB_TRIGGER_STATE_RELEASE].r);
  TEST_ASSERT_EQUAL_UINT8(
      secondary_color.g,
      config->trigger_state_colors[RGB_TRIGGER_STATE_RELEASE].g);
  TEST_ASSERT_EQUAL_UINT8(
      secondary_color.b,
      config->trigger_state_colors[RGB_TRIGGER_STATE_RELEASE].b);
  TEST_ASSERT_EQUAL_UINT8(solid_color.r,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_PRESS].r);
  TEST_ASSERT_EQUAL_UINT8(solid_color.g,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_PRESS].g);
  TEST_ASSERT_EQUAL_UINT8(solid_color.b,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_PRESS].b);
  TEST_ASSERT_EQUAL_UINT8(solid_color.r,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_HOLD].r);
  TEST_ASSERT_EQUAL_UINT8(solid_color.g,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_HOLD].g);
  TEST_ASSERT_EQUAL_UINT8(solid_color.b,
                          config->trigger_state_colors[RGB_TRIGGER_STATE_HOLD].b);
}

static void assert_background_matches_secondary(const rgb_config_t *config) {
  TEST_ASSERT_EQUAL_UINT8(config->secondary_color.r, config->background_color.r);
  TEST_ASSERT_EQUAL_UINT8(config->secondary_color.g, config->background_color.g);
  TEST_ASSERT_EQUAL_UINT8(config->secondary_color.b, config->background_color.b);
}

static void assert_default_radial_boundaries(const joystick_config_t *config) {
  for (uint32_t i = 0; i < JOYSTICK_RADIAL_BOUNDARY_SECTORS; i++) {
    TEST_ASSERT_EQUAL_UINT8(JOYSTICK_RADIAL_BOUNDARY_DEFAULT,
                            config->radial_boundaries[i]);
  }
}

static void assert_mouse_presets_match_active(const joystick_config_t *config) {
  TEST_ASSERT_EQUAL_UINT8(0, config->active_mouse_preset);
  for (uint32_t i = 0; i < JOYSTICK_MOUSE_PRESET_COUNT; i++) {
    TEST_ASSERT_EQUAL_UINT8(config->mouse_speed,
                            config->mouse_presets[i].mouse_speed);
    TEST_ASSERT_EQUAL_UINT8(config->mouse_acceleration,
                            config->mouse_presets[i].mouse_acceleration);
  }
}

bool wear_leveling_write(uint32_t addr, const void *buf, uint32_t len) {
  write_count++;
  write_addr = addr;
  write_len = len;
  memcpy(&written_config, buf, M_MIN((uint32_t)sizeof(written_config), len));
  return true;
}

void setUp(void) {
  memset(legacy_config, 0, sizeof(legacy_config));
  memset(&written_config, 0, sizeof(written_config));
  write_addr = 0;
  write_len = 0;
  write_count = 0;
}

void tearDown(void) {}

void test_migration_rejects_invalid_magic(void) {
  legacy_config[0] = 0xAA;

  TEST_ASSERT_FALSE(migration_try_migrate());
  TEST_ASSERT_EQUAL_UINT32(0, write_count);
}

void test_migration_v1_0_reaches_current_and_preserves_profile_data(void) {
  build_legacy_config_v1_0();

  TEST_ASSERT_TRUE(migration_try_migrate());
  TEST_ASSERT_EQUAL_UINT32(1, write_count);
  TEST_ASSERT_EQUAL_UINT32(0, write_addr);
  TEST_ASSERT_EQUAL_UINT32(sizeof(eeconfig_t), write_len);
  TEST_ASSERT_EQUAL_HEX16(EECONFIG_VERSION, written_config.version);
  TEST_ASSERT_EQUAL_HEX32(EECONFIG_MAGIC_START, written_config.magic_start);
  TEST_ASSERT_EQUAL_HEX32(EECONFIG_MAGIC_END, written_config.magic_end);
  TEST_ASSERT_EQUAL_UINT8(1, written_config.current_profile);
  TEST_ASSERT_EQUAL_UINT8(2, written_config.last_non_default_profile);

  TEST_ASSERT_EQUAL_UINT8(0x76, written_config.profiles[0].keymap[0][0]);
  TEST_ASSERT_EQUAL_UINT8(0x7B, written_config.profiles[0].keymap[0][1]);
  TEST_ASSERT_EQUAL_UINT8(30, written_config.profiles[0].tick_rate);
  TEST_ASSERT_TRUE(written_config.profiles[0].gamepad_options.keyboard_enabled);
  TEST_ASSERT_TRUE(written_config.profiles[0].gamepad_options.snappy_joystick);
  TEST_ASSERT_EQUAL_UINT8(0, written_config.profiles[0].macros[0].events[0].keycode);
  TEST_ASSERT_EQUAL_UINT8(MACRO_ACTION_END,
                          written_config.profiles[0].macros[0].events[0].action);
  TEST_ASSERT_EQUAL_UINT8(255, written_config.profiles[0].rgb_config.secondary_color.r);
  TEST_ASSERT_EQUAL_UINT8(255, written_config.profiles[0].rgb_config.secondary_color.g);
  TEST_ASSERT_EQUAL_UINT8(255, written_config.profiles[0].rgb_config.secondary_color.b);
  assert_background_matches_secondary(&written_config.profiles[0].rgb_config);
}

void test_migration_v1_8_null_migration_preserves_rgb_and_joystick_blocks(void) {
  build_legacy_config_v1_8();

  TEST_ASSERT_TRUE(migration_try_migrate());
  TEST_ASSERT_EQUAL_HEX16(EECONFIG_VERSION, written_config.version);
  TEST_ASSERT_EQUAL_UINT8(2, written_config.current_profile);
  TEST_ASSERT_EQUAL_UINT8(1, written_config.last_non_default_profile);

  const eeconfig_profile_t *profile = &written_config.profiles[1];
  TEST_ASSERT_EQUAL_UINT8(40, profile->tick_rate);
  TEST_ASSERT_EQUAL_UINT8(56, profile->rgb_config.global_brightness);
  TEST_ASSERT_EQUAL_UINT8(RGB_EFFECT_PIXEL_FLOW,
                          profile->rgb_config.current_effect);
  TEST_ASSERT_EQUAL_UINT8(26, profile->rgb_config.solid_color.r);
  TEST_ASSERT_EQUAL_UINT8(36, profile->rgb_config.solid_color.g);
  TEST_ASSERT_EQUAL_UINT8(46, profile->rgb_config.solid_color.b);
  TEST_ASSERT_EQUAL_UINT8(255, profile->rgb_config.secondary_color.r);
  TEST_ASSERT_EQUAL_UINT8(255, profile->rgb_config.secondary_color.g);
  TEST_ASSERT_EQUAL_UINT8(255, profile->rgb_config.secondary_color.b);
  assert_background_matches_secondary(&profile->rgb_config);
  TEST_ASSERT_EQUAL_UINT8(0, profile->rgb_config.layer_colors[0].r);
  TEST_ASSERT_EQUAL_UINT8(0, profile->rgb_config.layer_colors[0].g);
  TEST_ASSERT_EQUAL_UINT8(0, profile->rgb_config.layer_colors[0].b);
  assert_rgb_per_key_color(&profile->rgb_config, 16, 0);
  assert_rgb_per_key_color(&profile->rgb_config, 16, 9);

  TEST_ASSERT_EQUAL_UINT16(116, profile->joystick_config.x.min);
  TEST_ASSERT_EQUAL_UINT16(216, profile->joystick_config.x.center);
  TEST_ASSERT_EQUAL_UINT16(616, profile->joystick_config.y.max);
  TEST_ASSERT_EQUAL_UINT8(26, profile->joystick_config.deadzone);
  TEST_ASSERT_EQUAL_UINT8(JOYSTICK_MODE_SCROLL, profile->joystick_config.mode);
  TEST_ASSERT_EQUAL_UINT8(36, profile->joystick_config.mouse_speed);
  TEST_ASSERT_EQUAL_UINT8(196, profile->joystick_config.mouse_acceleration);
  TEST_ASSERT_EQUAL_UINT8(9, profile->joystick_config.sw_debounce_ms);
  TEST_ASSERT_EQUAL_UINT8(0xB0, profile->joystick_config.reserved[0]);
  assert_default_radial_boundaries(&profile->joystick_config);
  assert_mouse_presets_match_active(&profile->joystick_config);
}

void test_migration_v1_9_preserves_rgb_base_fields_and_per_key_colors(void) {
  build_legacy_config_v1_9();

  TEST_ASSERT_TRUE(migration_try_migrate());
  TEST_ASSERT_EQUAL_HEX16(EECONFIG_VERSION, written_config.version);

  TEST_ASSERT_EQUAL_UINT8(1, written_config.profiles[0].rgb_config.enabled);
  TEST_ASSERT_EQUAL_UINT8(40, written_config.profiles[0].rgb_config.global_brightness);
  TEST_ASSERT_EQUAL_UINT8(RGB_EFFECT_ALPHAS_MODS,
                          written_config.profiles[0].rgb_config.current_effect);
  TEST_ASSERT_EQUAL_UINT8(10, written_config.profiles[0].rgb_config.solid_color.r);
  TEST_ASSERT_EQUAL_UINT8(20, written_config.profiles[0].rgb_config.solid_color.g);
  TEST_ASSERT_EQUAL_UINT8(30, written_config.profiles[0].rgb_config.solid_color.b);
  TEST_ASSERT_EQUAL_UINT8(255, written_config.profiles[0].rgb_config.secondary_color.r);
  TEST_ASSERT_EQUAL_UINT8(255, written_config.profiles[0].rgb_config.secondary_color.g);
  TEST_ASSERT_EQUAL_UINT8(255, written_config.profiles[0].rgb_config.secondary_color.b);
  assert_background_matches_secondary(&written_config.profiles[0].rgb_config);
  TEST_ASSERT_EQUAL_UINT8(90, written_config.profiles[0].rgb_config.effect_speed);
  TEST_ASSERT_EQUAL_UINT8(0, written_config.profiles[0].rgb_config.sleep_timeout);
  TEST_ASSERT_EQUAL_UINT8(0, written_config.profiles[0].rgb_config.layer_indicator_mode);
  TEST_ASSERT_EQUAL_UINT8(0, written_config.profiles[0].rgb_config.layer_indicator_key);
  assert_rgb_per_key_color(&written_config.profiles[0].rgb_config, 0, 0);
  assert_rgb_per_key_color(&written_config.profiles[0].rgb_config, 0, 9);
}

void test_migration_v1_B_promotes_options_and_preserves_layer_colors(void) {
  build_legacy_config_v1_B(0x0A55);

  TEST_ASSERT_TRUE(migration_try_migrate());
  TEST_ASSERT_EQUAL_HEX16(EECONFIG_VERSION, written_config.version);
  TEST_ASSERT_EQUAL_HEX32(0x00000A55, written_config.options.raw);

  const eeconfig_profile_t *profile = &written_config.profiles[2];
  TEST_ASSERT_EQUAL_UINT8(102, profile->rgb_config.layer_colors[0].r);
  TEST_ASSERT_EQUAL_UINT8(103, profile->rgb_config.layer_colors[0].g);
  TEST_ASSERT_EQUAL_UINT8(104, profile->rgb_config.layer_colors[0].b);
  TEST_ASSERT_EQUAL_UINT8(105, profile->rgb_config.layer_colors[1].r);
  TEST_ASSERT_EQUAL_UINT8(255, profile->rgb_config.secondary_color.r);
  TEST_ASSERT_EQUAL_UINT8(255, profile->rgb_config.secondary_color.g);
  TEST_ASSERT_EQUAL_UINT8(255, profile->rgb_config.secondary_color.b);
  assert_background_matches_secondary(&profile->rgb_config);
  TEST_ASSERT_EQUAL_UINT8(34, profile->rgb_config.layer_indicator_key);
  assert_rgb_per_key_color(&profile->rgb_config, 37, 0);
  TEST_ASSERT_EQUAL_UINT8(202, profile->joystick_config.mouse_acceleration);
  TEST_ASSERT_EQUAL_UINT8(7, profile->joystick_config.sw_debounce_ms);
  assert_default_radial_boundaries(&profile->joystick_config);
  assert_mouse_presets_match_active(&profile->joystick_config);
}

void test_migration_v1_D_initializes_joystick_debounce_without_clobbering_other_fields(
    void) {
  build_legacy_config_v1_D(0x00001234);

  TEST_ASSERT_TRUE(migration_try_migrate());
  TEST_ASSERT_EQUAL_HEX16(EECONFIG_VERSION, written_config.version);
  TEST_ASSERT_EQUAL_HEX32(0x00001234, written_config.options.raw);

  const eeconfig_profile_t *profile = &written_config.profiles[0];
  TEST_ASSERT_EQUAL_UINT8(70, profile->rgb_config.secondary_color.r);
  TEST_ASSERT_EQUAL_UINT8(80, profile->rgb_config.secondary_color.g);
  TEST_ASSERT_EQUAL_UINT8(90, profile->rgb_config.secondary_color.b);
  assert_background_matches_secondary(&profile->rgb_config);
  TEST_ASSERT_EQUAL_UINT8(200, profile->joystick_config.mouse_acceleration);
  TEST_ASSERT_EQUAL_UINT8(5, profile->joystick_config.sw_debounce_ms);
  TEST_ASSERT_EQUAL_UINT8(0xA0, profile->joystick_config.reserved[0]);
  assert_default_radial_boundaries(&profile->joystick_config);
  assert_mouse_presets_match_active(&profile->joystick_config);
}

void test_migration_v1_10_appends_trigger_state_colors_without_clobbering_profile_data(
    void) {
  build_legacy_config_v1_10(0x12345678u);

  TEST_ASSERT_TRUE(migration_try_migrate());
  TEST_ASSERT_EQUAL_HEX16(EECONFIG_VERSION, written_config.version);
  TEST_ASSERT_EQUAL_HEX32(0x12345678u, written_config.options.raw);

  const eeconfig_profile_t *profile = &written_config.profiles[1];
  TEST_ASSERT_EQUAL_UINT8(91, profile->rgb_config.secondary_color.r);
  TEST_ASSERT_EQUAL_UINT8(101, profile->rgb_config.secondary_color.g);
  TEST_ASSERT_EQUAL_UINT8(111, profile->rgb_config.secondary_color.b);
  assert_background_matches_secondary(&profile->rgb_config);
  assert_trigger_state_defaults_from_legacy(
      &profile->rgb_config,
      (rgb_color_t){.r = 31, .g = 41, .b = 51},
      (rgb_color_t){.r = 91, .g = 101, .b = 111});
  TEST_ASSERT_EQUAL_UINT16(516, profile->joystick_config.x.min);
  TEST_ASSERT_EQUAL_UINT8(56, profile->joystick_config.mouse_speed);
  TEST_ASSERT_EQUAL_UINT8(86, profile->joystick_config.mouse_presets[2].mouse_speed);
  TEST_ASSERT_EQUAL_UINT8(156,
                          profile->joystick_config.mouse_presets[2].mouse_acceleration);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_migration_rejects_invalid_magic);
  RUN_TEST(test_migration_v1_0_reaches_current_and_preserves_profile_data);
  RUN_TEST(test_migration_v1_8_null_migration_preserves_rgb_and_joystick_blocks);
  RUN_TEST(test_migration_v1_9_preserves_rgb_base_fields_and_per_key_colors);
  RUN_TEST(test_migration_v1_B_promotes_options_and_preserves_layer_colors);
  RUN_TEST(
      test_migration_v1_D_initializes_joystick_debounce_without_clobbering_other_fields);
  RUN_TEST(
      test_migration_v1_10_appends_trigger_state_colors_without_clobbering_profile_data);
  return UNITY_END();
}
