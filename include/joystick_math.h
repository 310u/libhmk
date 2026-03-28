#pragma once

#include "joystick.h"

float joystick_boundary_lookup(const uint8_t *boundaries, float sector);
void joystick_apply_circular_correction_fp(const uint8_t *boundaries,
                                           int32_t *x_fp,
                                           int32_t *y_fp);
void joystick_apply_radial_deadzone_fp(int32_t *x_fp, int32_t *y_fp,
                                       uint8_t deadzone);
