#include "rgb.h"

#if defined(RGB_ENABLED)

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
#include "rgb_animated.h"
#include "rgb_internal.h"
#include "rgb_math.h"
#include "rgb_reactive.h"
#include "rgb_static.h"

/*
 * Attribution:
 * Many effects in this file are adapted from QMK's RGB Matrix / RGB Light
 * effect set. libhmk keeps the QMK-aligned effect names where possible and
 * adds board-specific LED mapping/clipping around them.
 *
 * Local libhmk-only effects in this file are:
 *   - RGB_EFFECT_ANALOG
 *   - RGB_EFFECT_BINARY_CLOCK
 *   - RGB_EFFECT_PER_KEY
 *   - RGB_EFFECT_TRIGGER_STATE
 */

// We need an array to hold the current LED colors
static rgb_color_t current_colors[NUM_LEDS];
static uint8_t rgb_grb_data[NUM_LEDS * 3];
static rgb_config_t rgb_config;
static uint8_t rgb_clock_unique_y[NUM_LEDS];
static uint8_t rgb_clock_row_leds[NUM_LEDS];

typedef struct {
    bool initialized;
    bool valid;
    uint8_t digit_leds[4][4];
    uint8_t separator_leds[4];
    uint8_t second_leds[10];
} rgb_clock_layout_t;

typedef struct {
    bool synced;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint32_t sync_tick_ms;
} rgb_clock_state_t;

static rgb_clock_layout_t rgb_clock_layout;
static rgb_clock_state_t rgb_clock_state;

// Heatmap state
void rgb_matrix_record_keypress(uint8_t index) {
    rgb_reactive_record_keypress(index);
}

static void rgb_clock_reset_layout(void) {
    rgb_clock_layout.initialized = false;
    rgb_clock_layout.valid = false;
    memset(rgb_clock_layout.digit_leds, 0xFF, sizeof(rgb_clock_layout.digit_leds));
    memset(rgb_clock_layout.separator_leds, 0xFF,
           sizeof(rgb_clock_layout.separator_leds));
    memset(rgb_clock_layout.second_leds, 0xFF,
           sizeof(rgb_clock_layout.second_leds));
}

void rgb_set_clock_time(uint8_t hours, uint8_t minutes, uint8_t seconds) {
    rgb_clock_state.synced = true;
    rgb_clock_state.hours = hours;
    rgb_clock_state.minutes = minutes;
    rgb_clock_state.seconds = seconds;
    rgb_clock_state.sync_tick_ms = timer_read();
}

