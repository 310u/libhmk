/*
 * joystick.c - Xbox Analog Joystick Implementation
 */

#include "joystick.h"
#include "hardware/hardware.h"
#include "hardware/analog_api.h" // For analog_read
#include "hardware/board_api.h"  // For low-level GPIO
#include "config.h"
#include "hid.h"
#include "eeconfig.h" 
#include "keycodes.h"
#include <stdlib.h>

// Include driver specific header for GPIO access if needed
#if defined(MCU_AT32F405) || defined(MCU_AT32F402)
#include "at32f402_405.h"
#endif

//--------------------------------------------------------------------+
// Generalized Analog Input Handling
//--------------------------------------------------------------------+

typedef struct {
    int16_t current_val; // Processed output value
} analog_state_t;

static analog_state_t analog_states[NUM_ANALOG_CONFIGS];

// Helpers
static int8_t clip_int8(int32_t val) {
    if (val > 127) return 127;
    if (val < -127) return -127;
    return (int8_t)val;
}

static uint8_t clip_uint8(int32_t val) {
    if (val > 255) return 255;
    if (val < 0) return 0;
    return (uint8_t)val;
}

// Processing Functions
static int16_t process_centered(uint16_t raw, const analog_config_t *config) {
    // If unconfigured/uncalibrated, use 2048 as center
    uint16_t center = config->center_value ? config->center_value : 2048;
    uint16_t deadzone = config->deadzone ? config->deadzone : (ADC_MAX_VALUE * JOYSTICK_DEADZONE_PERCENT / 100);
    uint16_t min_val = config->min_value; // 0 if unconfig
    uint16_t max_val = config->max_value ? config->max_value : ADC_MAX_VALUE;

    // Apply inversion
    if (config->inverted) {
        raw = (ADC_MAX_VALUE - raw); 
    }

    int32_t delta = (int32_t)raw - center;
    if (abs(delta) < deadzone) return 0;

    // Map center-deadzone to center
    int32_t val = (delta > 0) ? (delta - deadzone) : (delta + deadzone);
    
    // Calculate range based on side
    int32_t range = (delta > 0) ? (max_val - center - deadzone) : (center - min_val - deadzone);
    if (range <= 0) range = 1;

    // Scale to -127 to 127
    int32_t scaled = val * 127 / range;
    return clip_int8(scaled);
}

static int16_t process_linear(uint16_t raw, const analog_config_t *config) {
    if (config->inverted) {
        raw = (ADC_MAX_VALUE - raw);
    }
    
    // Default calibration fallback
    uint16_t min = config->min_value;
    uint16_t max = config->max_value ? config->max_value : ADC_MAX_VALUE;

    // Clamp to range
    if (raw <= min) return 0;
    if (raw >= max) return 255;

    // Scale
    uint32_t range = max - min;
    if (range == 0) return 0; // Avoid Div/0

    return (int16_t)((uint32_t)(raw - min) * 255 / range);
}

// Global State for Button
static bool joystick_btn_state = false;

void joystick_init(void) {
    // Initialize Button GPIO
    gpio_init_type gpio_init_struct;
    
    // Enable Clock for the configured port
    if (JOYSTICK_BUTTON_GPIO_PORT == GPIOA) crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    else if (JOYSTICK_BUTTON_GPIO_PORT == GPIOB) crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    else if (JOYSTICK_BUTTON_GPIO_PORT == GPIOC) crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_pins = JOYSTICK_BUTTON_GPIO_PIN;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL; 
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    
    gpio_init(JOYSTICK_BUTTON_GPIO_PORT, &gpio_init_struct);
}

void joystick_task(void) {
    const eeconfig_profile_t* profile = &CURRENT_PROFILE;

    int8_t mouse_x = 0;
    int8_t mouse_y = 0;

    // Process Inputs
    for (size_t i = 0; i < NUM_ANALOG_CONFIGS; i++) {
        const analog_config_t* config = &profile->analog_configs[i];
        
        if (config->id == 0) continue; // Unused slot

        uint16_t raw = analog_read(config->id);
        
        int16_t val = 0;
        if (config->type == 0) { // ANALOG_TYPE_CENTERED (Assuming enum 0)
             // We should define ANALOG_TYPE_CENTERED in common.h or config.h
             // For now assuming 0 from previous context
             val = process_centered(raw, config);
        } else {
             val = process_linear(raw, config);
        }
        analog_states[i].current_val = val;

        // Apply Function
        switch (config->function) {
            case ANALOG_FUNC_MOUSE_X: mouse_x += (int8_t)val; break;
            case ANALOG_FUNC_MOUSE_Y: mouse_y += (int8_t)val; break;
            // TODO: Implement Gamepad Axis mapping
            default: break;
        }
    }

    hid_mouse_xy_update(mouse_x, mouse_y);

    // 4. Button Reading
    joy_state_type bit_status = gpio_input_data_bit_read(JOYSTICK_BUTTON_GPIO_PORT, JOYSTICK_BUTTON_GPIO_PIN);
    bool current_btn_state = (bit_status == GPIO_PIN_RESET);

    if (current_btn_state != joystick_btn_state) {
        joystick_btn_state = current_btn_state;
        if (joystick_btn_state) {
            hid_keycode_add(JOYSTICK_BUTTON_KEYCODE);
        } else {
            hid_keycode_remove(JOYSTICK_BUTTON_KEYCODE);
        }
    }
}

// helper to find value
static int16_t get_analog_val(uint8_t function) {
    const eeconfig_profile_t* profile = &CURRENT_PROFILE;
    for (size_t i = 0; i < NUM_ANALOG_CONFIGS; i++) {
        if (profile->analog_configs[i].function == function) {
            return analog_states[i].current_val;
        }
    }
    return 0;
}

// These are legacy getters, might be unused or used by other modules
int8_t joystick_get_x(void) { return (int8_t)get_analog_val(ANALOG_FUNC_MOUSE_X); }
int8_t joystick_get_y(void) { return (int8_t)get_analog_val(ANALOG_FUNC_MOUSE_Y); }

int8_t joystick_2_get_x(void) { return 0; } // Deprecated/Unused
int8_t joystick_2_get_y(void) { return 0; } // Deprecated/Unused

bool joystick_get_button(void) { return joystick_btn_state; }
