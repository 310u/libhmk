#include "rgb.h"

#if defined(RGB_ENABLED)

#include <math.h>

#if defined(NUM_LEDS)
// Use custom if provided
#else
#define NUM_LEDS NUM_KEYS
#endif

#include "hardware/hardware.h"
#include "hardware/rgb_api.h"
#include "matrix.h"
#include "layout.h"
#include "eeconfig.h"
#include "rgb_coords.h"

/*
 * Attribution:
 * Many effects in this file are adapted from QMK's RGB Matrix / RGB Light
 * effect set. libhmk keeps the QMK-aligned effect names where possible and
 * adds board-specific LED mapping/clipping around them.
 *
 * Local libhmk-only effects in this file are:
 *   - RGB_EFFECT_ANALOG
 *   - RGB_EFFECT_PER_KEY
 */

// Simple PRNG for effects
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
static uint8_t qadd8(uint8_t a, uint8_t b) {
    uint16_t res = (uint16_t)a + b;
    return (res > 255) ? 255 : (uint8_t)res;
}
static uint8_t qsub8(uint8_t a, uint8_t b) {
    return (a > b) ? (a - b) : 0;
}
static uint8_t scale8(uint8_t value, uint8_t scale) {
    return (uint8_t)(((uint16_t)value * (uint16_t)(scale + 1)) >> 8);
}
static uint16_t scale16by8(uint16_t value, uint8_t scale) {
    return (uint16_t)(((uint32_t)value * (uint32_t)(scale + 1)) >> 8);
}
static uint8_t abs8(int16_t v) {
    return (uint8_t)((v < 0) ? -v : v);
}
static uint8_t sin8(uint8_t theta) {
    float rad = (float)theta * (2.0f * (float)M_PI / 256.0f);
    int16_t s = (int16_t)(sinf(rad) * 127.0f) + 128;
    if (s < 0) s = 0;
    if (s > 255) s = 255;
    return (uint8_t)s;
}
static uint8_t cos8(uint8_t theta) {
    return sin8(theta + 64);
}

// We need an array to hold the current LED colors
static rgb_color_t current_colors[NUM_LEDS];
static rgb_config_t rgb_config;
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
        uint8_t x = rgb_led_coords[i].x;
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

// Heatmap state
static uint8_t rgb_heatmap[NUM_LEDS] = {0};
// QMK-like last-hit tracking for reactive/splash effects
#define RGB_LAST_HITS 10
typedef struct {
    uint8_t index;
    uint8_t x;
    uint8_t y;
    uint32_t time_ms;
} rgb_hit_t;
static rgb_hit_t rgb_last_hits[RGB_LAST_HITS] = {0};
static uint8_t rgb_last_hits_count = 0;

static uint8_t reactive_clip_scale(uint8_t source_led, uint8_t target_led, uint8_t value);
static uint8_t reactive_clip_effect(uint8_t target_led, const rgb_hit_t *hit, uint8_t effect);

void rgb_matrix_record_keypress(uint8_t index) {
    if (index < NUM_KEYS) {
        uint8_t led_index = rgb_key_to_led[index];
        if (led_index >= NUM_LEDS) {
            return;
        }
        rgb_hit_t hit = {
            .index = led_index,
            .x = rgb_led_coords[led_index].x,
            .y = rgb_led_coords[led_index].y,
            .time_ms = timer_read(),
        };
        if (rgb_last_hits_count < RGB_LAST_HITS) {
            rgb_last_hits[rgb_last_hits_count++] = hit;
        } else {
            for (uint8_t i = 1; i < RGB_LAST_HITS; i++) {
                rgb_last_hits[i - 1] = rgb_last_hits[i];
            }
            rgb_last_hits[RGB_LAST_HITS - 1] = hit;
        }

        // QMK-like heatmap increase step
        rgb_heatmap[led_index] = qadd8(rgb_heatmap[led_index], 32);
        
        // Slightly heat up neighbors based on coordinate proximity
        led_point_t p1 = rgb_led_coords[led_index];
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            if (i == led_index) continue;
            led_point_t p2 = rgb_led_coords[i];
            int dx = p1.x - p2.x;
            int dy = p1.y - p2.y;
            uint8_t distance = (uint8_t)sqrt(dx * dx + dy * dy);
            if (distance <= 40) {
                uint8_t amount = qsub8(40, distance);
                if (amount > 16) amount = 16;
                amount = reactive_clip_scale(led_index, i, amount);
                rgb_heatmap[i] = qadd8(rgb_heatmap[i], amount);
            }
        }
    }
}

void rgb_init(void) {
    rgb_driver_init();
    memcpy(&rgb_config, &CURRENT_PROFILE.rgb_config, sizeof(rgb_config_t));
    rgb_update();
}

rgb_config_t* rgb_get_config(void) {
    return &rgb_config;
}

void rgb_apply_config(void) {
    // Force a re-render based on new config from EEPROM/USB
    rgb_update();
}

void rgb_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < NUM_LEDS) {
        current_colors[index].r = r;
        current_colors[index].g = g;
        current_colors[index].b = b;
    }
}

void rgb_set_all_color(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        rgb_set_color(i, r, g, b);
    }
}

