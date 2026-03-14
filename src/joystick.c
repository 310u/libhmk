#include <math.h>

#include "joystick.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "layout.h"
#include "wear_leveling.h"

#include "at32f402_405.h"

#if defined(JOYSTICK_ENABLED)

#ifndef JOYSTICK_X_ADC_INDEX
#error "JOYSTICK_X_ADC_INDEX not defined in board_def.h"
#endif

#ifndef JOYSTICK_Y_ADC_INDEX
#error "JOYSTICK_Y_ADC_INDEX not defined in board_def.h"
#endif

#ifndef JOYSTICK_SW_PIN
#error "JOYSTICK_SW_PIN not defined in board_def.h"
#endif

#ifndef JOYSTICK_SW_PORT
#error "JOYSTICK_SW_PORT not defined in board_def.h"
#endif

// Smoothing factor (0 = no smoothing, 15 = max smoothing)
#define JOYSTICK_SMOOTHING 4
#define JOYSTICK_MOUSE_REPORT_INTERVAL_MS 10u
#define JOYSTICK_SCROLL_REPORT_INTERVAL_MS 25u
#define JOYSTICK_MOUSE_FP_SHIFT 8
#define JOYSTICK_MOUSE_FP_ONE (1L << JOYSTICK_MOUSE_FP_SHIFT)
#define JOYSTICK_MOUSE_ACCELERATION_DEFAULT 255u
#define JOYSTICK_MOUSE_DIVISOR 50L
#define JOYSTICK_SCROLL_DIVISOR 250L
#define JOYSTICK_VECTOR_MAX 181u

static joystick_state_t current_state = {0};
static joystick_config_t config_cache = {0};

// Debounce state for push switch
static bool sw_raw = false;
static uint32_t sw_last_change_tick = 0;
static bool sw_debounced = false;
static uint32_t last_mouse_tick = 0;
static int32_t mouse_accum_x = 0;
static int32_t mouse_accum_y = 0;
static int32_t scroll_accum_x = 0;
static int32_t scroll_accum_y = 0;

static uint16_t smooth_adc(uint16_t old_val, uint16_t new_val) {
    if (old_val == 0) return new_val;
    return ((old_val * ((1 << JOYSTICK_SMOOTHING) - 1)) + new_val) >> JOYSTICK_SMOOTHING;
}

static int8_t apply_calibration(uint16_t raw_val, joystick_axis_calibration_t *cal) {
    // 0 = min, 2048 = center, 4095 = max
    int32_t val = raw_val;
    int32_t center = cal->center;
    int32_t min = cal->min;
    int32_t max = cal->max;

    // Failsafe in case of uncalibrated EEPROM
    if (center == 0 || max == 0) {
        center = 2048;
        min = 0;
        max = 4095;
    }

    int32_t dist_from_center = val - center;

    int32_t result = 0;
    if (dist_from_center > 0) {
        // Positive side
        int32_t range = max - center;
        if (range <= 0) range = 1; // Failsafe

        result = (dist_from_center * 127) / range;
        if (result > 127) result = 127;
    } else {
        // Negative side
        int32_t range = center - min;
        if (range <= 0) range = 1; // Failsafe

        result = (dist_from_center * 128) / range;
        if (result < -128) result = -128;
    }

    return (int8_t)result;
}

static uint16_t joystick_vector_length(int8_t x, int8_t y) {
    int16_t x16 = x;
    int16_t y16 = y;
    return (uint16_t)lroundf(sqrtf((float)(x16 * x16 + y16 * y16)));
}

static int8_t joystick_clamp_i16_to_i8(int16_t value) {
    if (value > INT8_MAX) {
        return INT8_MAX;
    }
    if (value < INT8_MIN) {
        return INT8_MIN;
    }
    return (int8_t)value;
}

static void joystick_apply_radial_deadzone(int8_t *x, int8_t *y, uint8_t deadzone) {
    uint16_t magnitude = joystick_vector_length(*x, *y);
    if (magnitude == 0u) {
        return;
    }

    if (deadzone >= 255u) {
        *x = 0;
        *y = 0;
        return;
    }

    uint16_t magnitude_norm = (uint16_t)((uint32_t)magnitude * 255u / JOYSTICK_VECTOR_MAX);
    if (magnitude_norm <= deadzone) {
        *x = 0;
        *y = 0;
        return;
    }

    uint16_t scaled_norm =
        (uint16_t)(((uint32_t)(magnitude_norm - deadzone) * 255u) / (255u - deadzone));
    int16_t scaled_x = (int16_t)(((int32_t)(*x) * scaled_norm) / magnitude_norm);
    int16_t scaled_y = (int16_t)(((int32_t)(*y) * scaled_norm) / magnitude_norm);

    // Radial remap can exceed the int8 axis range near full throw; clamp instead of wrapping.
    *x = joystick_clamp_i16_to_i8(scaled_x);
    *y = joystick_clamp_i16_to_i8(scaled_y);
}

