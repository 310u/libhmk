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
#include "diagnostic_mode.h"
#include "eeconfig.h"
#include "encoder.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "joystick.h"
#include "layout.h"
#include "matrix.h"
#include "micro_scheduler.h"
#include "rgb.h"
#include "tusb.h"
#include "usb_runtime.h"
#include "wear_leveling.h"
#include "xinput.h"
#include "slider.h"
#include "trackball.h"

// HMK_ENABLE_* flags are defined in micro_scheduler.h
// They can be overridden to 0 before that header is included.

#ifndef HMK_USB_TASK_INTERVAL
// TinyUSB task polling cadence. Values > 2 reduce USB interrupt overhead and can
// improve high-rate matrix scheduling at the cost of slightly higher command
// latency. For low-rate builds, keep the legacy every-other-loop default.
#define HMK_USB_TASK_INTERVAL 2u
#endif

#ifndef HMK_USB_TASK_PHASE
#define HMK_USB_TASK_PHASE 0u
#endif

#ifndef HMK_STAGGER_BACKGROUND_TASKS
// Staggering every loop can starve matrix throughput under heavy host traffic.
// Keep legacy burst mode as the safe default; enable staggering explicitly for
// controlled experiments.
#define HMK_STAGGER_BACKGROUND_TASKS 0
#endif

#ifndef HMK_LAYOUT_TASK_INTERVAL
#define HMK_LAYOUT_TASK_INTERVAL 8u
#endif

#ifndef HMK_LAYOUT_TASK_PHASE
#define HMK_LAYOUT_TASK_PHASE 0u
#endif

#ifndef HMK_XINPUT_TASK_INTERVAL
#define HMK_XINPUT_TASK_INTERVAL 8u
#endif

#ifndef HMK_XINPUT_TASK_PHASE
#define HMK_XINPUT_TASK_PHASE 0u
#endif

#ifndef HMK_TRACKBALL_TASK_INTERVAL
#define HMK_TRACKBALL_TASK_INTERVAL 8u
#endif

#ifndef HMK_TRACKBALL_TASK_PHASE
#define HMK_TRACKBALL_TASK_PHASE 0u
#endif

#ifndef HMK_JOYSTICK_TASK_INTERVAL
#define HMK_JOYSTICK_TASK_INTERVAL 8u
#endif

#ifndef HMK_JOYSTICK_TASK_PHASE
#define HMK_JOYSTICK_TASK_PHASE 0u
#endif

#ifndef HMK_ENCODER_TASK_INTERVAL
#define HMK_ENCODER_TASK_INTERVAL 8u
#endif

#ifndef HMK_ENCODER_TASK_PHASE
#define HMK_ENCODER_TASK_PHASE 0u
#endif

#ifndef HMK_SLIDER_TASK_INTERVAL
#define HMK_SLIDER_TASK_INTERVAL 8u
#endif

#ifndef HMK_SLIDER_TASK_PHASE
#define HMK_SLIDER_TASK_PHASE 0u
#endif

#ifndef HMK_RGB_TASK_INTERVAL
#define HMK_RGB_TASK_INTERVAL 8u
#endif

#ifndef HMK_RGB_TASK_PHASE
#define HMK_RGB_TASK_PHASE 0u
#endif

#ifndef HMK_COMMAND_TASK_INTERVAL
#define HMK_COMMAND_TASK_INTERVAL 8u
#endif

#ifndef HMK_COMMAND_TASK_PHASE
#define HMK_COMMAND_TASK_PHASE 0u
#endif

static inline bool main_task_due(uint32_t loop_count, uint32_t interval,
                                 uint32_t phase) {
  if (interval <= 1u) {
    return true;
  }

  return (loop_count % interval) == (phase % interval);
}

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
#if defined(RGB_ENABLED)
  rgb_init();
#endif
  hid_init();
  deferred_action_init();
  advanced_key_init();
  xinput_init();
  layout_init();
  encoder_init();
#if defined(JOYSTICK_ENABLED)
  joystick_init();
#endif
  trackball_init();
  slider_init();
  command_init();
  diagnostic_mode_init();
  micro_scheduler_init();

  while (1) {
    static uint32_t loop_count = 0;

    if (main_task_due(loop_count, HMK_USB_TASK_INTERVAL, HMK_USB_TASK_PHASE)) {
      tud_task();
      usb_runtime_task();
    }

    analog_task();
    matrix_task();

    diagnostic_mode_task();
    if (diagnostic_mode_is_active()) {
      matrix_scan_housekeeping();
#if HMK_ENABLE_COMMAND_TASK
      command_task();
#endif
    } else {
      matrix_scan_housekeeping();
      micro_scheduler_run();
    }
  }

  return 0;
#endif
}
