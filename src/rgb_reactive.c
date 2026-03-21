/*
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "rgb_reactive.h"

#if defined(RGB_ENABLED)

#if defined(NUM_LEDS)
#else
#define NUM_LEDS NUM_KEYS
#endif

#include <math.h>

#include "hardware/hardware.h"
#include "rgb_internal.h"
#include "rgb_math.h"

static uint8_t rgb_heatmap[NUM_LEDS] = {0};

#define RGB_LAST_HITS 10
typedef struct {
  uint8_t index;
  uint8_t x;
  uint8_t y;
  uint32_t time_ms;
} rgb_hit_t;

static rgb_hit_t rgb_last_hits[RGB_LAST_HITS] = {0};
static uint8_t rgb_last_hits_count = 0;
static uint32_t heatmap_tick = 0;

typedef enum {
  REACTIVE_MODE_SIMPLE = 0,
  REACTIVE_MODE_WIDE,
  REACTIVE_MODE_CROSS,
  REACTIVE_MODE_NEXUS,
} reactive_mode_t;

static reactive_mode_t reactive_mode_from_effect(uint8_t effect) {
  switch (effect) {
  case RGB_EFFECT_SOLID_REACTIVE_WIDE:
  case RGB_EFFECT_SOLID_REACTIVE_MULTIWIDE:
    return REACTIVE_MODE_WIDE;
  case RGB_EFFECT_SOLID_REACTIVE_CROSS:
  case RGB_EFFECT_SOLID_REACTIVE_MULTICROSS:
    return REACTIVE_MODE_CROSS;
  case RGB_EFFECT_SOLID_REACTIVE_NEXUS:
  case RGB_EFFECT_SOLID_REACTIVE_MULTINEXUS:
    return REACTIVE_MODE_NEXUS;
  default:
    return REACTIVE_MODE_SIMPLE;
  }
}

static bool reactive_is_multi(uint8_t effect) {
  return effect == RGB_EFFECT_SOLID_REACTIVE_MULTIWIDE ||
         effect == RGB_EFFECT_SOLID_REACTIVE_MULTICROSS ||
         effect == RGB_EFFECT_SOLID_REACTIVE_MULTINEXUS;
}

static bool splash_is_multi(uint8_t effect) {
  return effect == RGB_EFFECT_MULTISPLASH ||
         effect == RGB_EFFECT_SOLID_MULTISPLASH;
}

static uint8_t hit_elapsed_tick(const rgb_hit_t *hit, uint8_t speed) {
  const uint32_t elapsed = timer_elapsed(hit->time_ms);
  const uint32_t t = (elapsed * (uint32_t)qadd8(speed, 8u)) / 16u;
  return (t > 255u) ? 255u : (uint8_t)t;
}

static uint8_t reactive_clip_scale(uint8_t source_led, uint8_t target_led,
                                   uint8_t value) {
  return scale8(value, rgb_reactive_clip_at(source_led, target_led));
}

static uint8_t reactive_clip_effect(uint8_t target_led, const rgb_hit_t *hit,
                                    uint8_t effect) {
  uint8_t visible = (uint8_t)(255u - effect);
  visible = reactive_clip_scale(hit->index, target_led, visible);
  return (uint8_t)(255u - visible);
}

static uint8_t reactive_strength_for_hit(uint8_t led, const rgb_hit_t *hit,
                                         reactive_mode_t mode, uint8_t speed) {
  const uint8_t tick = hit_elapsed_tick(hit, speed);
  const int16_t dx = (int16_t)rgb_coord_x_at(led) - (int16_t)hit->x;
  const int16_t dy = (int16_t)rgb_coord_y_at(led) - (int16_t)hit->y;
  const uint8_t dist = (uint8_t)sqrt(dx * dx + dy * dy);
  int16_t effect = tick;

  switch (mode) {
  case REACTIVE_MODE_SIMPLE:
    effect = tick;
    break;
  case REACTIVE_MODE_WIDE:
    effect = (int16_t)(tick + dist * 5u);
    break;
  case REACTIVE_MODE_CROSS: {
    uint16_t ax = (dx < 0) ? (uint16_t)(-dx) : (uint16_t)dx;
    uint16_t ay = (dy < 0) ? (uint16_t)(-dy) : (uint16_t)dy;
    ax = (ax * 16u > 255u) ? 255u : ax * 16u;
    ay = (ay * 16u > 255u) ? 255u : ay * 16u;
    effect = (int16_t)(tick + ((ax > ay) ? ay : ax));
    break;
  }
  case REACTIVE_MODE_NEXUS:
    effect = (int16_t)(tick - dist);
    if (effect < 0) {
      effect = 255;
    }
    if (dist > 72u) {
      effect = 255;
    }
    if ((dx > 8 || dx < -8) && (dy > 8 || dy < -8)) {
      effect = 255;
    }
    break;
  default:
    effect = tick;
    break;
  }

  if (effect > 255) {
    effect = 255;
  }
  return reactive_clip_effect(led, hit, (uint8_t)effect);
}

static uint8_t splash_strength_for_hit(uint8_t led, const rgb_hit_t *hit,
                                       uint8_t speed) {
  const uint8_t tick = hit_elapsed_tick(hit, speed);
  const int16_t dx = (int16_t)rgb_coord_x_at(led) - (int16_t)hit->x;
  const int16_t dy = (int16_t)rgb_coord_y_at(led) - (int16_t)hit->y;
  const uint8_t dist = (uint8_t)sqrt(dx * dx + dy * dy);
  int16_t effect = (int16_t)tick - dist;
  if (effect < 0) {
    effect = 255;
  }
  if (effect > 255) {
    effect = 255;
  }
  return reactive_clip_effect(led, hit, (uint8_t)effect);
}

static uint8_t compute_reactive_intensity(uint8_t led, uint8_t effect,
                                          uint8_t speed) {
  const reactive_mode_t mode = reactive_mode_from_effect(effect);
  if (rgb_last_hits_count == 0) {
    return 0;
  }

  if (!reactive_is_multi(effect)) {
    return reactive_strength_for_hit(
        led, &rgb_last_hits[rgb_last_hits_count - 1u], mode, speed);
  }

  uint8_t best = 255;
  for (uint8_t i = 0; i < rgb_last_hits_count; i++) {
    const uint8_t v =
        reactive_strength_for_hit(led, &rgb_last_hits[i], mode, speed);
    if (v < best) {
      best = v;
    }
  }
  return best;
}

static uint8_t compute_splash_intensity(uint8_t led, uint8_t effect,
                                        uint8_t speed) {
  if (rgb_last_hits_count == 0) {
    return 0;
  }

  if (!splash_is_multi(effect)) {
    return splash_strength_for_hit(led, &rgb_last_hits[rgb_last_hits_count - 1u],
                                   speed);
  }

  uint8_t best = 255;
  for (uint8_t i = 0; i < rgb_last_hits_count; i++) {
    const uint8_t v = splash_strength_for_hit(led, &rgb_last_hits[i], speed);
    if (v < best) {
      best = v;
    }
  }
  return best;
}

void rgb_reactive_decay_heatmap(uint32_t current_tick) {
  if (timer_elapsed(heatmap_tick) < 25u) {
    return;
  }

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if (rgb_heatmap[i] > 0u) {
      rgb_heatmap[i] = qsub8(rgb_heatmap[i], 1u);
    }
  }

  heatmap_tick = current_tick;
}

void rgb_reactive_record_keypress(uint8_t index) {
  if (index >= NUM_KEYS) {
    return;
  }

  const uint8_t led_index = rgb_key_to_led_at(index);
  if (led_index >= NUM_LEDS) {
    return;
  }

  const rgb_hit_t hit = {
      .index = led_index,
      .x = rgb_coord_x_at(led_index),
      .y = rgb_coord_y_at(led_index),
      .time_ms = timer_read(),
  };

  if (rgb_last_hits_count < RGB_LAST_HITS) {
    rgb_last_hits[rgb_last_hits_count++] = hit;
  } else {
    for (uint8_t i = 1; i < RGB_LAST_HITS; i++) {
      rgb_last_hits[i - 1u] = rgb_last_hits[i];
    }
    rgb_last_hits[RGB_LAST_HITS - 1u] = hit;
  }

  rgb_heatmap[led_index] = qadd8(rgb_heatmap[led_index], 32u);

  const uint8_t source_x = rgb_coord_x_at(led_index);
  const uint8_t source_y = rgb_coord_y_at(led_index);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if (i == led_index) {
      continue;
    }

    const int dx = source_x - rgb_coord_x_at(i);
    const int dy = source_y - rgb_coord_y_at(i);
    const uint8_t distance = (uint8_t)sqrt(dx * dx + dy * dy);
    if (distance > 40u) {
      continue;
    }

    uint8_t amount = qsub8(40u, distance);
    if (amount > 16u) {
      amount = 16u;
    }
    amount = reactive_clip_scale(led_index, i, amount);
    rgb_heatmap[i] = qadd8(rgb_heatmap[i], amount);
  }
}

void rgb_reactive_render_heatmap(uint8_t effective_brightness) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t temp = rgb_heatmap[i];
    if (temp == 0u) {
      rgb_set_color(i, 0, 0, 0);
      continue;
    }

    const uint8_t sub = qsub8(temp, 85u);
    const uint8_t hue = (170u > sub) ? (uint8_t)(170u - sub) : 0u;
    const uint8_t heat = qsub8(qadd8(170u, temp), 170u);
    const uint8_t v = scale8((uint8_t)(heat * 3u), effective_brightness);
    const hsv_t hsv = {.h = hue, .s = 255, .v = v};
    const rgb_color_t color = hsv_to_rgb(hsv);
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

void rgb_reactive_render_effect(uint8_t effect, uint8_t base_hue,
                                uint8_t effective_brightness, uint8_t speed) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t intensity = compute_reactive_intensity(i, effect, speed);
    hsv_t hsv = {.h = base_hue, .s = 255, .v = effective_brightness};
    if (effect == RGB_EFFECT_SOLID_REACTIVE_SIMPLE) {
      hsv.v = scale8((uint8_t)(255u - intensity), hsv.v);
    } else if (effect == RGB_EFFECT_SOLID_REACTIVE) {
      hsv.h = (uint8_t)(base_hue +
                        scale8((uint8_t)(255u - intensity), 64u));
    } else {
      if (effect == RGB_EFFECT_SOLID_REACTIVE_NEXUS ||
          effect == RGB_EFFECT_SOLID_REACTIVE_MULTINEXUS) {
        const int16_t dy = (int16_t)rgb_coord_y_at(i) - 127;
        hsv.h = (uint8_t)(base_hue + (dy / 4));
      }
      hsv.v = qadd8(hsv.v, (uint8_t)(255u - intensity));
    }

    const rgb_color_t color = hsv_to_rgb(hsv);
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

void rgb_reactive_render_splash(uint8_t effect, uint8_t base_hue,
                                uint8_t effective_brightness, uint8_t speed) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t intensity = compute_splash_intensity(i, effect, speed);
    hsv_t hsv = {.h = base_hue, .s = 255, .v = effective_brightness};
    if (effect == RGB_EFFECT_SPLASH || effect == RGB_EFFECT_MULTISPLASH) {
      hsv.h = (uint8_t)(hsv.h + intensity);
    }
    hsv.v = qadd8(hsv.v, (uint8_t)(255u - intensity));
    const rgb_color_t color = hsv_to_rgb(hsv);
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

#endif