static int32_t joystick_vector_delta_fp(uint16_t magnitude, uint8_t speed,
                                        uint8_t acceleration, int32_t divisor) {
    const int64_t max_sq =
        (int64_t)JOYSTICK_VECTOR_MAX * (int64_t)JOYSTICK_VECTOR_MAX;
    const int64_t mag_sq = (int64_t)magnitude * (int64_t)magnitude;
    const int64_t curve_term =
        ((int64_t)(255u - acceleration) * max_sq +
         (int64_t)acceleration * mag_sq) /
        255LL;
    int64_t numerator = (int64_t)magnitude * curve_term;
    numerator *= (int64_t)speed;
    numerator *= (int64_t)JOYSTICK_MOUSE_FP_ONE;

    int64_t denominator = 16384LL * (int64_t)divisor;
    int64_t delta_fp = numerator / denominator;

    if (delta_fp > INT32_MAX) {
        return INT32_MAX;
    }
    return (int32_t)delta_fp;
}

static int8_t joystick_consume_mouse_accum(int32_t *accum) {
    int32_t whole = *accum / JOYSTICK_MOUSE_FP_ONE;

    if (whole > INT8_MAX) {
        whole = INT8_MAX;
    } else if (whole < INT8_MIN) {
        whole = INT8_MIN;
    }

    *accum -= whole * JOYSTICK_MOUSE_FP_ONE;
    return (int8_t)whole;
}

static uint8_t joystick_effective_mouse_acceleration(uint8_t raw) {
    return raw == 0u ? JOYSTICK_MOUSE_ACCELERATION_DEFAULT : raw;
}

void joystick_init(void) {
    // Init GPIO switch pin (input with pullup)
    gpio_init_type gpio_init_struct;
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = JOYSTICK_SW_PIN;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init(JOYSTICK_SW_PORT, &gpio_init_struct);

    // Initial load of config cache
    config_cache = joystick_get_config();

    // Reset state
    current_state.raw_x = 0;
    current_state.raw_y = 0;
    current_state.out_x = 0;
    current_state.out_y = 0;
    current_state.sw = false;

    // Initialize debounce state
    sw_raw = false;
    sw_debounced = false;
    sw_last_change_tick = 0;
    last_mouse_tick = timer_read();
    mouse_accum_x = 0;
    mouse_accum_y = 0;
    scroll_accum_x = 0;
    scroll_accum_y = 0;
}

// Global state getters/setters
joystick_config_t joystick_get_config(void) {
    if (eeconfig != NULL) {
        return CURRENT_PROFILE.joystick_config;
    }
    // Default fallback
    joystick_config_t def = {
        .x = {0, 2048, 4095},
        .y = {0, 2048, 4095},
        .deadzone = 150,
        .mode = JOYSTICK_MODE_MOUSE,
        .mouse_speed = 10,
        .mouse_acceleration = JOYSTICK_MOUSE_ACCELERATION_DEFAULT,
        .sw_debounce_ms = 5,
    };
    return def;
}

void joystick_set_config(joystick_config_t config) {
    if (eeconfig != NULL) {
        uint32_t addr = offsetof(eeconfig_t, profiles) +
                        eeconfig->current_profile * sizeof(eeconfig_profile_t) +
                        offsetof(eeconfig_profile_t, joystick_config);
        (void)wear_leveling_write(addr, &config, sizeof(config));
    }
    config_cache = config;
}

joystick_state_t joystick_get_state(void) {
    return current_state;
}

