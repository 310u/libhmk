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

#include "common.h"

//--------------------------------------------------------------------+
// Rotary Encoder Configuration
//--------------------------------------------------------------------+

#if !defined(ENCODER_NUM)
#define ENCODER_NUM 0
#endif

#if ENCODER_NUM == 0 &&                                                     \
    (defined(ENCODER_CW_KEYS) || defined(ENCODER_CCW_KEYS) ||               \
     defined(ENCODER_CW_KEYCODES) || defined(ENCODER_CCW_KEYCODES))
#error "ENCODER_NUM must be greater than 0 when encoder outputs are defined"
#endif

#if ENCODER_NUM > 0
#if !defined(ENCODER_A_PORTS)
#error "ENCODER_A_PORTS is not defined"
#endif

#if !defined(ENCODER_A_PINS)
#error "ENCODER_A_PINS is not defined"
#endif

#if !defined(ENCODER_B_PORTS)
#error "ENCODER_B_PORTS is not defined"
#endif

#if !defined(ENCODER_B_PINS)
#error "ENCODER_B_PINS is not defined"
#endif

#if (defined(ENCODER_CW_KEYS) && !defined(ENCODER_CCW_KEYS)) ||                  \
    (!defined(ENCODER_CW_KEYS) && defined(ENCODER_CCW_KEYS))
#error "ENCODER_CW_KEYS and ENCODER_CCW_KEYS must both be defined"
#endif

#if (defined(ENCODER_CW_KEYCODES) && !defined(ENCODER_CCW_KEYCODES)) ||          \
    (!defined(ENCODER_CW_KEYCODES) && defined(ENCODER_CCW_KEYCODES))
#error "ENCODER_CW_KEYCODES and ENCODER_CCW_KEYCODES must both be defined"
#endif

#if !defined(ENCODER_CW_KEYS) && !defined(ENCODER_CW_KEYCODES)
#error "ENCODER_CW_KEYS/ENCODER_CCW_KEYS or ENCODER_CW_KEYCODES/ENCODER_CCW_KEYCODES must be defined"
#endif

#if defined(ENCODER_CW_KEYS) && defined(ENCODER_CW_KEYCODES)
#error "Encoder key-index mode and keycode mode are mutually exclusive"
#endif

#if !defined(ENCODER_STEPS_PER_DETENT)
#define ENCODER_STEPS_PER_DETENT 4
#endif

#if !defined(ENCODER_QUEUE_SIZE)
#define ENCODER_QUEUE_SIZE 16
#endif

#if (defined(ENCODER_INPUT_PULLUP) && defined(ENCODER_INPUT_PULLDOWN)) ||       \
    (defined(ENCODER_INPUT_PULLUP) && defined(ENCODER_INPUT_NOPULL)) ||         \
    (defined(ENCODER_INPUT_PULLDOWN) && defined(ENCODER_INPUT_NOPULL))
#error "Encoder pull mode macros are mutually exclusive"
#endif

_Static_assert(ENCODER_STEPS_PER_DETENT > 0,
               "ENCODER_STEPS_PER_DETENT must be greater than 0");
_Static_assert(ENCODER_QUEUE_SIZE > 0,
               "ENCODER_QUEUE_SIZE must be greater than 0");
_Static_assert(ENCODER_QUEUE_SIZE <= UINT8_MAX,
               "ENCODER_QUEUE_SIZE must fit in uint8_t");
#endif

//--------------------------------------------------------------------+
// Rotary Encoder API
//--------------------------------------------------------------------+

void encoder_init(void);
void encoder_task(void);
