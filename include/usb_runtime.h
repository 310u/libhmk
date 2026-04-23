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
// USB Runtime Recovery API
//--------------------------------------------------------------------+

/**
 * @brief Initialize USB runtime recovery state
 *
 * @return None
 */
void usb_runtime_init(void);

/**
 * @brief Run deferred USB runtime recovery work
 *
 * @return None
 */
void usb_runtime_task(void);

/**
 * @brief Synchronize runtime USB-facing state after mount
 *
 * @return None
 */
void usb_runtime_mount(void);

/**
 * @brief Record that the USB bus entered suspend
 *
 * @return None
 */
void usb_runtime_suspend(void);

/**
 * @brief Synchronize runtime USB-facing state after resume
 *
 * @return None
 */
void usb_runtime_resume(void);

/**
 * @brief Return whether USB suspend handling is currently active
 *
 * This stays true while the host is suspended so other subsystems can
 * temporarily quiesce user-facing behavior such as LEDs.
 *
 * @return true when the device should behave as suspended
 */
bool usb_runtime_is_suspended(void);
