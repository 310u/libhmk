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
} joystick_config_t;

#define JOYSTICK_RADIAL_BOUNDARY_SECTORS 32u
#define JOYSTICK_RADIAL_BOUNDARY_DEFAULT 127u

_Static_assert(sizeof(((joystick_config_t *)0)->radial_boundaries) ==
                   JOYSTICK_RADIAL_BOUNDARY_SECTORS,
               "Invalid joystick radial boundary table size");
#define JOYSTICK_CONFIG_LEGACY_SIZE \
  offsetof(joystick_config_t, radial_boundaries)
_Static_assert(JOYSTICK_CONFIG_LEGACY_SIZE == 20u,
               "joystick_config_t legacy prefix mismatch");
_Static_assert(sizeof(joystick_config_t) == 52u,
               "joystick_config_t size mismatch");

static inline void
joystick_fill_default_radial_boundaries(uint8_t boundaries[JOYSTICK_RADIAL_BOUNDARY_SECTORS]) {
  for (uint8_t i = 0; i < JOYSTICK_RADIAL_BOUNDARY_SECTORS; i++) {
    boundaries[i] = JOYSTICK_RADIAL_BOUNDARY_DEFAULT;
  }
}

static inline void joystick_init_default_config(joystick_config_t *config) {
  memset(config, 0, sizeof(*config));
  config->x = (joystick_axis_calibration_t){0, 2048, 4095};
  config->y = (joystick_axis_calibration_t){0, 2048, 4095};
  config->deadzone = 150;
  config->mode = JOYSTICK_MODE_MOUSE;
  config->mouse_speed = 10;
  config->mouse_acceleration = 255;
  config->sw_debounce_ms = 5;
  joystick_fill_default_radial_boundaries(config->radial_boundaries);
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
void joystick_apply_config(joystick_config_t config);
void joystick_set_config(joystick_config_t config);
