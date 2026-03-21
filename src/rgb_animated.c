#include "rgb_animated.h"

#if defined(RGB_ENABLED)

#if defined(NUM_LEDS)
// Use custom if provided
#else
#define NUM_LEDS NUM_KEYS
#endif

#include "hardware/hardware.h"
#include "rgb_internal.h"
#include "rgb_math.h"

static uint32_t random_state = 0x12345678;
static uint8_t random8(void) {
    random_state = random_state * 1103515245 + 12345;
    return (uint8_t)(random_state >> 16);
}
static uint8_t random8_max(uint8_t max) {
    if (max == 0) return 0;
    return random8() % max;
}
static uint8_t random8_min_max(uint8_t min, uint8_t max) {
    if (min >= max) return min;
    return min + (random8() % (max - min));
}

static rgb_color_t pixel_flow_state[NUM_LEDS];
static uint8_t pixel_fractal_state[NUM_LEDS];
static uint8_t digital_rain_state[NUM_LEDS];
static uint8_t digital_rain_col_count = 0;
static uint8_t digital_rain_col_x[NUM_LEDS];
static uint8_t digital_rain_led_col[NUM_LEDS];
static uint8_t pixel_rain_index = 0;
static uint32_t pixel_flow_wait = 0;
static uint32_t pixel_fractal_wait = 0;
static uint32_t pixel_rain_wait = 0;
static uint8_t digital_rain_drop = 0;
static uint8_t digital_rain_decay = 0;

static uint8_t abs_diff_u8(uint8_t a, uint8_t b) {
    return (a > b) ? (a - b) : (b - a);
}

static void digital_rain_build_columns(void) {
    digital_rain_col_count = 0;
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        uint8_t x = rgb_coord_x_at(i);
        int8_t col = -1;
        for (uint8_t c = 0; c < digital_rain_col_count; c++) {
            if (abs_diff_u8(digital_rain_col_x[c], x) <= 12) {
                col = (int8_t)c;
                break;
            }
        }
        if (col < 0) {
            col = (int8_t)digital_rain_col_count;
            digital_rain_col_x[digital_rain_col_count++] = x;
        }
        digital_rain_led_col[i] = (uint8_t)col;
    }
}

static void rgb_render_pixel_flow(const rgb_animated_context_t *context) {
    uint8_t speed = scale16by8(qadd8(context->effect_speed, 16), 16);
    uint16_t interval = 3000 / (speed ? speed : 1);

    if (context->effect_changed) {
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            if (random8() & 2) {
                pixel_flow_state[i] = (rgb_color_t){0, 0, 0};
            } else {
                pixel_flow_state[i] = hsv_to_rgb((hsv_t){random8(),
                                                         random8_min_max(127, 255),
                                                         context->effective_brightness});
            }
        }
        pixel_flow_wait = timer_read();
    }

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        rgb_set_color(i, pixel_flow_state[i].r, pixel_flow_state[i].g,
                      pixel_flow_state[i].b);
    }

    if (timer_elapsed(pixel_flow_wait) < interval) return;

    for (uint8_t i = 0; i + 1 < NUM_LEDS; i++) {
        pixel_flow_state[i] = pixel_flow_state[i + 1];
    }
    pixel_flow_state[NUM_LEDS - 1] =
        (random8() & 2)
            ? (rgb_color_t){0, 0, 0}
            : hsv_to_rgb((hsv_t){random8(), random8_min_max(127, 255),
                                 context->effective_brightness});
    pixel_flow_wait = timer_read();
}

static void rgb_render_pixel_fractal(const rgb_animated_context_t *context) {
    uint8_t speed = scale16by8(qadd8(context->effect_speed, 16), 16);
    uint16_t interval = 3000 / (speed ? speed : 1);

    if (context->effect_changed) {
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            pixel_fractal_state[i] = 0;
        }
        pixel_fractal_wait = timer_read();
    }

    if (timer_elapsed(pixel_fractal_wait) >= interval) {
        uint8_t next[NUM_LEDS];
        memset(next, 0, sizeof(next));

        uint8_t half = NUM_LEDS / 2;
        for (uint8_t l = 0; l < half; l++) {
            uint8_t bit =
                (l + 1 < half) ? pixel_fractal_state[l + 1] : ((random8() & 3) == 0);
            next[l] = bit;
            next[NUM_LEDS - 1 - l] = bit;
        }
        if (NUM_LEDS & 1) next[half] = (random8() & 3) == 0;
        memcpy(pixel_fractal_state, next, sizeof(next));
        pixel_fractal_wait = timer_read();
    }

    rgb_color_t base =
        hsv_to_rgb((hsv_t){context->base_hue, 255, context->effective_brightness});
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        if (pixel_fractal_state[i]) {
            rgb_set_color(i, base.r, base.g, base.b);
        } else {
            rgb_set_color(i, 0, 0, 0);
        }
    }
}

