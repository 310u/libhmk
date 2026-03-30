#include "rgb_static.h"

#if defined(RGB_ENABLED)

#if defined(NUM_LEDS)
#else
#define NUM_LEDS NUM_KEYS
#endif

#include "lib/usqrt.h"
#include "rgb_internal.h"
#include "rgb_math.h"

static uint32_t random_state = 0x12345678u;

static uint8_t random8(void) {
  random_state = random_state * 1103515245u + 12345u;
  return (uint8_t)(random_state >> 16);
}

static uint8_t random8_max(uint8_t max) {
  if (max == 0u) {
    return 0u;
  }

  return (uint8_t)(random8() % max);
}

static uint8_t random8_min_max(uint8_t min, uint8_t max) {
  if (min >= max) {
    return min;
  }

  return (uint8_t)(min + (random8() % (uint8_t)(max - min)));
}

static rgb_color_t rgb_static_scale_color(rgb_color_t color,
                                          uint8_t brightness) {
  return (rgb_color_t){
      .r = (uint8_t)(((uint32_t)color.r * brightness) / 255u),
      .g = (uint8_t)(((uint32_t)color.g * brightness) / 255u),
      .b = (uint8_t)(((uint32_t)color.b * brightness) / 255u),
  };
}

static int16_t rgb_static_centered_x(uint8_t led) {
  return (int16_t)rgb_coord_x_at(led) - 127;
}

static int16_t rgb_static_centered_y(uint8_t led) {
  return (int16_t)rgb_coord_y_at(led) - 127;
}

static uint8_t rgb_static_distance(int16_t dx, int16_t dy) {
  const uint32_t dx_sq = (uint32_t)((int32_t)dx * (int32_t)dx);
  const uint32_t dy_sq = (uint32_t)((int32_t)dy * (int32_t)dy);
  return (uint8_t)usqrt32(dx_sq + dy_sq);
}

static uint8_t rgb_static_led_distance(uint8_t led) {
  const int16_t dx = rgb_static_centered_x(led);
  const int16_t dy = rgb_static_centered_y(led);
  return rgb_static_distance(dx, dy);
}

static uint8_t rgb_static_led_angle(uint8_t led) {
  const int16_t dx = rgb_static_centered_x(led);
  const int16_t dy = rgb_static_centered_y(led);
  return (uint8_t)(((atan2((double)dy, (double)dx) + M_PI) * 255.0) /
                   (2.0 * M_PI));
}

static void rgb_static_render_band_axis(const rgb_static_context_t *context,
                                        bool saturate) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t x = rgb_coord_x_at(i);
    int16_t dist = (int16_t)x + 28 - (int16_t)(context->scaled_timer >> 8);
    if (dist < 0) {
      dist = (int16_t)(-dist);
    }

    int16_t value = 255 - dist * 8;
    if (value < 0) {
      value = 0;
    }

    hsv_t hsv = {
        .h = context->base_hue,
        .s = saturate ? (uint8_t)value : 255u,
        .v = saturate ? context->effective_brightness
                      : (uint8_t)(((uint32_t)(uint16_t)value *
                                   context->effective_brightness) /
                                  255u),
    };
    const rgb_color_t color = hsv_to_rgb(hsv);
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