// Function to trigger the DMA transfer of the PWM data buffer
static void rgb_transmit_dma(void) {
    uint8_t grb_data[NUM_LEDS * 3];
    uint16_t offset = 0;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        grb_data[offset++] = current_colors[i].g;
        grb_data[offset++] = current_colors[i].r;
        grb_data[offset++] = current_colors[i].b;
    }

    rgb_driver_write(grb_data, offset);
    rgb_driver_task();
}

void rgb_update(void) {
    if (!rgb_config.enabled) {
        rgb_set_all_color(0, 0, 0);
        rgb_transmit_dma();
        return;
    }
    rgb_transmit_dma();
}

static uint8_t rgb_to_hue(rgb_color_t rgb) {
    uint8_t r = rgb.r, g = rgb.g, b = rgb.b;
    uint8_t max = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    uint8_t min = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    if (max == min) return 0;
    uint8_t d = max - min;
    int h;
    if (max == r) h = (g - b) * 42 / d;
    else if (max == g) h = (b - r) * 42 / d + 85;
    else h = (r - g) * 42 / d + 170;
    if (h < 0) h += 255;
    return (uint8_t)h;
}

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
    uint32_t elapsed = timer_elapsed(hit->time_ms);
    uint32_t t = (elapsed * ((uint32_t)qadd8(speed, 8))) / 16;
    return (t > 255) ? 255 : (uint8_t)t;
}

static uint8_t reactive_clip_scale(uint8_t source_led, uint8_t target_led, uint8_t value) {
    return scale8(value, rgb_reactive_clip[source_led][target_led]);
}

static uint8_t reactive_clip_effect(uint8_t target_led, const rgb_hit_t *hit, uint8_t effect) {
    uint8_t visible = (uint8_t)(255u - effect);
    visible = reactive_clip_scale(hit->index, target_led, visible);
    return (uint8_t)(255u - visible);
}

static uint8_t reactive_strength_for_hit(uint8_t led, const rgb_hit_t *hit, reactive_mode_t mode, uint8_t speed) {
    uint8_t tick = hit_elapsed_tick(hit, speed);
    int16_t dx = (int16_t)rgb_led_coords[led].x - (int16_t)hit->x;
    int16_t dy = (int16_t)rgb_led_coords[led].y - (int16_t)hit->y;
    uint8_t dist = (uint8_t)sqrt(dx * dx + dy * dy);
    int16_t effect = tick;

    switch (mode) {
        case REACTIVE_MODE_SIMPLE:
            effect = tick;
            break;
        case REACTIVE_MODE_WIDE:
            effect = tick + dist * 5;
            break;
        case REACTIVE_MODE_CROSS: {
            uint16_t ax = (dx < 0) ? -dx : dx;
            uint16_t ay = (dy < 0) ? -dy : dy;
            ax = (ax * 16 > 255) ? 255 : ax * 16;
            ay = (ay * 16 > 255) ? 255 : ay * 16;
            effect = tick + ((ax > ay) ? ay : ax);
            break;
        }
        case REACTIVE_MODE_NEXUS:
            effect = tick - dist;
            if (effect < 0) effect = 255;
            if (dist > 72) effect = 255;
            if ((dx > 8 || dx < -8) && (dy > 8 || dy < -8)) effect = 255;
            break;
        default:
            effect = tick;
            break;
    }
    if (effect > 255) effect = 255;
    return reactive_clip_effect(led, hit, (uint8_t)effect);
}

static uint8_t splash_strength_for_hit(uint8_t led, const rgb_hit_t *hit, uint8_t speed) {
    uint8_t tick = hit_elapsed_tick(hit, speed);
    int16_t dx = (int16_t)rgb_led_coords[led].x - (int16_t)hit->x;
    int16_t dy = (int16_t)rgb_led_coords[led].y - (int16_t)hit->y;
    uint8_t dist = (uint8_t)sqrt(dx * dx + dy * dy);
    int16_t effect = (int16_t)tick - dist;
    if (effect < 0) effect = 255;
    if (effect > 255) effect = 255;
    return reactive_clip_effect(led, hit, (uint8_t)effect);
}

static uint8_t compute_reactive_intensity(uint8_t led, uint8_t effect, uint8_t speed) {
    reactive_mode_t mode = reactive_mode_from_effect(effect);
    bool multi = reactive_is_multi(effect);
    if (rgb_last_hits_count == 0) return 0;

    if (!multi) {
        return reactive_strength_for_hit(led, &rgb_last_hits[rgb_last_hits_count - 1], mode, speed);
    }

    uint8_t best = 255;
    for (uint8_t i = 0; i < rgb_last_hits_count; i++) {
        uint8_t v = reactive_strength_for_hit(led, &rgb_last_hits[i], mode, speed);
        if (v < best) best = v;
    }
    return best;
}

static uint8_t compute_splash_intensity(uint8_t led, uint8_t effect, uint8_t speed) {
    bool multi = splash_is_multi(effect);
    if (rgb_last_hits_count == 0) return 0;

    if (!multi) {
        return splash_strength_for_hit(led, &rgb_last_hits[rgb_last_hits_count - 1], speed);
    }

    uint8_t best = 255;
    for (uint8_t i = 0; i < rgb_last_hits_count; i++) {
        uint8_t v = splash_strength_for_hit(led, &rgb_last_hits[i], speed);
        if (v < best) best = v;
    }
    return best;
}

