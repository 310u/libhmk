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
    static uint32_t loop_count = 0;

    if ((loop_count & 1) == 0) {
      tud_task();
      usb_runtime_task();
    }

    analog_task();
    matrix_task();

#if defined(HMK_DIAG_CHANNEL_IDENTITY)
    matrix_scan_housekeeping();
#if HMK_ENABLE_COMMAND_TASK
    command_task();
#endif
#else
    matrix_scan_housekeeping();

#if HMK_STAGGER_BACKGROUND_TASKS
    const uint32_t task_phase = loop_count & 7u;

    if (task_phase == 0u) {
#if HMK_ENABLE_LAYOUT_TASK
      layout_task();
#endif
    } else if (task_phase == 1u) {
#if HMK_ENABLE_XINPUT_TASK
      xinput_task();
#endif
    } else if (task_phase == 2u) {
#if HMK_ENABLE_TRACKBALL_TASK
      trackball_task();
#endif
    } else if (task_phase == 3u) {
#if defined(JOYSTICK_ENABLED) && HMK_ENABLE_JOYSTICK_TASK
      joystick_task();
#endif
    } else if (task_phase == 4u) {
#if HMK_ENABLE_ENCODER_TASK
      encoder_task();
#endif
    } else if (task_phase == 5u) {
#if HMK_ENABLE_SLIDER_TASK
      slider_task();
#endif
    } else if (task_phase == 6u) {
#if defined(RGB_ENABLED)
#if HMK_ENABLE_RGB_TASK
      rgb_task();
#endif
#endif
    } else {
#if HMK_ENABLE_COMMAND_TASK
      command_task();
#endif
    }
#else
#if HMK_ENABLE_LAYOUT_TASK
    if (main_task_due(loop_count, HMK_LAYOUT_TASK_INTERVAL,
          HMK_LAYOUT_TASK_PHASE)) {
  layout_task();
    }
#endif
#if HMK_ENABLE_XINPUT_TASK
    if (main_task_due(loop_count, HMK_XINPUT_TASK_INTERVAL,
          HMK_XINPUT_TASK_PHASE)) {
  xinput_task();
    }
#endif
#if HMK_ENABLE_TRACKBALL_TASK
    if (main_task_due(loop_count, HMK_TRACKBALL_TASK_INTERVAL,
          HMK_TRACKBALL_TASK_PHASE)) {
  trackball_task();
    }
#endif
#if defined(JOYSTICK_ENABLED) && HMK_ENABLE_JOYSTICK_TASK
    if (main_task_due(loop_count, HMK_JOYSTICK_TASK_INTERVAL,
          HMK_JOYSTICK_TASK_PHASE)) {
  joystick_task();
    }
#endif
#if HMK_ENABLE_ENCODER_TASK
    if (main_task_due(loop_count, HMK_ENCODER_TASK_INTERVAL,
          HMK_ENCODER_TASK_PHASE)) {
  encoder_task();
    }
#endif
#if HMK_ENABLE_SLIDER_TASK
    if (main_task_due(loop_count, HMK_SLIDER_TASK_INTERVAL,
          HMK_SLIDER_TASK_PHASE)) {
  slider_task();
    }
#endif
#if defined(RGB_ENABLED)
#if HMK_ENABLE_RGB_TASK
    if (main_task_due(loop_count, HMK_RGB_TASK_INTERVAL,
          HMK_RGB_TASK_PHASE)) {
  rgb_task();
    }
#endif
#endif
#if HMK_ENABLE_COMMAND_TASK
    if (main_task_due(loop_count, HMK_COMMAND_TASK_INTERVAL,
          HMK_COMMAND_TASK_PHASE)) {
  command_task();
    }
#endif
#endif
    loop_count++;
#endif
#if defined(__arm__)
    __asm__ volatile ("wfi");
#endif
  }

  return 0;
#endif
}
