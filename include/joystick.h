#pragma once

#include "common.h"

// 0: Mouse movement
// 1: XInput Left Stick
// 2: XInput Right Stick
typedef enum {
    JOYSTICK_MODE_MOUSE = 0,
    JOYSTICK_MODE_XINPUT_LS = 1,
    JOYSTICK_MODE_XINPUT_RS = 2,
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
    uint8_t reserved[5];
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
void joystick_set_config(joystick_config_t config);
