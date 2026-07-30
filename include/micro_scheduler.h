/*
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Task enable flags (define to 0 before including to disable a task)
//--------------------------------------------------------------------+

#ifndef HMK_ENABLE_LAYOUT_TASK
#define HMK_ENABLE_LAYOUT_TASK 1
#endif

#ifndef HMK_ENABLE_XINPUT_TASK
#define HMK_ENABLE_XINPUT_TASK 1
#endif

#ifndef HMK_ENABLE_TRACKBALL_TASK
#define HMK_ENABLE_TRACKBALL_TASK 1
#endif

#ifndef HMK_ENABLE_JOYSTICK_TASK
#define HMK_ENABLE_JOYSTICK_TASK 1
#endif

#ifndef HMK_ENABLE_ENCODER_TASK
#define HMK_ENABLE_ENCODER_TASK 1
#endif

#ifndef HMK_ENABLE_SLIDER_TASK
#define HMK_ENABLE_SLIDER_TASK 1
#endif

#ifndef HMK_ENABLE_RGB_TASK
#define HMK_ENABLE_RGB_TASK 1
#endif

#ifndef HMK_ENABLE_COMMAND_TASK
#define HMK_ENABLE_COMMAND_TASK 1
#endif

//--------------------------------------------------------------------+
// Microsecond Cooperative Scheduler API
//--------------------------------------------------------------------+

/**
 * @brief Default task execution budget per main loop run in microseconds.
 * Must be well below the matrix scan window (MATRIX_SCHEDULER_BUDGET_US).
 * For divider=2 (~16kHz, 63us window), keep this at or below 50us.
 */
#ifndef MICRO_SCHEDULER_MAX_BUDGET_US
#define MICRO_SCHEDULER_MAX_BUDGET_US 50u
#endif

/**
 * @brief Initialize the microsecond scheduler.
 */
void micro_scheduler_init(void);

/**
 * @brief Execute due background tasks within the time budget.
 * Should be called once per main loop iteration after matrix_task().
 */
void micro_scheduler_run(void);

#ifdef __cplusplus
}
#endif
