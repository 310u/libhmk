#pragma once

#include <stdbool.h>

#include "rgb.h"

uint8_t rgb_coord_x_at(uint8_t led);
uint8_t rgb_coord_y_at(uint8_t led);
uint8_t rgb_key_to_led_at(uint8_t key);
uint8_t rgb_reactive_clip_at(uint8_t source_led, uint8_t target_led);
