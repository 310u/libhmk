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

#include "advanced_keys.h"
#include "commands.h"
#include "crc32.h"
#include "deferred_actions.h"
#include "eeconfig.h"
#include "encoder.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "joystick.h"
#include "layout.h"
#include "matrix.h"
#include "rgb.h"
#include "tusb.h"
#include "usb_runtime.h"
#include "wear_leveling.h"
#include "xinput.h"
#include "slider.h"
#include "trackball.h"

static void main_apply_analog_scan_runtime_config(void) {
  if (!analog_set_mux_sample_delay_us(eeconfig->mux_sample_delay_us)) {
    (void)analog_set_mux_sample_delay_us(ADC_SAMPLE_DELAY_DEFAULT);
  }
}

int main(void) {
#if defined(USB_ENUM_DIAGNOSTIC)
  board_init();
  timer_init();
  tud_init(BOARD_TUD_RHPORT);

  while (1) {
    tud_task();
#if defined(__arm__)
    __asm__ volatile ("wfi");
#endif
  }

  return 0;
#elif defined(USB_BOOT_DIAGNOSTIC_EECONFIG)
  board_init();
  timer_init();
  usb_runtime_init();
  crc32_init();
  flash_init();
  wear_leveling_init();
  eeconfig_init();
  tud_init(BOARD_TUD_RHPORT);

  while (1) {
    tud_task();
    usb_runtime_task();
#if defined(__arm__)
    __asm__ volatile ("wfi");
#endif
  }

  return 0;
#else
  // Initialize the hardware
  board_init();
  timer_init();
  usb_runtime_init();
  crc32_init();
  flash_init();

  // Initialize the persistent configuration
  wear_leveling_init();
  eeconfig_init();
#if defined(RECOVERY_RESET_CURRENT_PROFILE_RGB) && defined(RGB_ENABLED)
  (void)eeconfig_reset_profile_rgb(eeconfig->current_profile);
#endif

  // Start USB before optional peripherals so the device can still enumerate
  // even if a later hardware block fails to initialize.
  tud_init(BOARD_TUD_RHPORT);

  // Initialize the core modules
  analog_init();
  main_apply_analog_scan_runtime_config();
  matrix_init();
#if defined(RGB_ENABLED) && !defined(HMK_DIAG_CHANNEL_IDENTITY)
  rgb_init();
#endif
  hid_init();
  deferred_action_init();
  advanced_key_init();
  xinput_init();
  layout_init();
  encoder_init();
#if defined(JOYSTICK_ENABLED) && !defined(HMK_DIAG_CHANNEL_IDENTITY)
  joystick_init();
#endif
#if !defined(HMK_DIAG_CHANNEL_IDENTITY)
  trackball_init();
#endif
  slider_init();
  command_init();

  while (1) {
    tud_task();
    usb_runtime_task();

    analog_task();
    matrix_task();

#if defined(HMK_DIAG_CHANNEL_IDENTITY)
    matrix_scan_housekeeping();
    command_task();
#else
    matrix_scan_housekeeping();

    static uint32_t loop_count = 0;
    if ((loop_count & 15) == 0) {
      layout_task();
      xinput_task();
      trackball_task();
#if defined(JOYSTICK_ENABLED)
      joystick_task();
#endif
      encoder_task();
      slider_task();
#if defined(RGB_ENABLED)
      rgb_task();
#endif
      command_task();
    }
    loop_count++;
#endif
#if defined(__arm__)
    __asm__ volatile ("wfi");
#endif
  }

  return 0;
#endif
}
