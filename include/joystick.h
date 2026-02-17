/*
 * joystick.h - Xbox Analog Joystick Module
 */

#pragma once

#include "common.h"

//--------------------------------------------------------------------+
// Joystick API
//--------------------------------------------------------------------+

/**
 * @brief Initialize the joystick module
 *
 * Configures the button GPIO pin. ADC initialization is handled by analog_init().
 */
void joystick_init(void);

/**
 * @brief Joystick task
 *
 * Reads ADC values, performs normalization (center calibration, deadzone, scaling),
 * and reads the button state. Should be called periodically in the main loop.
 */
void joystick_task(void);

/**
 * @brief Get the processed Joystick X coordinate
 *
 * @return int8_t X coordinate (-127 to 127)
 */
int8_t joystick_get_x(void);

/**
 * @brief Get the processed Joystick Y coordinate
 *
 * @return int8_t Y coordinate (-127 to 127)
 */
int8_t joystick_get_y(void);

/**
 * @brief Get the processed Joystick 2 X coordinate
 *
 * @return int8_t X coordinate (-127 to 127)
 */
int8_t joystick_2_get_x(void);

/**
 * @brief Get the processed Joystick 2 Y coordinate
 *
 * @return int8_t Y coordinate (-127 to 127)
 */
int8_t joystick_2_get_y(void);

/**
 * @brief Get the Joystick Button state
 *
 * @return bool true if pressed, false otherwise
 */
bool joystick_get_button(void);