void joystick_task(void) {
    // Read raw ADC values
    uint16_t x_raw = analog_read_raw(JOYSTICK_X_ADC_INDEX);
    uint16_t y_raw = analog_read_raw(JOYSTICK_Y_ADC_INDEX);
    // Apply smoothing
    current_state.raw_x = smooth_adc(current_state.raw_x, x_raw);
    current_state.raw_y = smooth_adc(current_state.raw_y, y_raw);
    // Read push switch with debounce
    bool sw_current = (gpio_input_data_bit_read(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN) == RESET);
    if (sw_current != sw_raw) {
        sw_raw = sw_current;
        sw_last_change_tick = timer_read();
    }
    if (config_cache.sw_debounce_ms == 0) {
        // Debounce disabled
        sw_debounced = sw_raw;
    } else if (timer_elapsed(sw_last_change_tick) >= (uint32_t)config_cache.sw_debounce_ms) {
        sw_debounced = sw_raw;
    }
    current_state.sw = sw_debounced;

    // Apply calibration
    current_state.out_x = apply_calibration(current_state.raw_x, &config_cache.x);
    current_state.out_y = apply_calibration(current_state.raw_y, &config_cache.y);
    joystick_apply_radial_deadzone(&current_state.out_x, &current_state.out_y, config_cache.deadzone);

    // Handle Output Modes
    // XInput modes are handled directly by xinput.c querying joystick_get_state()
    
    // Mouse mode
    if (config_cache.mode == JOYSTICK_MODE_MOUSE) {
        // Needs a tick pacing mechanism so it doesn't slam the host with max speed HID reports
        uint32_t tick = timer_read();
        if (timer_elapsed(last_mouse_tick) >= JOYSTICK_MOUSE_REPORT_INTERVAL_MS) {
            if (current_state.out_x != 0 || current_state.out_y != 0 || current_state.sw) {
                uint8_t acceleration =
                    joystick_effective_mouse_acceleration(config_cache.mouse_acceleration);
                uint16_t magnitude =
                    joystick_vector_length(current_state.out_x, current_state.out_y);
                int32_t dx_fp = 0;
                int32_t dy_fp = 0;
                if (magnitude != 0u) {
                    int32_t delta_fp =
                        joystick_vector_delta_fp(magnitude, config_cache.mouse_speed,
                                                 acceleration,
                                                 JOYSTICK_MOUSE_DIVISOR);
                    dx_fp = ((int32_t)current_state.out_x * delta_fp) / (int32_t)magnitude;
                    dy_fp = ((int32_t)current_state.out_y * delta_fp) / (int32_t)magnitude;
                }

                if (is_sniper_active) {
                    dx_fp = (dx_fp * eeconfig->options.sniper_mode_multiplier) / 255;
                    dy_fp = (dy_fp * eeconfig->options.sniper_mode_multiplier) / 255;
                }

                mouse_accum_x += dx_fp;
                mouse_accum_y -= dy_fp; // Up should be negative Y in mouse coordinates.

                int8_t dx = joystick_consume_mouse_accum(&mouse_accum_x);
                int8_t dy = joystick_consume_mouse_accum(&mouse_accum_y);

                // For some reason Y axis might be inverted depending on hardware orientation, adjust here if needed
                uint8_t buttons = 0;
                if (current_state.sw) buttons |= 1; // Left click

                // Call hid_mouse_move which is now available in hid.c
                hid_mouse_move(dx, dy, buttons);
            }
            last_mouse_tick = tick;
        }
    } else if (config_cache.mode == JOYSTICK_MODE_SCROLL) {
        uint32_t tick = timer_read();
        if (timer_elapsed(last_mouse_tick) >= JOYSTICK_SCROLL_REPORT_INTERVAL_MS) {
            if (current_state.out_x != 0 || current_state.out_y != 0) {
                uint16_t magnitude =
                    joystick_vector_length(current_state.out_x, current_state.out_y);
                int32_t dx_fp = 0;
                int32_t dy_fp = 0;
                if (magnitude != 0u) {
                    int32_t delta_fp =
                        joystick_vector_delta_fp(magnitude, config_cache.mouse_speed,
                                                 JOYSTICK_MOUSE_ACCELERATION_DEFAULT,
                                                 JOYSTICK_SCROLL_DIVISOR);
                    dx_fp = ((int32_t)current_state.out_x * delta_fp) / (int32_t)magnitude;
                    dy_fp = ((int32_t)current_state.out_y * delta_fp) / (int32_t)magnitude;
                }

                if (is_sniper_active) {
                    dx_fp = (dx_fp * eeconfig->options.sniper_mode_multiplier) / 255;
                    dy_fp = (dy_fp * eeconfig->options.sniper_mode_multiplier) / 255;
                }

                scroll_accum_x += dx_fp;
                scroll_accum_y += dy_fp;

                int8_t pan = joystick_consume_mouse_accum(&scroll_accum_x);
                int8_t wheel = joystick_consume_mouse_accum(&scroll_accum_y);

                // Typical orientation: up joystick = positive scroll (up)
                // Right joystick = positive pan (right)
                hid_mouse_scroll(wheel, pan);
            }
            uint8_t buttons = 0;
            if (current_state.sw) buttons |= 1; // Allows click while scrolling
            // If we just clicked, send a move with 0 dx/dy to register the click quickly
            if (current_state.sw) hid_mouse_move(0, 0, buttons);
            last_mouse_tick = tick;
        }
    }
}

#endif // JOYSTICK_ENABLED