static void rgb_static_render_band_polar(const rgb_static_context_t *context,
                                         rgb_effect_t effect) {
  const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
  const bool spiral = effect == RGB_EFFECT_BAND_SPIRAL_SAT ||
                      effect == RGB_EFFECT_BAND_SPIRAL_VAL;
  const bool saturate = effect == RGB_EFFECT_BAND_PINWHEEL_SAT ||
                        effect == RGB_EFFECT_BAND_SPIRAL_SAT;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t dist = rgb_static_led_distance(i);
    uint8_t offset = rgb_static_led_angle(i);
    if (spiral) {
      offset = (uint8_t)(offset + dist);
    }

    int16_t value = 255 - abs((int16_t)offset - (int16_t)time) * 8;
    if (value < 0) {
      value = 0;
    }

    hsv_t hsv = {
        .h = context->base_hue,
        .s = saturate ? (uint8_t)value : 255u,
        .v = saturate ? context->effective_brightness
                      : (uint8_t)(((uint32_t)(uint16_t)value *
                                   context->effective_brightness) /
                                  255u),
    };
    const rgb_color_t color = hsv_to_rgb(hsv);
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

static void rgb_static_render_gradient(const rgb_static_context_t *context,
                                       bool vertical) {
  const uint8_t scale = scale8(64u, context->config->effect_speed);

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t coord = vertical ? rgb_coord_y_at(i) : rgb_coord_x_at(i);
    const uint8_t hue =
        vertical ? (uint8_t)(context->base_hue + scale * (coord >> 4))
                 : (uint8_t)(context->base_hue +
                             (((uint16_t)scale * coord) >> 5));
    const rgb_color_t color = hsv_to_rgb((hsv_t){
        .h = hue,
        .s = 255u,
        .v = context->effective_brightness,
    });
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

static void rgb_static_render_axis_cycle(const rgb_static_context_t *context,
                                         bool vertical) {
  const uint8_t time = (uint8_t)(context->scaled_timer >> 8);

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t coord = vertical ? rgb_coord_y_at(i) : rgb_coord_x_at(i);
    const rgb_color_t color = hsv_to_rgb((hsv_t){
        .h = (uint8_t)(coord - time),
        .s = 255u,
        .v = context->effective_brightness,
    });
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

static void rgb_static_render_cycle_out_in(const rgb_static_context_t *context,
                                           bool dual) {
  const uint8_t time = (uint8_t)(context->scaled_timer >> 8);

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t hue;

    if (dual) {
      const int16_t dx = rgb_static_centered_x(i);
      const int16_t dy = rgb_static_centered_y(i);
      const int16_t centered = 63 - (int16_t)abs8(dx);
      const uint8_t dist = rgb_static_distance(centered, dy);
      const uint8_t ring_hue = (uint8_t)(3u * dist + time);
      hue = (ring_hue & 0x80u) ? context->secondary_hue : ring_hue;
    } else {
      const uint8_t dist = rgb_static_led_distance(i);
      hue = (uint8_t)((3u * dist / 2u) + time);
    }

    const rgb_color_t color = hsv_to_rgb((hsv_t){
        .h = hue,
        .s = 255u,
        .v = context->effective_brightness,
    });
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

static void rgb_static_render_cycle_polar(const rgb_static_context_t *context,
                                          bool spiral) {
  const uint8_t time = (uint8_t)(context->scaled_timer >> 8);

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    const uint8_t angle = rgb_static_led_angle(i);
    const uint8_t dist = rgb_static_led_distance(i);
    const uint8_t hue =
        spiral ? (uint8_t)(dist - time - angle) : (uint8_t)(angle + time);
    const rgb_color_t color = hsv_to_rgb((hsv_t){
        .h = hue,
        .s = 255u,
        .v = context->effective_brightness,
    });
    rgb_set_color(i, color.r, color.g, color.b);
  }
}

static void rgb_static_render_starlight_family(
    const rgb_static_context_t *context, rgb_effect_t effect) {
  const uint8_t time = scale8((uint8_t)(context->scaled_timer >> 8),
                              (uint8_t)(context->config->effect_speed >> 3));
  const uint8_t value = scale8(
      (uint8_t)(abs8((int16_t)sin8(time) - 128) << 1),
      context->effective_brightness);

  if ((context->tick % 5u != 0u) || context->tick == context->prev_tick) {
    return;
  }

  const uint8_t rand_idx = random8_max(NUM_LEDS);
  hsv_t hsv = {.h = context->base_hue, .s = 255u, .v = value};

  if (effect == RGB_EFFECT_STARLIGHT_DUAL_HUE) {
    hsv.h = (uint8_t)(hsv.h + random8_max(31u));
  } else if (effect == RGB_EFFECT_STARLIGHT_DUAL_SAT) {
    hsv.s = (uint8_t)(hsv.s + random8_max(31u));
  }

  const rgb_color_t color = hsv_to_rgb(hsv);
  rgb_set_color(rand_idx, color.r, color.g, color.b);
}

void rgb_static_reset(void) { random_state = 0x12345678u; }

bool rgb_static_render(rgb_effect_t effect, const rgb_static_context_t *context) {
  switch (effect) {
  case RGB_EFFECT_OFF:
    rgb_set_all_color(0u, 0u, 0u);
    return true;

  case RGB_EFFECT_SOLID_COLOR: {
    const rgb_color_t color =
        rgb_static_scale_color(context->config->solid_color,
                               context->effective_brightness);
    rgb_set_all_color(color.r, color.g, color.b);
    return true;
  }

  case RGB_EFFECT_ALPHAS_MODS: {
    const rgb_color_t base =
        rgb_static_scale_color(context->config->solid_color,
                               context->effective_brightness);
    const rgb_color_t mod =
        rgb_static_scale_color(context->config->secondary_color,
                               context->effective_brightness);

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const rgb_color_t color = rgb_led_is_mod_at(i) ? mod : base;
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  case RGB_EFFECT_BAND_SAT:
    rgb_static_render_band_axis(context, true);
    return true;

  case RGB_EFFECT_BAND_VAL:
    rgb_static_render_band_axis(context, false);
    return true;

  case RGB_EFFECT_BAND_PINWHEEL_SAT:
  case RGB_EFFECT_BAND_PINWHEEL_VAL:
  case RGB_EFFECT_BAND_SPIRAL_SAT:
  case RGB_EFFECT_BAND_SPIRAL_VAL:
    rgb_static_render_band_polar(context, effect);
    return true;

  case RGB_EFFECT_GRADIENT_UP_DOWN:
    rgb_static_render_gradient(context, true);
    return true;

  case RGB_EFFECT_GRADIENT_LEFT_RIGHT:
    rgb_static_render_gradient(context, false);
    return true;

  case RGB_EFFECT_BREATHING: {
    const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
    const uint8_t pulse =
        (uint8_t)(abs8((int16_t)sin8(time >> 1) - 128) << 1);
    const uint8_t value = scale8(pulse, context->effective_brightness);
    const rgb_color_t color = hsv_to_rgb((hsv_t){
        .h = context->base_hue,
        .s = 255u,
        .v = value,
    });
    rgb_set_all_color(color.r, color.g, color.b);
    return true;
  }

  case RGB_EFFECT_CYCLE_ALL: {
    const rgb_color_t color = hsv_to_rgb((hsv_t){
        .h = (uint8_t)(context->scaled_timer >> 8),
        .s = 255u,
        .v = context->effective_brightness,
    });
    rgb_set_all_color(color.r, color.g, color.b);
    return true;
  }

  case RGB_EFFECT_CYCLE_LEFT_RIGHT:
    rgb_static_render_axis_cycle(context, false);
    return true;

  case RGB_EFFECT_CYCLE_UP_DOWN:
    rgb_static_render_axis_cycle(context, true);
    return true;

  case RGB_EFFECT_CYCLE_OUT_IN:
    rgb_static_render_cycle_out_in(context, false);
    return true;

  case RGB_EFFECT_CYCLE_OUT_IN_DUAL:
    rgb_static_render_cycle_out_in(context, true);
    return true;

  case RGB_EFFECT_RAINBOW_MOVING_CHEVRON: {
    const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const uint8_t hue = (uint8_t)(context->base_hue +
                                    abs8((int16_t)rgb_coord_y_at(i) - 127) +
                                    (rgb_coord_x_at(i) - time));
      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = hue,
          .s = 255u,
          .v = context->effective_brightness,
      });
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  case RGB_EFFECT_DUAL_BEACON: {
    const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
    const int16_t sn = (int16_t)sin8(time) - 128;
    const int16_t cs = (int16_t)cos8(time) - 128;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const int16_t dx = rgb_static_centered_x(i);
      const int16_t dy = rgb_static_centered_y(i);
      const int16_t proj = (dy * cs + dx * sn) / 128;
      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = (uint8_t)(context->base_hue + proj),
          .s = 255u,
          .v = context->effective_brightness,
      });
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  case RGB_EFFECT_CYCLE_PINWHEEL:
    rgb_static_render_cycle_polar(context, false);
    return true;

  case RGB_EFFECT_CYCLE_SPIRAL:
    rgb_static_render_cycle_polar(context, true);
    return true;

  case RGB_EFFECT_RAINBOW_BEACON: {
    const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
    const int16_t sn = (int16_t)sin8(time) - 128;
    const int16_t cs = (int16_t)cos8(time) - 128;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const int16_t dx = rgb_static_centered_x(i);
      const int16_t dy = rgb_static_centered_y(i);
      const int16_t delta = (dy * 2 * cs + dx * 2 * sn) / 128;
      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = (uint8_t)(time + delta),
          .s = 255u,
          .v = context->effective_brightness,
      });
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  case RGB_EFFECT_RAINBOW_PINWHEELS: {
    const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
    const int16_t sn = (int16_t)sin8(time) - 128;
    const int16_t cs = (int16_t)cos8(time) - 128;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const int16_t dx = rgb_static_centered_x(i);
      const int16_t dy = rgb_static_centered_y(i);
      const int16_t adx = dx < 0 ? (int16_t)(-dx) : dx;
      const int16_t delta = (dy * 3 * cs + (56 - adx) * 3 * sn) / 128;
      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = (uint8_t)(time + delta),
          .s = 255u,
          .v = context->effective_brightness,
      });
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  case RGB_EFFECT_FLOWER_BLOOMING: {
    const uint8_t phase = (uint8_t)((context->anim_timer / 4u) % 255u);

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const uint8_t dist = rgb_static_led_distance(i);
      const double wave =
          sin((((double)((int16_t)phase - (int16_t)dist)) * M_PI) / 64.0);
      uint16_t value = (uint16_t)(127.0 + 127.0 * wave);
      if (value > 255u) {
        value = 255u;
      }

      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = (uint8_t)(context->base_hue + dist),
          .s = 255u,
          .v = (uint8_t)(((uint32_t)value * context->effective_brightness) /
                         255u),
      });
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  case RGB_EFFECT_STARLIGHT:
    rgb_static_render_starlight_family(context, effect);
    return true;

  case RGB_EFFECT_RAINDROPS:
    if ((context->tick % 10u == 0u) && context->tick != context->prev_tick) {
      const uint8_t rand_idx = random8_max(NUM_LEDS);
      hsv_t hsv = {
          .h = context->base_hue,
          .s = 255u,
          .v = context->effective_brightness,
      };
      const int8_t delta_h = (int8_t)((int16_t)((uint16_t)hsv.h + 128u) -
                                      (int16_t)hsv.h) /
                             4;
      hsv.h = (uint8_t)(hsv.h + (uint8_t)(delta_h * random8_max(3u)));

      const rgb_color_t color = hsv_to_rgb(hsv);
      rgb_set_color(rand_idx, color.r, color.g, color.b);
    }
    return true;

  case RGB_EFFECT_JELLYBEAN_RAINDROPS:
    if ((context->tick % 5u == 0u) && context->tick != context->prev_tick) {
      const uint8_t rand_idx = random8_max(NUM_LEDS);
      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = random8(),
          .s = random8_min_max(127u, 255u),
          .v = context->effective_brightness,
      });
      rgb_set_color(rand_idx, color.r, color.g, color.b);
    }
    return true;

  case RGB_EFFECT_STARLIGHT_SMOOTH:
  case RGB_EFFECT_STARLIGHT_DUAL_HUE:
  case RGB_EFFECT_STARLIGHT_DUAL_SAT:
    rgb_static_render_starlight_family(context, effect);
    return true;

  case RGB_EFFECT_HUE_BREATHING: {
    const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
    const uint8_t delta = scale8(
        (uint8_t)(abs8((int16_t)sin8(time >> 1) - 128) << 1), 12u);
    const rgb_color_t color = hsv_to_rgb((hsv_t){
        .h = (uint8_t)(context->base_hue + delta),
        .s = 255u,
        .v = context->effective_brightness,
    });
    rgb_set_all_color(color.r, color.g, color.b);
    return true;
  }

  case RGB_EFFECT_HUE_PENDULUM: {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const uint8_t delta = scale8(
          (uint8_t)(abs8((int16_t)sin8((uint8_t)(context->scaled_timer >> 8)) +
                         rgb_coord_x_at(i) - 128) <<
                    1),
          12u);
      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = (uint8_t)(context->base_hue + delta),
          .s = 255u,
          .v = context->effective_brightness,
      });
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  case RGB_EFFECT_HUE_WAVE: {
    const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = (uint8_t)(context->base_hue +
                         scale8(abs8((int16_t)rgb_coord_x_at(i) - time), 24u)),
          .s = 255u,
          .v = context->effective_brightness,
      });
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  case RGB_EFFECT_RIVERFLOW: {
    const uint8_t time = (uint8_t)(context->scaled_timer >> 8);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      const uint8_t t = (uint8_t)(time + (i * 315u));
      const uint8_t value = scale8(
          (uint8_t)(abs8((int16_t)sin8(t) - 128) << 1),
          context->effective_brightness);
      const rgb_color_t color = hsv_to_rgb((hsv_t){
          .h = context->base_hue,
          .s = 255u,
          .v = value,
      });
      rgb_set_color(i, color.r, color.g, color.b);
    }
    return true;
  }

  default:
    return false;
  }
}

#endif
