#pragma once

#include "rgb.h"

void rgb_reactive_decay_heatmap(uint32_t current_tick);
void rgb_reactive_record_keypress(uint8_t index);
void rgb_reactive_render_heatmap(uint8_t effective_brightness);
void rgb_reactive_render_effect(uint8_t effect, uint8_t base_hue,
                                uint8_t effective_brightness, uint8_t speed);
void rgb_reactive_render_splash(uint8_t effect, uint8_t base_hue,
                                uint8_t effective_brightness, uint8_t speed);
