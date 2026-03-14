#pragma once

#include "common.h"

#if defined(RGB_ENABLED)

// Color and Configuration Structures
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

typedef struct {
    uint8_t h; // 0-255
    uint8_t s; // 0-255
    uint8_t v; // 0-255
} hsv_t;

// Most effect names intentionally mirror QMK RGB Matrix / RGB Light effects.
// In libhmk, ANALOG and PER_KEY are local extensions; OFF maps to QMK's NONE.
typedef enum {
    RGB_EFFECT_OFF = 0,
    RGB_EFFECT_SOLID_COLOR = 1,
    RGB_EFFECT_ALPHAS_MODS,
    RGB_EFFECT_GRADIENT_UP_DOWN,
    RGB_EFFECT_GRADIENT_LEFT_RIGHT,
    RGB_EFFECT_BREATHING,
    RGB_EFFECT_BAND_SAT,
    RGB_EFFECT_BAND_VAL,
    RGB_EFFECT_BAND_PINWHEEL_SAT,
    RGB_EFFECT_BAND_PINWHEEL_VAL,
    RGB_EFFECT_BAND_SPIRAL_SAT,
    RGB_EFFECT_BAND_SPIRAL_VAL,
    RGB_EFFECT_CYCLE_ALL,
    RGB_EFFECT_CYCLE_LEFT_RIGHT,
    RGB_EFFECT_CYCLE_UP_DOWN,
    RGB_EFFECT_CYCLE_OUT_IN,
    RGB_EFFECT_CYCLE_OUT_IN_DUAL,
    RGB_EFFECT_RAINBOW_MOVING_CHEVRON,
    RGB_EFFECT_CYCLE_PINWHEEL,
    RGB_EFFECT_CYCLE_SPIRAL,
    RGB_EFFECT_DUAL_BEACON,
    RGB_EFFECT_RAINBOW_BEACON,
    RGB_EFFECT_RAINBOW_PINWHEELS,
    RGB_EFFECT_FLOWER_BLOOMING,
    RGB_EFFECT_RAINDROPS,
    RGB_EFFECT_JELLYBEAN_RAINDROPS,
    RGB_EFFECT_HUE_BREATHING,
    RGB_EFFECT_HUE_PENDULUM,
    RGB_EFFECT_HUE_WAVE,
    RGB_EFFECT_PIXEL_FRACTAL,
    RGB_EFFECT_PIXEL_FLOW,
    RGB_EFFECT_PIXEL_RAIN,
    RGB_EFFECT_TYPING_HEATMAP,
    RGB_EFFECT_DIGITAL_RAIN,
    RGB_EFFECT_SOLID_REACTIVE_SIMPLE,
    RGB_EFFECT_SOLID_REACTIVE,
    RGB_EFFECT_SOLID_REACTIVE_WIDE,
    RGB_EFFECT_SOLID_REACTIVE_MULTIWIDE,
    RGB_EFFECT_SOLID_REACTIVE_CROSS,
    RGB_EFFECT_SOLID_REACTIVE_MULTICROSS,
    RGB_EFFECT_SOLID_REACTIVE_NEXUS,
    RGB_EFFECT_SOLID_REACTIVE_MULTINEXUS,
    RGB_EFFECT_SPLASH,
    RGB_EFFECT_MULTISPLASH,
    RGB_EFFECT_SOLID_SPLASH,
    RGB_EFFECT_SOLID_MULTISPLASH,
    RGB_EFFECT_STARLIGHT,
    RGB_EFFECT_STARLIGHT_SMOOTH,
    RGB_EFFECT_STARLIGHT_DUAL_HUE,
    RGB_EFFECT_STARLIGHT_DUAL_SAT,
    RGB_EFFECT_RIVERFLOW,
    RGB_EFFECT_ANALOG,
    RGB_EFFECT_PER_KEY,
    RGB_EFFECT_MAX
} rgb_effect_t;

typedef struct {
    uint8_t enabled;
    uint8_t global_brightness;
    uint8_t current_effect;
    rgb_color_t solid_color;
    rgb_color_t secondary_color;
    uint8_t effect_speed;
    uint8_t sleep_timeout; // in minutes, 0 = disabled
    uint8_t layer_indicator_mode; // 0=Fill, 1=Flash, 2=Specific Key
    uint8_t layer_indicator_key;  // LED index for mode 2
    rgb_color_t layer_colors[NUM_LAYERS];
    rgb_color_t per_key_colors[NUM_KEYS];
} rgb_config_t;

// API
void rgb_init(void);
void rgb_task(void);
void rgb_set_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void rgb_set_all_color(uint8_t r, uint8_t g, uint8_t b);
void rgb_update(void);
rgb_color_t hsv_to_rgb(hsv_t hsv);
void rgb_matrix_record_keypress(uint8_t index);

// Provide access to the configuration block for EEPROM
rgb_config_t* rgb_get_config(void);
void rgb_apply_config(void);

#endif // RGB_ENABLED
