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

#include "micro_scheduler.h"

#include "advanced_keys.h"
#include "commands.h"
#include "encoder.h"
#include "hardware/hardware.h"
#include "joystick.h"
#include "layout.h"
#include "rgb.h"
#include "slider.h"
#include "trackball.h"
#include "tusb.h"
#include "usb_runtime.h"
#include "xinput.h"

#ifndef HMK_USB_TASK_INTERVAL_US
#define HMK_USB_TASK_INTERVAL_US 125u
#endif

#ifndef HMK_LAYOUT_TASK_INTERVAL_US
#define HMK_LAYOUT_TASK_INTERVAL_US 1000u
#endif

#ifndef HMK_XINPUT_TASK_INTERVAL_US
#define HMK_XINPUT_TASK_INTERVAL_US 1000u
#endif

#ifndef HMK_TRACKBALL_TASK_INTERVAL_US
#define HMK_TRACKBALL_TASK_INTERVAL_US 1000u
#endif

#ifndef HMK_JOYSTICK_TASK_INTERVAL_US
#define HMK_JOYSTICK_TASK_INTERVAL_US 1000u
#endif

#ifndef HMK_ENCODER_TASK_INTERVAL_US
#define HMK_ENCODER_TASK_INTERVAL_US 1000u
#endif

#ifndef HMK_SLIDER_TASK_INTERVAL_US
#define HMK_SLIDER_TASK_INTERVAL_US 1000u
#endif

#ifndef HMK_RGB_TASK_INTERVAL_US
#define HMK_RGB_TASK_INTERVAL_US 16666u
#endif

#ifndef HMK_COMMAND_TASK_INTERVAL_US
#define HMK_COMMAND_TASK_INTERVAL_US 2000u
#endif

typedef void (*task_fn_t)(void);

typedef struct {
  task_fn_t fn;
  uint32_t interval_us;
  uint32_t last_run_us;
  bool enabled;
} micro_task_entry_t;

static void usb_combined_task(void) {
  tud_task();
  usb_runtime_task();
}

static micro_task_entry_t tasks[] = {
    {.fn = usb_combined_task, .interval_us = HMK_USB_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#if HMK_ENABLE_LAYOUT_TASK
    {.fn = layout_task, .interval_us = HMK_LAYOUT_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#endif
#if HMK_ENABLE_XINPUT_TASK
    {.fn = xinput_task, .interval_us = HMK_XINPUT_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#endif
#if defined(TRACKBALL_ENABLED) && HMK_ENABLE_TRACKBALL_TASK
    {.fn = trackball_task, .interval_us = HMK_TRACKBALL_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#endif
#if defined(JOYSTICK_ENABLED) && HMK_ENABLE_JOYSTICK_TASK
    {.fn = joystick_task, .interval_us = HMK_JOYSTICK_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#endif
#if HMK_ENABLE_ENCODER_TASK
    {.fn = encoder_task, .interval_us = HMK_ENCODER_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#endif
#if HMK_ENABLE_SLIDER_TASK
    {.fn = slider_task, .interval_us = HMK_SLIDER_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#endif
#if defined(RGB_ENABLED) && HMK_ENABLE_RGB_TASK
    {.fn = rgb_task, .interval_us = HMK_RGB_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#endif
#if HMK_ENABLE_COMMAND_TASK
    {.fn = command_task, .interval_us = HMK_COMMAND_TASK_INTERVAL_US, .last_run_us = 0, .enabled = true},
#endif
};

#define TASK_COUNT (sizeof(tasks) / sizeof(tasks[0]))

static uint32_t current_task_idx = 0;

void micro_scheduler_init(void) {
  uint32_t now = timer_read_us();
  for (size_t i = 0; i < TASK_COUNT; i++) {
    tasks[i].last_run_us = now;
  }
  current_task_idx = 0;
}

void micro_scheduler_run(void) {
  uint32_t start_us = timer_read_us();

  for (size_t i = 0; i < TASK_COUNT; i++) {
    uint32_t idx = (current_task_idx + i) % TASK_COUNT;
    micro_task_entry_t *t = &tasks[idx];

    if (!t->enabled || t->fn == NULL) {
      continue;
    }

    uint32_t now = timer_read_us();
    if ((now - t->last_run_us) >= t->interval_us) {
      t->fn();
      t->last_run_us = timer_read_us();

      // Check if budget is exhausted
      if (timer_elapsed_us(start_us) >= MICRO_SCHEDULER_MAX_BUDGET_US) {
        current_task_idx = (idx + 1) % TASK_COUNT;
        break;
      }
    }
  }
}