void rgb_init(void) {
    rgb_driver_init();
    memcpy(&rgb_config, &CURRENT_PROFILE.rgb_config, sizeof(rgb_config_t));
    rgb_clock_reset_layout();
    memset(&rgb_clock_state, 0, sizeof(rgb_clock_state));
    rgb_static_reset();
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
    uint16_t offset = 0;

    for (uint8_t i = 0; i < NUM_LEDS; i++) {
        rgb_grb_data[offset++] = current_colors[i].g;
        rgb_grb_data[offset++] = current_colors[i].r;
        rgb_grb_data[offset++] = current_colors[i].b;
    }

    rgb_driver_write(rgb_grb_data, offset);
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

uint8_t rgb_coord_x_at(uint8_t led) { return rgb_led_coords[led].x; }

uint8_t rgb_coord_y_at(uint8_t led) { return rgb_led_coords[led].y; }

bool rgb_led_is_mod_at(uint8_t led) { return rgb_led_is_mod[led] != 0u; }

uint8_t rgb_key_to_led_at(uint8_t key) { return rgb_key_to_led[key]; }

uint8_t rgb_reactive_clip_at(uint8_t source_led, uint8_t target_led) {
    return rgb_reactive_clip[source_led][target_led];
}

static rgb_color_t scale_rgb_color(rgb_color_t color, uint8_t brightness) {
    return (rgb_color_t){
        .r = (uint8_t)(((uint32_t)color.r * brightness) / 255u),
        .g = (uint8_t)(((uint32_t)color.g * brightness) / 255u),
        .b = (uint8_t)(((uint32_t)color.b * brightness) / 255u),
    };
}

static rgb_trigger_state_t rgb_trigger_state_for_key(const key_state_t *state) {
    if (state->is_pressed) {
        return state->key_dir == KEY_DIR_INACTIVE ? RGB_TRIGGER_STATE_HOLD
                                                  : RGB_TRIGGER_STATE_PRESS;
    }

    return state->key_dir == KEY_DIR_UP ? RGB_TRIGGER_STATE_RELEASE
                                        : RGB_TRIGGER_STATE_IDLE;
}

static void rgb_clock_sort_u8(uint8_t *values, uint16_t count) {
    for (uint16_t i = 1; i < count; i++) {
        uint8_t value = values[i];
        uint16_t j = i;
        while (j > 0 && values[j - 1u] > value) {
            values[j] = values[j - 1u];
            j--;
        }
        values[j] = value;
    }
}

static void rgb_clock_sort_leds_by_x(uint8_t *leds, uint16_t count) {
    for (uint16_t i = 1; i < count; i++) {
        uint8_t led = leds[i];
        uint8_t led_x = rgb_led_coords[led].x;
        uint16_t j = i;
        while (j > 0 && rgb_led_coords[leds[j - 1u]].x > led_x) {
            leds[j] = leds[j - 1u];
            j--;
        }
        leds[j] = led;
    }
}

static void rgb_clock_build_layout(void) {
    rgb_clock_reset_layout();
    rgb_clock_layout.initialized = true;

    uint16_t unique_y_count = 0;
    for (uint16_t led = 0; led < NUM_LEDS; led++) {
        uint8_t y = rgb_led_coords[led].y;
        bool seen = false;

        for (uint16_t i = 0; i < unique_y_count; i++) {
            if (rgb_clock_unique_y[i] == y) {
                seen = true;
                break;
            }
        }

        if (!seen)
            rgb_clock_unique_y[unique_y_count++] = y;
    }

    if (unique_y_count < 3u)
        return;

    rgb_clock_sort_u8(rgb_clock_unique_y, unique_y_count);

    for (uint8_t row = 0; row < 3u; row++) {
        uint16_t row_led_count = 0;

        for (uint16_t led = 0; led < NUM_LEDS; led++) {
            if (rgb_led_coords[led].y == rgb_clock_unique_y[row])
                rgb_clock_row_leds[row_led_count++] = led;
        }

        if (row_led_count < 10u)
            return;

        rgb_clock_sort_leds_by_x(rgb_clock_row_leds, row_led_count);

        if (row == 0u) {
            for (uint8_t bit = 0; bit < 4u; bit++) {
                rgb_clock_layout.digit_leds[0][bit] = rgb_clock_row_leds[bit];
                rgb_clock_layout.digit_leds[2][bit] =
                    rgb_clock_row_leds[row_led_count - 4u + bit];
            }
            rgb_clock_layout.separator_leds[0] = rgb_clock_row_leds[4];
            rgb_clock_layout.separator_leds[1] =
                rgb_clock_row_leds[row_led_count - 5u];
        } else if (row == 1u) {
            for (uint8_t bit = 0; bit < 4u; bit++) {
                rgb_clock_layout.digit_leds[1][bit] = rgb_clock_row_leds[bit];
                rgb_clock_layout.digit_leds[3][bit] =
                    rgb_clock_row_leds[row_led_count - 4u + bit];
            }
            rgb_clock_layout.separator_leds[2] = rgb_clock_row_leds[4];
            rgb_clock_layout.separator_leds[3] =
                rgb_clock_row_leds[row_led_count - 5u];
        } else {
            for (uint8_t i = 0; i < 5u; i++) {
                rgb_clock_layout.second_leds[i] = rgb_clock_row_leds[i];
                rgb_clock_layout.second_leds[5u + i] =
                    rgb_clock_row_leds[row_led_count - 5u + i];
            }
        }
    }

    rgb_clock_layout.valid = true;
}

static void rgb_clock_render(uint8_t effective_brightness,
                             uint32_t current_tick) {
    if (!rgb_clock_layout.initialized)
        rgb_clock_build_layout();

    rgb_set_all_color(0, 0, 0);

    const rgb_color_t active_color =
        scale_rgb_color(rgb_config.solid_color, effective_brightness);
    const rgb_color_t accent_color =
        scale_rgb_color(rgb_config.secondary_color, effective_brightness);
    const rgb_color_t background_color = scale_rgb_color(
        rgb_config.secondary_color,
        M_MAX((uint8_t)8u, (uint8_t)(effective_brightness / 10u)));

    if (!rgb_clock_layout.valid) {
        rgb_color_t pulse_color = ((current_tick / 500u) & 1u) == 0u
                                      ? active_color
                                      : (rgb_color_t){0, 0, 0};
        rgb_set_all_color(pulse_color.r, pulse_color.g, pulse_color.b);
        return;
    }

    if (!rgb_clock_state.synced) {
        const rgb_color_t pulse_color = ((current_tick / 500u) & 1u) == 0u
                                            ? active_color
                                            : background_color;
        for (uint8_t i = 0; i < M_ARRAY_SIZE(rgb_clock_layout.separator_leds);
             i++) {
            uint8_t led = rgb_clock_layout.separator_leds[i];
            if (led != 0xFFu)
                rgb_set_color(led, pulse_color.r, pulse_color.g, pulse_color.b);
        }
        for (uint8_t i = 0; i < M_ARRAY_SIZE(rgb_clock_layout.second_leds); i++) {
            uint8_t led = rgb_clock_layout.second_leds[i];
            if (led != 0xFFu)
                rgb_set_color(led, background_color.r, background_color.g,
                              background_color.b);
        }
        return;
    }

    const uint32_t elapsed_seconds = timer_elapsed(rgb_clock_state.sync_tick_ms) / 1000u;
    uint32_t total_seconds = (uint32_t)rgb_clock_state.hours * 3600u +
                             (uint32_t)rgb_clock_state.minutes * 60u +
                             (uint32_t)rgb_clock_state.seconds + elapsed_seconds;
    total_seconds %= 86400u;

    const uint8_t hours = (uint8_t)(total_seconds / 3600u);
    const uint8_t minutes = (uint8_t)((total_seconds % 3600u) / 60u);
    const uint8_t seconds = (uint8_t)(total_seconds % 60u);
    const uint8_t digits[4] = {
        (uint8_t)(hours / 10u),
        (uint8_t)(hours % 10u),
        (uint8_t)(minutes / 10u),
        (uint8_t)(minutes % 10u),
    };

    for (uint8_t digit = 0; digit < M_ARRAY_SIZE(rgb_clock_layout.digit_leds);
         digit++) {
        for (uint8_t bit = 0; bit < M_ARRAY_SIZE(rgb_clock_layout.digit_leds[0]);
             bit++) {
            const uint8_t led = rgb_clock_layout.digit_leds[digit][bit];
            if (led == 0xFFu)
                continue;

            const bool is_on = (digits[digit] & (uint8_t)(1u << (3u - bit))) != 0u;
            const rgb_color_t color = is_on ? active_color : (rgb_color_t){0, 0, 0};
            rgb_set_color(led, color.r, color.g, color.b);
        }
    }

    const rgb_color_t separator_color =
        (seconds & 1u) == 0u ? accent_color : background_color;
    for (uint8_t i = 0; i < M_ARRAY_SIZE(rgb_clock_layout.separator_leds); i++) {
        uint8_t led = rgb_clock_layout.separator_leds[i];
        if (led != 0xFFu)
            rgb_set_color(led, separator_color.r, separator_color.g,
                          separator_color.b);
    }

    const uint8_t second_step = seconds / 6u;
    const uint8_t second_phase =
        (uint8_t)((((uint32_t)(seconds % 6u)) + 1u) * effective_brightness / 6u);
    const rgb_color_t head_color =
        scale_rgb_color(rgb_config.solid_color, second_phase);

    for (uint8_t i = 0; i < M_ARRAY_SIZE(rgb_clock_layout.second_leds); i++) {
        uint8_t led = rgb_clock_layout.second_leds[i];
        if (led == 0xFFu)
            continue;

        rgb_color_t color = background_color;
        if (i < second_step)
            color = accent_color;
        else if (i == second_step)
            color = head_color;

        rgb_set_color(led, color.r, color.g, color.b);
    }
}

void rgb_task(void) {
    rgb_driver_task();

    if (!rgb_config.enabled) return;

    static uint32_t last_render_tick = 0;
    uint32_t current_tick = timer_read();

    rgb_reactive_decay_heatmap(current_tick);

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
    uint8_t secondary_hue = rgb_to_hue(rgb_config.secondary_color);
    static uint8_t prev_effect = 0xff;
    bool effect_changed = (prev_effect != rgb_config.current_effect);
    prev_effect = rgb_config.current_effect;
    rgb_animated_context_t animated_context = {
        .base_hue = base_hue,
        .effective_brightness = effective_brightness,
        .effect_speed = rgb_config.effect_speed,
        .effect_changed = effect_changed,
    };
    rgb_static_context_t static_context = {
        .config = &rgb_config,
        .base_hue = base_hue,
        .secondary_hue = secondary_hue,
        .effective_brightness = effective_brightness,
        .anim_timer = anim_timer,
        .scaled_timer = scaled_timer,
        .tick = tick,
        .prev_tick = prev_tick,
    };

    switch (rgb_config.current_effect) {
        case RGB_EFFECT_PIXEL_FLOW: {
            rgb_animated_render(RGB_EFFECT_PIXEL_FLOW, &animated_context);
            break;
        }

        case RGB_EFFECT_PIXEL_FRACTAL: {
            rgb_animated_render(RGB_EFFECT_PIXEL_FRACTAL, &animated_context);
            break;
        }

        case RGB_EFFECT_PIXEL_RAIN: {
            rgb_animated_render(RGB_EFFECT_PIXEL_RAIN, &animated_context);
            break;
        }

        case RGB_EFFECT_TYPING_HEATMAP: {
            rgb_reactive_render_heatmap(effective_brightness);
            break;
        }

        case RGB_EFFECT_DIGITAL_RAIN: {
            rgb_animated_render(RGB_EFFECT_DIGITAL_RAIN, &animated_context);
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
        case RGB_EFFECT_BINARY_CLOCK: {
            rgb_clock_render(effective_brightness, current_tick);
            break;
        }
        case RGB_EFFECT_TRIGGER_STATE: {
            rgb_color_t state_colors[RGB_TRIGGER_STATE_COLOR_COUNT];
            for (uint8_t state = 0; state < RGB_TRIGGER_STATE_COLOR_COUNT; state++) {
                state_colors[state] = scale_rgb_color(
                    rgb_config.trigger_state_colors[state], effective_brightness);
            }

            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                rgb_color_t color = {0, 0, 0};
                uint8_t key_index = rgb_led_key_index[i];

                if (key_index < NUM_KEYS) {
                    const key_state_t *state = &key_matrix[key_index];
                    color = state_colors[rgb_trigger_state_for_key(state)];
                }

                rgb_set_color(i, color.r, color.g, color.b);
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
            rgb_reactive_render_effect(rgb_config.current_effect, base_hue,
                                       effective_brightness,
                                       rgb_config.effect_speed);
            break;
        }
        case RGB_EFFECT_SPLASH:
        case RGB_EFFECT_MULTISPLASH:
        case RGB_EFFECT_SOLID_SPLASH:
        case RGB_EFFECT_SOLID_MULTISPLASH: {
            rgb_reactive_render_splash(rgb_config.current_effect, base_hue,
                                       effective_brightness,
                                       rgb_config.effect_speed);
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
            if (rgb_static_render(rgb_config.current_effect, &static_context)) {
                break;
            }

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
