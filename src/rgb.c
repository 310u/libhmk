#include "rgb.h"

#if defined(RGB_ENABLED)

#if defined(NUM_LEDS)
// Use custom if provided
#else
#define NUM_LEDS NUM_KEYS
#endif

#include "hardware/hardware.h"
#include "matrix.h"
#include "layout.h"
#include "rgb_coords.h"

// We need an array to hold the current LED colors
static rgb_color_t current_colors[NUM_LEDS];
static rgb_config_t rgb_config;

// Heatmap state
static uint8_t rgb_heatmap[NUM_LEDS] = {0};

void rgb_matrix_record_keypress(uint8_t index) {
    if (index < NUM_LEDS) {
        // Increase heat by a large amount (e.g. 130) capped at 255
        uint16_t heat = rgb_heatmap[index] + 130;
        rgb_heatmap[index] = (heat > 255) ? 255 : heat;
        
        // Slightly heat up neighbors based on coordinate proximity
        led_point_t p1 = rgb_led_coords[index];
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            if (i == index) continue;
            led_point_t p2 = rgb_led_coords[i];
            int dx = p1.x - p2.x;
            int dy = p1.y - p2.y;
            int dist_sq = dx*dx + dy*dy;
            if (dist_sq < 400) { // e.g. within 1-1.5 keys distance (1U = 12) 12*12=144 
                uint16_t n_heat = rgb_heatmap[i] + 40;
                rgb_heatmap[i] = (n_heat > 255) ? 255 : n_heat;
            }
        }
    }
}

void rgb_init(void) {
    // Initialize GPIO for RGB_DATA_PIN.
    // Setting up AT32 PWM+DMA is complex, for this stub we initialize standard variables.
    // In a full implementation, you'd setup TMR (Timer) and DMA here.
    
    // Default config fallback
    rgb_config.enabled = 1;
    rgb_config.global_brightness = 255;
    rgb_config.current_effect = RGB_EFFECT_SOLID_COLOR;
    rgb_config.solid_color.r = 255;
    rgb_config.solid_color.g = 0;
    rgb_config.solid_color.b = 0;
    rgb_config.effect_speed = 128;
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
    // Logic to convert current_colors to PWM duty cycles and start DMA
    // (Stubbed out hardware specifics)
}

void rgb_update(void) {
    if (!rgb_config.enabled) {
        rgb_set_all_color(0, 0, 0);
        rgb_transmit_dma();
        return;
    }
    rgb_transmit_dma();
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
    if (!rgb_config.enabled) return;

    static uint32_t last_render_tick = 0;
    static uint32_t heatmap_tick = 0;
    uint32_t current_tick = timer_read();
    
    // Heatmap decay process (every 25ms)
    if (timer_elapsed(heatmap_tick) >= 25) {
        for (uint8_t i = 0; i < NUM_LEDS; i++) {
            if (rgb_heatmap[i] > 0) {
                // Decay value. Reduce by 2 per tick.
                rgb_heatmap[i] = (rgb_heatmap[i] > 2) ? (rgb_heatmap[i] - 2) : 0;
            }
        }
        heatmap_tick = current_tick;
    }

    // Limit render framerate to ~60fps (16ms)
    if (timer_elapsed(last_render_tick) < 16) return;
    last_render_tick = current_tick;

    uint8_t effective_brightness = rgb_config.global_brightness;
    uint32_t idle_time = matrix_get_idle_time();
    uint32_t timeout_ms = rgb_config.sleep_timeout * 60000;
    
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
    // effect_speed = 128 is "normal". 255 is approx double. 0 is paused.
    static uint32_t anim_timer = 0;
    anim_timer += (rgb_config.effect_speed * 16) / 128;

    switch (rgb_config.current_effect) {
        case RGB_EFFECT_SOLID_COLOR:
            rgb_set_all_color(rgb_config.solid_color.r, rgb_config.solid_color.g, rgb_config.solid_color.b);
            break;

        case RGB_EFFECT_BREATHING: {
            // Breathing uses a triangle wave based on anim_timer
            uint8_t phase = (anim_timer / 4) % 255;
            uint8_t val = (phase > 127) ? (255 - phase) * 2 : phase * 2;
            // Map solid color brightness to val while keeping max global_brightness
            uint32_t r = (rgb_config.solid_color.r * val) >> 8;
            uint32_t g = (rgb_config.solid_color.g * val) >> 8;
            uint32_t b = (rgb_config.solid_color.b * val) >> 8;
            rgb_set_all_color(r, g, b);
            break;
        }

        case RGB_EFFECT_CYCLE_LEFT_RIGHT: {
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t x = rgb_led_coords[i].x;
                hsv_t hsv = {
                    .h = (uint8_t)((anim_timer / 4) + x),
                    .s = 255,
                    .v = effective_brightness
                };
                rgb_color_t c = hsv_to_rgb(hsv);
                rgb_set_color(i, c.r, c.g, c.b);
            }
            break;
        }

        case RGB_EFFECT_TYPING_HEATMAP: {
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t temp = rgb_heatmap[i];
                // Map temp 0-255 to Hue: Blue (170) -> Red (0)
                // When 0, hue doesn't matter much if V=0, but to have a slow fade out
                // we set base glow if desired, or just pitch black.
                // Let's do dark blue for 0 heat, bright red/white for 255
                uint8_t h = 170 - ((temp * 170) / 255); // 170 down to 0
                uint8_t v = (temp > 0) ? (temp / 2 + effective_brightness / 2) : 0; 
                if (v > effective_brightness) v = effective_brightness;
                if (temp == 0) {
                    rgb_set_color(i, 0, 0, 0);
                } else {
                    hsv_t hsv = { .h = h, .s = 255, .v = v };
                    rgb_color_t c = hsv_to_rgb(hsv);
                    rgb_set_color(i, c.r, c.g, c.b);
                }
            }
            break;
        }

        case RGB_EFFECT_ANALOG: {
            for (uint8_t i = 0; i < NUM_LEDS; i++) {
                uint8_t dist = key_matrix[i].distance;
                if (dist == 0) {
                    rgb_set_color(i, 0, 0, 0);
                } else {
                    // Hue: 85 (Green) -> 0 (Red)
                    uint8_t h = 85 - ((dist * 85) / 255);
                    // Brightness: scales up to effective_brightness
                    uint32_t v = ((uint32_t)dist * effective_brightness) / 255;
                    hsv_t hsv = { .h = h, .s = 255, .v = (uint8_t)v };
                    rgb_color_t c = hsv_to_rgb(hsv);
                    rgb_set_color(i, c.r, c.g, c.b);
                }
            }
            break;
        }

        default:
            // Fallback for unimplemented effects
            rgb_set_all_color(0, 0, 0);
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
