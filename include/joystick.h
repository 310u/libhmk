#pragma once

#include "common.h"

// 0: Mouse movement
// 1: XInput Left Stick
// 2: XInput Right Stick
typedef enum {
    JOYSTICK_MODE_DISABLED = 0,
    JOYSTICK_MODE_MOUSE = 1,
    JOYSTICK_MODE_XINPUT_LS = 2,
    JOYSTICK_MODE_XINPUT_RS = 3,
    JOYSTICK_MODE_SCROLL = 4,
    JOYSTICK_MODE_CURSOR_4 = 5,
    JOYSTICK_MODE_CURSOR_8 = 6,
} joystick_mode_t;

typedef struct __attribute__((packed)) {
    uint16_t min;
    uint16_t center;
    uint16_t max;
} joystick_axis_calibration_t;

#define JOYSTICK_RADIAL_BOUNDARY_SECTORS 32u
#define JOYSTICK_RADIAL_BOUNDARY_DEFAULT 127u
#define JOYSTICK_MOUSE_SPEED_DEFAULT 10u
#define JOYSTICK_MOUSE_ACCELERATION_DEFAULT 255u
#define JOYSTICK_MOUSE_PRESET_COUNT 4u

typedef struct __attribute__((packed)) {
    uint8_t mouse_speed;
    uint8_t mouse_acceleration;
} joystick_mouse_preset_t;

typedef struct __attribute__((packed)) {
    joystick_axis_calibration_t x;
    joystick_axis_calibration_t y;
    uint8_t deadzone;
    uint8_t mode; // joystick_mode_t
    uint8_t mouse_speed; // 1-255
    uint8_t mouse_acceleration; // 1-255, 255 = strongest acceleration
    uint8_t sw_debounce_ms; // Push switch debounce time in ms (0 = disabled)
    uint8_t reserved[3];
    uint8_t radial_boundaries[32];
    uint8_t active_mouse_preset;
    joystick_mouse_preset_t mouse_presets[JOYSTICK_MOUSE_PRESET_COUNT];
} joystick_config_t;

_Static_assert(sizeof(((joystick_config_t *)0)->radial_boundaries) ==
                   JOYSTICK_RADIAL_BOUNDARY_SECTORS,
               "Invalid joystick radial boundary table size");
#define JOYSTICK_CONFIG_CURRENT_SIZE sizeof(joystick_config_t)
#define JOYSTICK_CONFIG_LEGACY_SIZE \
  offsetof(joystick_config_t, radial_boundaries)
_Static_assert(JOYSTICK_CONFIG_LEGACY_SIZE == 20u,
               "joystick_config_t legacy prefix mismatch");
_Static_assert(sizeof(joystick_mouse_preset_t) == 2u,
               "joystick_mouse_preset_t size mismatch");
_Static_assert(sizeof(joystick_config_t) == 61u,
               "joystick_config_t size mismatch");

static inline void
joystick_fill_default_radial_boundaries(uint8_t boundaries[JOYSTICK_RADIAL_BOUNDARY_SECTORS]) {
  for (uint8_t i = 0; i < JOYSTICK_RADIAL_BOUNDARY_SECTORS; i++) {
    boundaries[i] = JOYSTICK_RADIAL_BOUNDARY_DEFAULT;
  }
}

static inline void joystick_fill_default_mouse_presets(
    joystick_mouse_preset_t presets[JOYSTICK_MOUSE_PRESET_COUNT],
    uint8_t mouse_speed, uint8_t mouse_acceleration) {
  const uint8_t speed =
      mouse_speed == 0u ? JOYSTICK_MOUSE_SPEED_DEFAULT : mouse_speed;
  const uint8_t acceleration = mouse_acceleration == 0u
                                   ? JOYSTICK_MOUSE_ACCELERATION_DEFAULT
                                   : mouse_acceleration;

  for (uint8_t i = 0; i < JOYSTICK_MOUSE_PRESET_COUNT; i++) {
    presets[i] = (joystick_mouse_preset_t){
        .mouse_speed = speed,
        .mouse_acceleration = acceleration,
    };
  }
}

static inline void joystick_init_default_config(joystick_config_t *config) {
  memset(config, 0, sizeof(*config));
  config->x = (joystick_axis_calibration_t){0, 2048, 4095};
  config->y = (joystick_axis_calibration_t){0, 2048, 4095};
  config->deadzone = 150;
  config->mode = JOYSTICK_MODE_MOUSE;
  config->mouse_speed = JOYSTICK_MOUSE_SPEED_DEFAULT;
  config->mouse_acceleration = JOYSTICK_MOUSE_ACCELERATION_DEFAULT;
  config->sw_debounce_ms = 5;
  joystick_fill_default_radial_boundaries(config->radial_boundaries);
  config->active_mouse_preset = 0;
  joystick_fill_default_mouse_presets(config->mouse_presets,
                                      config->mouse_speed,
                                      config->mouse_acceleration);
}

typedef struct {
    uint16_t raw_x;
    uint16_t raw_y;
    int8_t out_x;
    int8_t out_y;
    bool sw;
} joystick_state_t;

void joystick_init(void);
void joystick_task(void);
joystick_state_t joystick_get_state(void);
joystick_config_t joystick_get_config(void);
joystick_config_t joystick_normalize_config(joystick_config_t config);
void joystick_select_mouse_preset(joystick_config_t *config,
                                  uint8_t preset_index);
void joystick_apply_config(joystick_config_t config);
void joystick_set_config(joystick_config_t config);
