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

  memcpy(*dst, curve, sizeof(curve));
  *dst += sizeof(curve);
  write_u8(dst, options);
}

static void write_legacy_profile_v1_0(uint8_t **dst, uint8_t seed) {
  write_legacy_keymap(dst, seed);
  write_legacy_actuation(dst, (uint8_t)(seed + 32));
  write_legacy_advanced_keys(dst, 12, (uint8_t)(seed + 64));
  write_u8(dst, (uint8_t)(30 + seed));
}

static void write_legacy_profile_v1_9(uint8_t **dst, uint8_t seed) {
  write_legacy_keymap(dst, seed);
  write_legacy_actuation(dst, (uint8_t)(seed + 32));
  write_legacy_advanced_keys(dst, 13, (uint8_t)(seed + 64));
  write_fill(dst, (uint8_t)(seed + 96), NUM_KEYS);
  write_legacy_gamepad_options(dst, 0b00001001);
  write_u8(dst, (uint8_t)(24 + seed));
  write_legacy_macros(dst, (uint8_t)(seed + 112));

  write_u8(dst, 1);                          // enabled
  write_u8(dst, (uint8_t)(40 + seed));      // brightness
  write_u8(dst, RGB_EFFECT_ALPHAS_MODS);    // effect
  write_u8(dst, (uint8_t)(10 + seed));      // solid r
  write_u8(dst, (uint8_t)(20 + seed));      // solid g
  write_u8(dst, (uint8_t)(30 + seed));      // solid b
  write_u8(dst, (uint8_t)(90 + seed));      // speed

  for (uint32_t i = 0; i < NUM_KEYS; i++) {
    write_u8(dst, (uint8_t)(seed + i));
    write_u8(dst, (uint8_t)(seed + i + 1));
    write_u8(dst, (uint8_t)(seed + i + 2));
  }

  write_fill(dst, 0, sizeof(joystick_config_t));
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
  TEST_ASSERT_EQUAL_UINT8(90, written_config.profiles[0].rgb_config.effect_speed);
  TEST_ASSERT_EQUAL_UINT8(0, written_config.profiles[0].rgb_config.sleep_timeout);
  TEST_ASSERT_EQUAL_UINT8(0, written_config.profiles[0].rgb_config.layer_indicator_mode);
  TEST_ASSERT_EQUAL_UINT8(0, written_config.profiles[0].rgb_config.layer_indicator_key);

  TEST_ASSERT_EQUAL_UINT8(0, written_config.profiles[0].rgb_config.per_key_colors[0].r);
  TEST_ASSERT_EQUAL_UINT8(1, written_config.profiles[0].rgb_config.per_key_colors[0].g);
  TEST_ASSERT_EQUAL_UINT8(2, written_config.profiles[0].rgb_config.per_key_colors[0].b);
  TEST_ASSERT_EQUAL_UINT8(9, written_config.profiles[0].rgb_config.per_key_colors[9].r);
  TEST_ASSERT_EQUAL_UINT8(10, written_config.profiles[0].rgb_config.per_key_colors[9].g);
  TEST_ASSERT_EQUAL_UINT8(11, written_config.profiles[0].rgb_config.per_key_colors[9].b);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_migration_rejects_invalid_magic);
  RUN_TEST(test_migration_v1_0_reaches_current_and_preserves_profile_data);
  RUN_TEST(test_migration_v1_9_preserves_rgb_base_fields_and_per_key_colors);
  return UNITY_END();
}