static void rgb_render_pixel_rain(const rgb_animated_context_t *context) {
    if (context->effect_changed) {
        pixel_rain_index = random8_max(NUM_LEDS);
        pixel_rain_wait = timer_read();
    }

    uint32_t rain_interval =
        2048u - (uint32_t)scale16by8(1792u, context->effect_speed);
    if (timer_elapsed(pixel_rain_wait) < rain_interval) return;

    hsv_t hsv = (random8() & 2)
                    ? (hsv_t){0, 0, 0}
                    : (hsv_t){random8(), random8_min_max(127, 255),
                              context->effective_brightness};
    rgb_color_t color = hsv_to_rgb(hsv);
    rgb_set_color(pixel_rain_index, color.r, color.g, color.b);
    pixel_rain_index = random8_max(NUM_LEDS);
    pixel_rain_wait = timer_read();
}

static void rgb_render_digital_rain(const rgb_animated_context_t *context) {
    const uint8_t drop_ticks = 28;
    const uint8_t pure_green =
        (((uint16_t)context->effective_brightness) * 3) >> 2;
    const uint8_t max_boost =
        (((uint16_t)context->effective_brightness) * 3) >> 2;
    const uint8_t max_intensity = context->effective_brightness;
    const uint8_t decay_ticks =
        max_intensity ? (0xff / max_intensity) : 0xff;

    if (context->effect_changed) {
        digital_rain_build_columns();
        memset(digital_rain_state, 0, sizeof(digital_rain_state));
        digital_rain_drop = 0;
        digital_rain_decay = 0;
    }

    digital_rain_decay++;
    if (digital_rain_drop == 0) {
        for (uint8_t c = 0; c < digital_rain_col_count; c++) {
            if (random8_max(24) != 0) continue;

            uint8_t top = 0xff;
            uint8_t top_y = 0xff;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                if (digital_rain_led_col[i] != c) continue;
                uint8_t y = rgb_coord_y_at(i);
                if (y < top_y) {
                    top_y = y;
                    top = i;
                }
            }
            if (top != 0xff) digital_rain_state[top] = max_intensity;
        }
    }

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        if (digital_rain_state[i] > 0 && digital_rain_state[i] < max_intensity) {
            if (digital_rain_decay >= decay_ticks) digital_rain_state[i]--;
        }

        if (digital_rain_state[i] > pure_green) {
            uint8_t boost =
                (uint8_t)((uint16_t)max_boost *
                          (digital_rain_state[i] - pure_green) /
                          ((max_intensity > pure_green)
                               ? (max_intensity - pure_green)
                               : 1));
            rgb_set_color(i, boost, max_intensity, boost);
        } else {
            uint8_t green = (pure_green > 0)
                                ? (uint8_t)((uint16_t)max_intensity *
                                            digital_rain_state[i] / pure_green)
                                : 0;
            rgb_set_color(i, 0, green, 0);
        }
    }

    if (digital_rain_decay >= decay_ticks) digital_rain_decay = 0;
    if (++digital_rain_drop <= drop_ticks) return;

    digital_rain_drop = 0;
    for (uint8_t c = 0; c < digital_rain_col_count; c++) {
        uint8_t col_leds[NUM_LEDS];
        uint8_t count = 0;

        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            if (digital_rain_led_col[i] == c) col_leds[count++] = i;
        }

        for (uint8_t a = 0; a < count; a++) {
            for (uint8_t b = a + 1; b < count; b++) {
                if (rgb_coord_y_at(col_leds[a]) < rgb_coord_y_at(col_leds[b])) {
                    uint8_t temp = col_leds[a];
                    col_leds[a] = col_leds[b];
                    col_leds[b] = temp;
                }
            }
        }

        if (count > 0 && digital_rain_state[col_leds[0]] == max_intensity) {
            digital_rain_state[col_leds[0]]--;
        }
        for (uint8_t j = 0; j + 1 < count; j++) {
            uint8_t below = col_leds[j];
            uint8_t above = col_leds[j + 1];
            if (digital_rain_state[above] >= max_intensity) {
                digital_rain_state[above] = max_intensity - 1;
                digital_rain_state[below] = max_intensity;
            }
        }
    }
}

void rgb_animated_render(rgb_effect_t effect,
                         const rgb_animated_context_t *context) {
    switch (effect) {
        case RGB_EFFECT_PIXEL_FLOW:
            rgb_render_pixel_flow(context);
            break;
        case RGB_EFFECT_PIXEL_FRACTAL:
            rgb_render_pixel_fractal(context);
            break;
        case RGB_EFFECT_PIXEL_RAIN:
            rgb_render_pixel_rain(context);
            break;
        case RGB_EFFECT_DIGITAL_RAIN:
            rgb_render_digital_rain(context);
            break;
        default:
            break;
    }
}

#endif
