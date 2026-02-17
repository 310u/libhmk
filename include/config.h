/*
 * config.h - Joystick Configuration
 */

#pragma once

//--------------------------------------------------------------------+
// Joystick Configuration
//--------------------------------------------------------------------+

// Analog Input Configuration
#define ANALOG_SPECIAL_START_ID 240

// Analog Input Types
#define ANALOG_TYPE_CENTERED 0 // For Joysticks (Center is neutral)
#define ANALOG_TYPE_LINEAR   1 // For Sliders/Knobs (End is neutral)

// Joystick Matrix IDs
// Use special IDs (252-255) to map joystick channels.
#define JOYSTICK_1_X_MATRIX_ID 254
#define JOYSTICK_1_Y_MATRIX_ID 255
#define JOYSTICK_2_X_MATRIX_ID 252
#define JOYSTICK_2_Y_MATRIX_ID 253

// Analog Configuration List (for processing logic)
// Format: { MATRIX_ID, ANALOG_TYPE }
#define ANALOG_INPUT_CONFIG \
    { JOYSTICK_1_X_MATRIX_ID, ANALOG_TYPE_CENTERED }, \
    { JOYSTICK_1_Y_MATRIX_ID, ANALOG_TYPE_CENTERED }, \
    { JOYSTICK_2_X_MATRIX_ID, ANALOG_TYPE_CENTERED }, \
    { JOYSTICK_2_Y_MATRIX_ID, ANALOG_TYPE_CENTERED }

// Joystick Button Configuration
// Defines the GPIO port and pin for the joystick push button.
// This pin will be configured as Input with Pull-up.
#define JOYSTICK_BUTTON_GPIO_PORT GPIOA      // Example: GPIOA
#define JOYSTICK_BUTTON_GPIO_PIN  GPIO_PINS_0 // Example: Pin 0

// Joystick Parameters
#define JOYSTICK_DEADZONE_PERCENT 5
#define JOYSTICK_OUTPUT_MIN -127
#define JOYSTICK_OUTPUT_MAX 127

// Joystick Button Keycode
// The keycode to register when the button is pressed.
// Example: MS_BTN1 (Left Mouse), KC_E (Key E), etc.
#define JOYSTICK_BUTTON_KEYCODE MS_BTN1
