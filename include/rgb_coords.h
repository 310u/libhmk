#pragma once

#include "rgb.h"

#if defined(RGB_ENABLED)

typedef struct {
    uint8_t x;
    uint8_t y;
} led_point_t;

// Example layout, evenly spaced for mochiko40he (40 LEDs)
// 1U = 12 units.
// 10 columns * 12 = 120 width
// 4 rows * 12 = 48 height
const led_point_t rgb_led_coords[NUM_KEYS] = {
    // Row 0
    {0, 0}, {12, 0}, {24, 0}, {36, 0}, {48, 0}, {60, 0}, {72, 0}, {84, 0}, {96, 0}, {108, 0},
    // Row 1
    {0, 12}, {12, 12}, {24, 12}, {36, 12}, {48, 12}, {60, 12}, {72, 12}, {84, 12}, {96, 12}, {108, 12},
    // Row 2
    {0, 24}, {12, 24}, {24, 24}, {36, 24}, {48, 24}, {60, 24}, {72, 24}, {84, 24}, {96, 24}, {108, 24},
    // Row 3 (Bottom row)
    {0, 36}, {12, 36}, {24, 36}, {36, 36}, {48, 36}, {60, 36}, {72, 36}, {84, 36}, {96, 36}, {108, 36}
};

#endif
