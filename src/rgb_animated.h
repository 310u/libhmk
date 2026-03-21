#pragma once

#include "rgb.h"

#if defined(RGB_ENABLED)

typedef struct {
    uint8_t base_hue;
    uint8_t effective_brightness;
    uint8_t effect_speed;
    bool effect_changed;
} rgb_animated_context_t;

void rgb_animated_reset(void);
void rgb_animated_render(rgb_effect_t effect,
                         const rgb_animated_context_t *context);

#endif
