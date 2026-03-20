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
} joystick_config_t;

_Static_assert(sizeof(joystick_config_t) == 20, "joystick_config_t size mismatch");

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