rgb_color_t hsv_to_rgb(hsv_t hsv) {
    rgb_color_t rgb = {0, 0, 0};
    if (hsv.s == 0) {
        rgb.r = rgb.g = rgb.b = hsv.v;
        return rgb;
    }

    uint8_t region = hsv.h / 43;
    uint8_t remainder = (hsv.h - (region * 43)) * 6; 

    uint8_t p = (hsv.v * (255 - hsv.s)) >> 8;
    uint8_t q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    uint8_t t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:  rgb.r = hsv.v; rgb.g = t; rgb.b = p; break;
        case 1:  rgb.r = q; rgb.g = hsv.v; rgb.b = p; break;
        case 2:  rgb.r = p; rgb.g = hsv.v; rgb.b = t; break;
        case 3:  rgb.r = p; rgb.g = q; rgb.b = hsv.v; break;
        case 4:  rgb.r = t; rgb.g = p; rgb.b = hsv.v; break;
        default: rgb.r = hsv.v; rgb.g = p; rgb.b = q; break;
    }
    return rgb;
}

void rgb_task(void) {
    rgb_driver_task();

    if (!rgb_config.enabled) return;

    static uint32_t last_render_tick = 0;
    static uint32_t heatmap_tick = 0;
    uint32_t current_tick = timer_read();
    
    // Heatmap decay process (every 25ms)
    if (timer_elapsed(heatmap_tick) >= 25) {
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            if (rgb_heatmap[i] > 0) rgb_heatmap[i] = qsub8(rgb_heatmap[i], 1);
        }
        heatmap_tick = current_tick;
    }

    // Limit render framerate to ~60fps (16ms)
    if (timer_elapsed(last_render_tick) < 16) return;
    last_render_tick = current_tick;

    uint8_t effective_brightness = rgb_config.global_brightness;
    uint32_t idle_time = matrix_get_idle_time();
    uint32_t timeout_ms = (uint32_t)rgb_config.sleep_timeout * 60000u;
    
    static bool was_asleep = false;
    if (timeout_ms > 0 && idle_time > timeout_ms) {
        uint32_t fade_duration = 2000; // 2 seconds to fade out
        if (idle_time >= timeout_ms + fade_duration) {
            effective_brightness = 0;
        } else {
            uint32_t passed = idle_time - timeout_ms;
            effective_brightness = (effective_brightness * (fade_duration - passed)) / fade_duration;
        }
    }

    if (effective_brightness == 0) {
        if (!was_asleep) {
            rgb_set_all_color(0, 0, 0);
            rgb_update();
            was_asleep = true;
        }
        return;
    }
    was_asleep = false;

    // A generic rolling timer based on system ticks and effect_speed
    static uint32_t anim_timer = 0;
    static uint16_t scaled_timer = 0;
    
    anim_timer += ((uint32_t)rgb_config.effect_speed * 16u) / 128u;
    
    uint16_t prev_scaled = scaled_timer;
    scaled_timer += qadd8(rgb_config.effect_speed, 16);
    
    // Values for periodic triggers (converted to 8-bit shifts for simplicity)
    uint16_t tick = scaled_timer >> 8;
    uint16_t prev_tick = prev_scaled >> 8;
    
    uint8_t base_hue = rgb_to_hue(rgb_config.solid_color);
    static uint8_t prev_effect = 0xff;
    bool effect_changed = (prev_effect != rgb_config.current_effect);
    prev_effect = rgb_config.current_effect;

    switch (rgb_config.current_effect) {
        case RGB_EFFECT_OFF:
            rgb_set_all_color(0, 0, 0);
            break;
        case RGB_EFFECT_SOLID_COLOR: {
            uint32_t r = ((uint32_t)rgb_config.solid_color.r * effective_brightness) / 255u;
            uint32_t g = ((uint32_t)rgb_config.solid_color.g * effective_brightness) / 255u;
            uint32_t b = ((uint32_t)rgb_config.solid_color.b * effective_brightness) / 255u;
            rgb_set_all_color(r, g, b);
            break;
        }
        case RGB_EFFECT_ALPHAS_MODS: {
            uint8_t base_r = ((uint32_t)rgb_config.solid_color.r * effective_brightness) / 255;
            uint8_t base_g = ((uint32_t)rgb_config.solid_color.g * effective_brightness) / 255;
            uint8_t base_b = ((uint32_t)rgb_config.solid_color.b * effective_brightness) / 255;
            uint8_t mod_r = ((uint32_t)rgb_config.secondary_color.r * effective_brightness) / 255;
            uint8_t mod_g = ((uint32_t)rgb_config.secondary_color.g * effective_brightness) / 255;
            uint8_t mod_b = ((uint32_t)rgb_config.secondary_color.b * effective_brightness) / 255;

            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                bool is_mod = rgb_led_is_mod[i] != 0;
                rgb_set_color(i, is_mod ? mod_r : base_r, is_mod ? mod_g : base_g,
                              is_mod ? mod_b : base_b);
            }
            break;
        }

        case RGB_EFFECT_BAND_SAT:
        case RGB_EFFECT_BAND_VAL: {
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                // QMK: s = hsv.s - abs(scale8(g_led_config.point[i].x, 228) + 28 - time) * 8;
                // We'll use a simplified version that matches the visual band behavior
                int16_t dist = (int16_t)x + 28 - (scaled_timer >> 8);
                if (dist < 0) dist = -dist;
                int16_t val = 255 - dist * 8;
                if (val < 0) val = 0;
                
                if (rgb_config.current_effect == RGB_EFFECT_BAND_SAT) {
                    hsv_t hsv = { .h = base_hue, .s = (uint8_t)val, .v = effective_brightness };
                    rgb_set_color(i, hsv_to_rgb(hsv).r, hsv_to_rgb(hsv).g, hsv_to_rgb(hsv).b);
                } else {
                    hsv_t hsv = { .h = base_hue, .s = 255, .v = (uint8_t)((uint32_t)val * effective_brightness / 255) };
                    rgb_set_color(i, hsv_to_rgb(hsv).r, hsv_to_rgb(hsv).g, hsv_to_rgb(hsv).b);
                }
            }
            break;
        }

        case RGB_EFFECT_BAND_PINWHEEL_SAT:
        case RGB_EFFECT_BAND_PINWHEEL_VAL:
        case RGB_EFFECT_BAND_SPIRAL_SAT:
        case RGB_EFFECT_BAND_SPIRAL_VAL: {
            uint8_t time = scaled_timer >> 8;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                int16_t dx = (int16_t)rgb_led_coords[i].x - 127;
                int16_t dy = (int16_t)rgb_led_coords[i].y - 127;
                uint8_t dist = (uint8_t)sqrt(dx*dx + dy*dy);
                uint8_t angle = (uint8_t)((atan2(dy, dx) + M_PI) * 255 / (2 * M_PI));
                
                uint8_t offset = angle;
                if (rgb_config.current_effect == RGB_EFFECT_BAND_SPIRAL_SAT || rgb_config.current_effect == RGB_EFFECT_BAND_SPIRAL_VAL) {
                    offset += dist;
                }
                
                int16_t val = 255 - abs((int16_t)offset - time) * 8;
                if (val < 0) val = 0;

                if (rgb_config.current_effect == RGB_EFFECT_BAND_PINWHEEL_SAT || rgb_config.current_effect == RGB_EFFECT_BAND_SPIRAL_SAT) {
                    hsv_t hsv = { .h = base_hue, .s = (uint8_t)val, .v = effective_brightness };
                    rgb_set_color(i, hsv_to_rgb(hsv).r, hsv_to_rgb(hsv).g, hsv_to_rgb(hsv).b);
                } else {
                    hsv_t hsv = { .h = base_hue, .s = 255, .v = (uint8_t)((uint32_t)val * effective_brightness / 255) };
                    rgb_set_color(i, hsv_to_rgb(hsv).r, hsv_to_rgb(hsv).g, hsv_to_rgb(hsv).b);
                }
            }
            break;
        }

        case RGB_EFFECT_PIXEL_FLOW: {
            uint16_t interval = 3000 / (scale16by8(qadd8(rgb_config.effect_speed, 16), 16) ? scale16by8(qadd8(rgb_config.effect_speed, 16), 16) : 1);
            if (effect_changed) {
                for (uint8_t i = 0; i < NUM_LEDS; i++) {
                    if (random8() & 2) {
                        pixel_flow_state[i] = (rgb_color_t){0, 0, 0};
                    } else {
                        pixel_flow_state[i] = hsv_to_rgb((hsv_t){random8(), random8_min_max(127, 255), effective_brightness});
                    }
                }
                pixel_flow_wait = timer_read();
            }
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                rgb_set_color(i, pixel_flow_state[i].r, pixel_flow_state[i].g, pixel_flow_state[i].b);
            }
            if (timer_elapsed(pixel_flow_wait) >= interval) {
                for (uint8_t i = 0; i + 1 < NUM_LEDS; i++) pixel_flow_state[i] = pixel_flow_state[i + 1];
                pixel_flow_state[NUM_LEDS - 1] = (random8() & 2)
                    ? (rgb_color_t){0, 0, 0}
                    : hsv_to_rgb((hsv_t){random8(), random8_min_max(127, 255), effective_brightness});
                pixel_flow_wait = timer_read();
            }
            break;
        }

        case RGB_EFFECT_PIXEL_FRACTAL: {
            uint16_t interval = 3000 / (scale16by8(qadd8(rgb_config.effect_speed, 16), 16) ? scale16by8(qadd8(rgb_config.effect_speed, 16), 16) : 1);
            if (effect_changed) {
                for (uint8_t i = 0; i < NUM_LEDS; i++) pixel_fractal_state[i] = 0;
                pixel_fractal_wait = timer_read();
            }
            if (timer_elapsed(pixel_fractal_wait) >= interval) {
                uint8_t next[NUM_LEDS];
                for (uint8_t i = 0; i < NUM_LEDS; i++) next[i] = 0;
                uint8_t half = NUM_LEDS / 2;
                for (uint8_t l = 0; l < half; l++) {
                    uint8_t bit = (l + 1 < half) ? pixel_fractal_state[l + 1] : ((random8() & 3) == 0);
                    next[l] = bit;
                    next[NUM_LEDS - 1 - l] = bit;
                }
                if (NUM_LEDS & 1) next[half] = (random8() & 3) == 0;
                for (uint8_t i = 0; i < NUM_LEDS; i++) pixel_fractal_state[i] = next[i];
                pixel_fractal_wait = timer_read();
            }
            rgb_color_t base = hsv_to_rgb((hsv_t){base_hue, 255, effective_brightness});
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                if (pixel_fractal_state[i]) rgb_set_color(i, base.r, base.g, base.b);
                else rgb_set_color(i, 0, 0, 0);
            }
            break;
        }

        case RGB_EFFECT_PIXEL_RAIN: {
            if (effect_changed) {
                pixel_rain_index = random8_max(NUM_LEDS);
                pixel_rain_wait = timer_read();
            }
            uint32_t rain_interval = 2048u - (uint32_t)scale16by8(1792u, rgb_config.effect_speed);
            if (timer_elapsed(pixel_rain_wait) >= rain_interval) {
                hsv_t hsv = (random8() & 2) ? (hsv_t){0, 0, 0} : (hsv_t){random8(), random8_min_max(127, 255), effective_brightness};
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(pixel_rain_index, c.r, c.g, c.b);
                pixel_rain_index = random8_max(NUM_LEDS);
                pixel_rain_wait = timer_read();
            }
            break;
        }

        case RGB_EFFECT_GRADIENT_UP_DOWN: {
            uint8_t scale = scale8(64, rgb_config.effect_speed);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t y = rgb_led_coords[i].y;
                uint8_t h = (uint8_t)(base_hue + scale * (y >> 4));
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_GRADIENT_LEFT_RIGHT: {
            uint8_t scale = scale8(64, rgb_config.effect_speed);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t h = (uint8_t)(base_hue + ((scale * x) >> 5));
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }


        case RGB_EFFECT_BREATHING: {
            // QMK-style breathing: scale8(abs8(sin8(time / 2) - 128) * 2, val)
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            uint8_t s = sin8(time >> 1);
            uint8_t pulse = (uint8_t)(abs8((int16_t)s - 128) << 1);
            uint8_t v = scale8(pulse, effective_brightness);
            
            rgb_color_t c = hsv_to_rgb((hsv_t){.h = base_hue, .s = 255, .v = v});
            rgb_set_all_color(c.r, c.g, c.b);
            break;
        }

        case RGB_EFFECT_CYCLE_ALL: {
            uint8_t h = (uint8_t)(scaled_timer >> 8);
            rgb_color_t c = hsv_to_rgb((hsv_t){.h = h, .s = 255, .v = effective_brightness});
            rgb_set_all_color(c.r, c.g, c.b);
            break;
        }

        case RGB_EFFECT_CYCLE_LEFT_RIGHT: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t h = (uint8_t)(x - time);
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_CYCLE_UP_DOWN: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t y = rgb_led_coords[i].y;
                uint8_t h = (uint8_t)(y - time);
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_CYCLE_OUT_IN: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t y = rgb_led_coords[i].y;
                int16_t dx = (int16_t)x - 127;
                int16_t dy = (int16_t)y - 127;
                uint8_t dist = (uint8_t)sqrt(dx*dx + dy*dy);
                // QMK: h = 3 * dist / 2 + time
                uint8_t h = (uint8_t)((3 * dist / 2) + time);
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }
        case RGB_EFFECT_CYCLE_OUT_IN_DUAL: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            uint8_t secondary_hue = rgb_to_hue(rgb_config.secondary_color);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t y = rgb_led_coords[i].y;
                int16_t dx = (int16_t)x - 127;
                int16_t dy = (int16_t)y - 127;
                int16_t centered = (127 / 2) - abs8(dx);
                uint8_t dist = (uint8_t)sqrt(centered * centered + dy * dy);
                uint8_t h = (uint8_t)(3 * dist + time);
                // Alternate between primary/secondary hue rings.
                uint8_t final_h = (h & 0x80) ? secondary_hue : h;
                hsv_t hsv = { .h = final_h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_RAINBOW_MOVING_CHEVRON: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t y = rgb_led_coords[i].y;
                uint8_t h = (uint8_t)(base_hue + abs8((int16_t)y - 127) + (x - time));
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }
        case RGB_EFFECT_DUAL_BEACON: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            uint8_t sn = sin8(time);
            uint8_t cs = cos8(time);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                int16_t dx = (int16_t)rgb_led_coords[i].x - 127;
                int16_t dy = (int16_t)rgb_led_coords[i].y - 127;
                int16_t proj = (dy * ((int16_t)cs - 128) + dx * ((int16_t)sn - 128)) / 128;
                uint8_t h = (uint8_t)(base_hue + proj);
                rgb_color_t c = hsv_to_rgb((hsv_t){.h = h, .s = 255, .v = effective_brightness});
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_CYCLE_PINWHEEL:
        case RGB_EFFECT_CYCLE_SPIRAL: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t y = rgb_led_coords[i].y;
                int16_t dx = (int16_t)x - 127;
                int16_t dy = (int16_t)y - 127;
                uint8_t angle = (uint8_t)((atan2(dy, dx) + M_PI) * 255 / (2 * M_PI));
                uint8_t dist = (uint8_t)sqrt(dx * dx + dy * dy);
                uint8_t h = (rgb_config.current_effect == RGB_EFFECT_CYCLE_SPIRAL)
                                ? (uint8_t)(dist - time - angle)
                                : (uint8_t)(angle + time);
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_RAINBOW_BEACON: {
            // QMK: hsv.h += ((y - center_y) * 2 * cos + (x - center_x) * 2 * sin) / 128
            uint8_t time = scaled_timer >> 8;
            int16_t sn = (int16_t)sin8(time) - 128;
            int16_t cs = (int16_t)cos8(time) - 128;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                int16_t dx = (int16_t)rgb_led_coords[i].x - 127;
                int16_t dy = (int16_t)rgb_led_coords[i].y - 127;
                // Hue shifts based on time + spatial beacon rotation
                int16_t delta = (dy * 2 * cs + dx * 2 * sn) / 128;
                uint8_t h = (uint8_t)(time + delta);
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_RAINBOW_PINWHEELS: {
            // QMK: hsv.h += ((y - center_y) * 3 * cos + (56 - abs(x - center_x)) * 3 * sin) / 128
            uint8_t time = scaled_timer >> 8;
            int16_t sn = (int16_t)sin8(time) - 128;
            int16_t cs = (int16_t)cos8(time) - 128;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                int16_t dx = (int16_t)rgb_led_coords[i].x - 127;
                int16_t dy = (int16_t)rgb_led_coords[i].y - 127;
                int16_t adx = (dx < 0) ? -dx : dx;
                // Add time to hue for full color cycling
                int16_t delta = (dy * 3 * cs + (56 - adx) * 3 * sn) / 128;
                uint8_t h = (uint8_t)(time + delta);
                hsv_t hsv = { .h = h, .s = 255, .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_FLOWER_BLOOMING: {
            uint8_t phase = (anim_timer / 4) % 255;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t y = rgb_led_coords[i].y;
                int16_t dx = (int16_t)x - 127;
                int16_t dy = (int16_t)y - 127;
                uint8_t dist = (uint8_t)sqrt(dx*dx + dy*dy);
                uint8_t v = (uint8_t)(127 + 127 * sin((phase - dist) * M_PI / 64));
                hsv_t hsv = { .h = (uint8_t)(base_hue + dist), .s = 255, .v = (uint8_t)((uint32_t)v * effective_brightness / 255) };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_STARLIGHT: {
            uint8_t st_time = scale8((uint8_t)(scaled_timer >> 8), (uint8_t)(rgb_config.effect_speed >> 3));
            uint8_t st_v = scale8((uint8_t)(abs8((int16_t)sin8(st_time) - 128) << 1), effective_brightness);
            if ((tick % 5 == 0) && (tick != prev_tick)) {
                uint8_t rand_idx = random8_max(NUM_LEDS);
                hsv_t hsv = { .h = base_hue, .s = 255, .v = st_v };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(rand_idx, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_RAINDROPS: {
            // Periodic trigger
            if ((tick % 10 == 0) && (tick != prev_tick)) {
                uint8_t rand_idx = random8_max(NUM_LEDS);
                hsv_t hsv = { .h = base_hue, .s = 255, .v = effective_brightness };
                
                int8_t delta_h = (int8_t)((hsv.h + 128) - hsv.h) / 4;
                hsv.h += (uint8_t)(delta_h * random8_max(3));
                
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(rand_idx, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_JELLYBEAN_RAINDROPS: {
            if ((tick % 5 == 0) && (tick != prev_tick)) {
                uint8_t rand_idx = random8_max(NUM_LEDS);
                hsv_t hsv = { .h = random8(), .s = random8_min_max(127, 255), .v = effective_brightness };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(rand_idx, c.r, c.g, c.b);
            }
            break;
        }
        case RGB_EFFECT_STARLIGHT_SMOOTH:
        case RGB_EFFECT_STARLIGHT_DUAL_HUE:
        case RGB_EFFECT_STARLIGHT_DUAL_SAT: {
            uint8_t st_time = scale8((uint8_t)(scaled_timer >> 8), (uint8_t)(rgb_config.effect_speed >> 3));
            uint8_t st_v = scale8((uint8_t)(abs8((int16_t)sin8(st_time) - 128) << 1), effective_brightness);
            if ((tick % 5 == 0) && (tick != prev_tick)) {
                uint8_t rand_idx = random8_max(NUM_LEDS);
                hsv_t hsv = { .h = base_hue, .s = 255, .v = st_v };
                if (rgb_config.current_effect == RGB_EFFECT_STARLIGHT_DUAL_HUE) {
                    hsv.h = (uint8_t)(hsv.h + random8_max(31));
                } else if (rgb_config.current_effect == RGB_EFFECT_STARLIGHT_DUAL_SAT) {
                    hsv.s = (uint8_t)(hsv.s + random8_max(31));
                }
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(rand_idx, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_TYPING_HEATMAP: {
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t temp = rgb_heatmap[i];
                if (temp == 0) {
                    rgb_set_color(i, 0, 0, 0);
                    continue;
                }
                uint8_t sub = qsub8(temp, 85);
                uint8_t hue = (170 > sub) ? (170 - sub) : 0;
                uint8_t heat = qsub8(qadd8(170, temp), 170);
                uint8_t v = scale8((uint8_t)(heat * 3), effective_brightness);
                hsv_t hsv = { .h = hue, .s = 255, .v = v };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_DIGITAL_RAIN: {
            const uint8_t drop_ticks = 28;
            const uint8_t pure_green = (((uint16_t)effective_brightness) * 3) >> 2;
            const uint8_t max_boost = (((uint16_t)effective_brightness) * 3) >> 2;
            const uint8_t max_intensity = effective_brightness;
            const uint8_t decay_ticks = max_intensity ? (0xff / max_intensity) : 0xff;
            if (effect_changed) {
                digital_rain_build_columns();
                for (uint8_t i = 0; i < NUM_LEDS; i++) digital_rain_state[i] = 0;
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
                        uint8_t y = rgb_led_coords[i].y;
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
                    uint8_t boost = (uint8_t)((uint16_t)max_boost * (digital_rain_state[i] - pure_green) / ((max_intensity > pure_green) ? (max_intensity - pure_green) : 1));
                    rgb_set_color(i, boost, max_intensity, boost);
                } else {
                    uint8_t green = (pure_green > 0) ? (uint8_t)((uint16_t)max_intensity * digital_rain_state[i] / pure_green) : 0;
                    rgb_set_color(i, 0, green, 0);
                }
            }
            if (digital_rain_decay >= decay_ticks) digital_rain_decay = 0;
            if (++digital_rain_drop > drop_ticks) {
                digital_rain_drop = 0;
                for (uint8_t c = 0; c < digital_rain_col_count; c++) {
                    uint8_t col_leds[NUM_LEDS];
                    uint8_t count = 0;
                    for (uint8_t i = 0; i < NUM_LEDS; i++) {
                        if (digital_rain_led_col[i] == c) col_leds[count++] = i;
                    }
                    // Sort by y descending (bottom -> top)
                    for (uint8_t a = 0; a < count; a++) {
                        for (uint8_t b = a + 1; b < count; b++) {
                            if (rgb_led_coords[col_leds[a]].y < rgb_led_coords[col_leds[b]].y) {
                                uint8_t t = col_leds[a];
                                col_leds[a] = col_leds[b];
                                col_leds[b] = t;
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
            break;
        }

        case RGB_EFFECT_ANALOG: {
            uint8_t base_r = (uint8_t)(((uint32_t)rgb_config.secondary_color.r * effective_brightness) / 255u);
            uint8_t base_g = (uint8_t)(((uint32_t)rgb_config.secondary_color.g * effective_brightness) / 255u);
            uint8_t base_b = (uint8_t)(((uint32_t)rgb_config.secondary_color.b * effective_brightness) / 255u);
            uint8_t pressed_r = (uint8_t)(((uint32_t)rgb_config.solid_color.r * effective_brightness) / 255u);
            uint8_t pressed_g = (uint8_t)(((uint32_t)rgb_config.solid_color.g * effective_brightness) / 255u);
            uint8_t pressed_b = (uint8_t)(((uint32_t)rgb_config.solid_color.b * effective_brightness) / 255u);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t key_index = rgb_led_key_index[i];
                uint8_t dist = (key_index < NUM_KEYS) ? key_matrix[key_index].distance : 0;
                uint8_t final_r = (uint8_t)(((uint32_t)pressed_r * dist + (uint32_t)base_r * (uint32_t)(255u - dist)) / 255u);
                uint8_t final_g = (uint8_t)(((uint32_t)pressed_g * dist + (uint32_t)base_g * (uint32_t)(255u - dist)) / 255u);
                uint8_t final_b = (uint8_t)(((uint32_t)pressed_b * dist + (uint32_t)base_b * (uint32_t)(255u - dist)) / 255u);
                rgb_set_color(i, final_r, final_g, final_b);
            }
            break;
        }
        case RGB_EFFECT_HUE_BREATHING: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            uint8_t delta = 12;
            uint8_t h = (uint8_t)(base_hue + scale8((uint8_t)(abs8((int16_t)sin8(time >> 1) - 128) << 1), delta));
            rgb_color_t c = hsv_to_rgb((hsv_t){.h = h, .s = 255, .v = effective_brightness});
            rgb_set_all_color(c.r, c.g, c.b);
            break;
        }
        case RGB_EFFECT_HUE_PENDULUM: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            uint8_t huedelta = 12;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t delta = scale8((uint8_t)(abs8((int16_t)sin8(time) + x - 128) << 1), huedelta);
                uint8_t h = (uint8_t)(base_hue + delta);
                rgb_color_t c = hsv_to_rgb((hsv_t){.h = h, .s = 255, .v = effective_brightness});
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }
        case RGB_EFFECT_HUE_WAVE: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            uint8_t huedelta = 24;
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                uint8_t h = (uint8_t)(base_hue + scale8(abs8((int16_t)x - time), huedelta));
                rgb_color_t c = hsv_to_rgb((hsv_t){.h = h, .s = 255, .v = effective_brightness});
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }
        case RGB_EFFECT_RIVERFLOW: {
            uint8_t time = (uint8_t)(scaled_timer >> 8);
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t t = (uint8_t)(time + (i * 315));
                uint8_t v = scale8((uint8_t)(abs8((int16_t)sin8(t) - 128) << 1), effective_brightness);
                rgb_color_t c = hsv_to_rgb((hsv_t){.h = base_hue, .s = 255, .v = v});
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_SOLID_REACTIVE:
        case RGB_EFFECT_SOLID_REACTIVE_SIMPLE:
        case RGB_EFFECT_SOLID_REACTIVE_WIDE:
        case RGB_EFFECT_SOLID_REACTIVE_MULTIWIDE:
        case RGB_EFFECT_SOLID_REACTIVE_CROSS:
        case RGB_EFFECT_SOLID_REACTIVE_MULTICROSS:
        case RGB_EFFECT_SOLID_REACTIVE_NEXUS:
        case RGB_EFFECT_SOLID_REACTIVE_MULTINEXUS: {
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t effect = compute_reactive_intensity(i, rgb_config.current_effect, rgb_config.effect_speed);
                hsv_t hsv = { .h = base_hue, .s = 255, .v = effective_brightness };
                if (rgb_config.current_effect == RGB_EFFECT_SOLID_REACTIVE_SIMPLE) {
                    hsv.v = scale8((uint8_t)(255 - effect), hsv.v);
                } else if (rgb_config.current_effect == RGB_EFFECT_SOLID_REACTIVE) {
                    hsv.h = (uint8_t)(base_hue + scale8((uint8_t)(255 - effect), 64));
                } else {
                    if (rgb_config.current_effect == RGB_EFFECT_SOLID_REACTIVE_NEXUS ||
                        rgb_config.current_effect == RGB_EFFECT_SOLID_REACTIVE_MULTINEXUS) {
                        int16_t dy = (int16_t)rgb_led_coords[i].y - 127;
                        hsv.h = (uint8_t)(base_hue + (dy / 4));
                    }
                    hsv.v = qadd8(hsv.v, (uint8_t)(255 - effect));
                }
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }
        case RGB_EFFECT_SPLASH:
        case RGB_EFFECT_MULTISPLASH:
        case RGB_EFFECT_SOLID_SPLASH:
        case RGB_EFFECT_SOLID_MULTISPLASH: {
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t effect = compute_splash_intensity(i, rgb_config.current_effect, rgb_config.effect_speed);
                hsv_t hsv = { .h = base_hue, .s = 255, .v = effective_brightness };
                if (rgb_config.current_effect == RGB_EFFECT_SPLASH || rgb_config.current_effect == RGB_EFFECT_MULTISPLASH) {
                    hsv.h = (uint8_t)(hsv.h + effect);
                }
                hsv.v = qadd8(hsv.v, (uint8_t)(255 - effect));
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }
        case RGB_EFFECT_PER_KEY: {
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                rgb_color_t color = rgb_config.per_key_colors[i];
                uint8_t r = ((uint32_t)color.r * effective_brightness) / 255;
                uint8_t g = ((uint32_t)color.g * effective_brightness) / 255;
                uint8_t b = ((uint32_t)color.b * effective_brightness) / 255;
                rgb_set_color(i, r, g, b);
            }
            break;
        }

        default:
            // Rainbow wave as default for anything else
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                hsv_t hsv = {
                    .h = (uint8_t)((anim_timer / 16) + x),
                    .s = 255,
                    .v = effective_brightness
                };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
    }

    // Layer Indicator Override
    static uint8_t previous_layer = 0;
    static uint32_t layer_switch_time = 0;
    uint8_t current_layer = layout_get_current_layer();
    
    if (current_layer != previous_layer) {
        layer_switch_time = timer_read();
        previous_layer = current_layer;
    }

    if (current_layer > 0 && current_layer < NUM_LAYERS) {
        rgb_color_t layer_color = rgb_config.layer_colors[current_layer];
        if (layer_color.r > 0 || layer_color.g > 0 || layer_color.b > 0) {
            uint8_t r = ((uint32_t)layer_color.r * effective_brightness) / 255;
            uint8_t g = ((uint32_t)layer_color.g * effective_brightness) / 255;
            uint8_t b = ((uint32_t)layer_color.b * effective_brightness) / 255;

            if (rgb_config.layer_indicator_mode == 0) {
                // Mode 0: Fill entire keyboard
                rgb_set_all_color(r, g, b);
            } else if (rgb_config.layer_indicator_mode == 1) {
                // Mode 1: Flash entire keyboard for 500ms
                if (timer_elapsed(layer_switch_time) < 500) {
                    rgb_set_all_color(r, g, b);
                }
            } else if (rgb_config.layer_indicator_mode == 2) {
                // Mode 2: Illuminate a specific key
                if (rgb_config.layer_indicator_key < NUM_LEDS) {
                    rgb_set_color(rgb_config.layer_indicator_key, r, g, b);
                }
            }
        }
    }

    rgb_update();
}

#endif // RGB_ENABLED
