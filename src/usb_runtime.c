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

#include "usb_runtime.h"

#include "hardware/timer_api.h"
#include "hid.h"
#include "tusb.h"
#include "xinput.h"

#define USB_RESUME_RECOVERY_THRESHOLD_MS 5000u
#define USB_RESUME_RECOVERY_DISCONNECT_MS 20u

typedef struct {
  bool suspend_observed;
  bool reconnect_pending;
  bool disconnected;
  uint32_t suspend_start_ms;
  uint32_t disconnect_start_ms;
} usb_runtime_state_t;

static usb_runtime_state_t usb_runtime_state;

static void usb_runtime_resync(void) {
  hid_clear_runtime_state();
  xinput_reset_runtime_state();
}

void usb_runtime_init(void) {
  usb_runtime_state.suspend_observed = false;
  usb_runtime_state.reconnect_pending = false;
  usb_runtime_state.disconnected = false;
}

void usb_runtime_task(void) {
  if (usb_runtime_state.reconnect_pending) {
    usb_runtime_state.reconnect_pending = false;
    usb_runtime_state.disconnected = true;
    usb_runtime_state.disconnect_start_ms = timer_read();

    usb_runtime_resync();
    (void)tud_disconnect();
    return;
  }

  if (!usb_runtime_state.disconnected) {
    return;
  }

  if (timer_elapsed(usb_runtime_state.disconnect_start_ms) <
      USB_RESUME_RECOVERY_DISCONNECT_MS) {
    return;
  }

  usb_runtime_state.disconnected = false;
  (void)tud_connect();
}

void usb_runtime_mount(void) {
  usb_runtime_init();
  usb_runtime_resync();
}

void usb_runtime_suspend(void) {
  usb_runtime_state.suspend_observed = true;
  usb_runtime_state.suspend_start_ms = timer_read();
}

void usb_runtime_resume(void) {
  if (usb_runtime_state.suspend_observed &&
      timer_elapsed(usb_runtime_state.suspend_start_ms) >=
          USB_RESUME_RECOVERY_THRESHOLD_MS) {
    usb_runtime_state.reconnect_pending = true;
  }

  usb_runtime_state.suspend_observed = false;
  usb_runtime_resync();
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

void tud_mount_cb(void) { usb_runtime_mount(); }

void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;

  // Avoid controller-specific link changes inside the suspend callback itself.
  // We only record timing here and defer any recovery action until resume/task.
  usb_runtime_suspend();
}

void tud_resume_cb(void) { usb_runtime_resume(); }
