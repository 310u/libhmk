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
