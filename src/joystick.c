#include "joystick.h"
#include "board_def.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "layout.h"

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

static joystick_state_t current_state = {0};
static joystick_config_t config_cache = {0};

static uint16_t smooth_adc(uint16_t old_val, uint16_t new_val) {
    if (old_val == 0) return new_val;
    return ((old_val * ((1 << JOYSTICK_SMOOTHING) - 1)) + new_val) >> JOYSTICK_SMOOTHING;
}

static int8_t apply_calibration(uint16_t raw_val, joystick_axis_calibration_t *cal, uint8_t deadzone) {
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

    // Apply deadzone
    if (dist_from_center > -deadzone && dist_from_center < deadzone) {
        return 0;
    }

    int32_t result = 0;
    if (dist_from_center > 0) {
        // Positive side
        int32_t range = max - center;
        if (range <= 0) range = 1; // Failsafe
        
        // Remove deadzone from calculation range
        dist_from_center -= deadzone;
        range -= deadzone;
        if (range <= 0) range = 1;

        result = (dist_from_center * 127) / range;
        if (result > 127) result = 127;
    } else {
        // Negative side
        int32_t range = center - min;
        if (range <= 0) range = 1; // Failsafe

        dist_from_center += deadzone;
        range -= deadzone;
        if (range <= 0) range = 1;

        result = (dist_from_center * 128) / range;
        if (result < -128) result = -128;
    }

    return (int8_t)result;
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
}

// Global state getters/setters
joystick_config_t joystick_get_config(void) {
    if (eeconfig_is_initialized()) {
        return eeconfig_get_profile()->joystick_config;
    }
    // Default fallback
    joystick_config_t def = {
        .x = {0, 2048, 4095},
        .y = {0, 2048, 4095},
        .deadzone = 150,
        .mode = JOYSTICK_MODE_MOUSE,
        .mouse_speed = 10,
    };
    return def;
}

void joystick_set_config(joystick_config_t config) {
    if (eeconfig_is_initialized()) {
        eeconfig_profile_t* p = eeconfig_get_profile();
        p->joystick_config = config;
        eeconfig_sync();
        config_cache = config;
    }
}

joystick_state_t joystick_get_state(void) {
    return current_state;
}

static uint32_t last_mouse_tick = 0;

void joystick_task(void) {
    // Read raw ADC values
    uint16_t x_raw = analog_read_raw(JOYSTICK_X_ADC_INDEX);
    uint16_t y_raw = analog_read_raw(JOYSTICK_Y_ADC_INDEX);
    // Apply smoothing
    current_state.raw_x = smooth_adc(current_state.raw_x, x_raw);
    current_state.raw_y = smooth_adc(current_state.raw_y, y_raw);
    current_state.sw = (gpio_input_data_bit_read(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN) == RESET);

    // Apply calibration
    current_state.out_x = apply_calibration(current_state.raw_x, &config_cache.x, config_cache.deadzone);
    current_state.out_y = apply_calibration(current_state.raw_y, &config_cache.y, config_cache.deadzone);

    // Handle Output Modes
    // XInput modes are handled directly by xinput.c querying joystick_get_state()
    
    // Mouse mode
    if (config_cache.mode == JOYSTICK_MODE_MOUSE) {
        // Needs a tick pacing mechanism so it doesn't slam the host with max speed HID reports
        uint32_t tick = system_get_ms();
        if (tick - last_mouse_tick >= 10) { // 100Hz max update rate for mouse logic
            if (current_state.out_x != 0 || current_state.out_y != 0 || current_state.sw) {
                // Determine movement deltas based on out_x and mouse_speed
                // Mapping -128..127 to pixels depending on speed.
                
                // Simple non-linear scaling: square the input for better accuracy at slow speeds
                int32_t dx = (int32_t)current_state.out_x * current_state.out_x * current_state.out_x / 16384;
                int32_t dy = (int32_t)current_state.out_y * current_state.out_y * current_state.out_y / 16384;

                dx = (dx * config_cache.mouse_speed) / 50;
                dy = (dy * config_cache.mouse_speed) / 50;

                if (is_sniper_active) {
                    dx = (dx * eeconfig->options.sniper_mode_multiplier) / 255;
                    dy = (dy * eeconfig->options.sniper_mode_multiplier) / 255;
                }

                // For some reason Y axis might be inverted depending on hardware orientation, adjust here if needed
                // Usually up is negative Y in mouse coordinates
                dy = -dy; 

                uint8_t buttons = 0;
                if (current_state.sw) buttons |= 1; // Left click

                // Call hid_mouse_move which is now available in hid.c
                hid_mouse_move((int8_t)dx, (int8_t)dy, buttons);
            }
            last_mouse_tick = tick;
        }
    } else if (config_cache.mode == JOYSTICK_MODE_SCROLL) {
        uint32_t tick = system_get_ms();
        if (tick - last_mouse_tick >= 25) { // 40Hz for scrolling to avoid going too fast
            if (current_state.out_x != 0 || current_state.out_y != 0) {
                // Non-linear scaling
                int32_t dx = (int32_t)current_state.out_x * current_state.out_x * current_state.out_x / 16384;
                int32_t dy = (int32_t)current_state.out_y * current_state.out_y * current_state.out_y / 16384;

                dx = (dx * config_cache.mouse_speed) / 250;
                dy = (dy * config_cache.mouse_speed) / 250;

                if (is_sniper_active) {
                    dx = (dx * eeconfig->options.sniper_mode_multiplier) / 255;
                    dy = (dy * eeconfig->options.sniper_mode_multiplier) / 255;
                }

                // Typical orientation: up joystick = positive scroll (up)
                // Right joystick = positive pan (right)
                hid_mouse_scroll((int8_t)dy, (int8_t)dx);
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
