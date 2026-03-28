#pragma once

#include <stdbool.h>

#include "rgb.h"

#if defined(RGB_ENABLED)

typedef struct {
  const rgb_config_t *config;
  uint8_t base_hue;
  uint8_t secondary_hue;
  uint8_t effective_brightness;
  uint32_t anim_timer;
  uint16_t scaled_timer;
  uint16_t tick;
  uint16_t prev_tick;
} rgb_static_context_t;

void rgb_static_reset(void);
bool rgb_static_render(rgb_effect_t effect, const rgb_static_context_t *context);

#endif
