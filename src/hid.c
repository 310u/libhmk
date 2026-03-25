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

#include "hid.h"

#include "commands.h"
#include "event_trace.h"
#include "hardware/hardware.h"
#include "keycodes.h"
#include "matrix.h"
#include "tusb.h"
#include "usb_descriptors.h"

// Track how many keys are currently in the 6KRO part of the report
static uint8_t num_6kro_keys;
static hid_nkro_kb_report_t kb_report;
static hid_nkro_kb_report_t kb_report_last_sent;

#define MAX_PENDING_KB_REPORTS 16u
static hid_nkro_kb_report_t kb_report_queue[MAX_PENDING_KB_REPORTS];
static uint8_t kb_report_queue_head;
static uint8_t kb_report_queue_size;

static uint16_t system_report;
static uint16_t consumer_report;
static hid_mouse_report_t mouse_report;
static uint8_t mouse_keycode_buttons;
static uint8_t mouse_pointer_buttons;
static int32_t mouse_pending_x;
static int32_t mouse_pending_y;
static int32_t mouse_pending_wheel;
static int32_t mouse_pending_pan;
static uint16_t system_report_last_sent;
static uint16_t consumer_report_last_sent;
static uint8_t mouse_buttons_last_sent;

static void hid_mouse_sync_buttons(void) {
  mouse_report.buttons = mouse_keycode_buttons | mouse_pointer_buttons;
}

static int8_t hid_mouse_clamp_pending(int32_t value) {
  if (value > INT8_MAX)
    return INT8_MAX;
  if (value < INT8_MIN)
    return INT8_MIN;
  return (int8_t)value;
}

static void hid_keyboard_queue_report(void) {
  const hid_nkro_kb_report_t *baseline = &kb_report_last_sent;
  if (kb_report_queue_size != 0u) {
    const uint32_t tail_index = ((uint32_t)kb_report_queue_head +
                                 (uint32_t)kb_report_queue_size - 1u) &
                                (uint32_t)(MAX_PENDING_KB_REPORTS - 1u);
    const uint8_t tail =
        (uint8_t)tail_index;
    baseline = &kb_report_queue[tail];
  }

  if (memcmp(baseline, &kb_report, sizeof(kb_report)) == 0)
    return;

  if (kb_report_queue_size == MAX_PENDING_KB_REPORTS) {
    // If the queue overflows, drop the oldest unsent snapshot and keep the
    // newest transitions flowing.
    kb_report_queue_head =
        (kb_report_queue_head + 1u) & (MAX_PENDING_KB_REPORTS - 1u);
    kb_report_queue_size--;
  }

  const uint32_t tail_index =
      ((uint32_t)kb_report_queue_head + (uint32_t)kb_report_queue_size) &
      (uint32_t)(MAX_PENDING_KB_REPORTS - 1u);
  const uint8_t tail = (uint8_t)tail_index;
  kb_report_queue[tail] = kb_report;
  kb_report_queue_size++;
}

#if !defined(HID_DISABLED)
/**
 * @brief Send the keyboard report
 *
 * This function will send the keyboard report to its exclusive interface.
 *
 * @return None
 */
static void hid_send_keyboard_report(void) {
  if (kb_report_queue_size == 0u) {
    hid_keyboard_queue_report();
  }

  if (kb_report_queue_size == 0u)
    return;

  hid_nkro_kb_report_t *report = &kb_report_queue[kb_report_queue_head];

  if (tud_hid_n_report(USB_ITF_KEYBOARD, 0, report, sizeof(*report))) {
    EVENT_TRACE(
        "[event] hid send keyboard modifiers=0x%02x keys=[%u,%u,%u,%u,%u,%u] "
        "queued=%u\n",
        report->modifiers, report->keycodes[0], report->keycodes[1],
        report->keycodes[2], report->keycodes[3], report->keycodes[4],
        report->keycodes[5], kb_report_queue_size);
    kb_report_last_sent = *report;
    kb_report_queue_head =
        (kb_report_queue_head + 1u) & (MAX_PENDING_KB_REPORTS - 1u);
    kb_report_queue_size--;
  }
}
#endif

/**
 * @brief Find the next available report and send it
 *
 * @param starting_report_id The report ID to start searching from
 *
 * @return None
 */
static void hid_send_hid_report(uint8_t starting_report_id) {
  for (uint8_t report_id = starting_report_id; report_id < REPORT_ID_COUNT;
       report_id++) {
    switch (report_id) {
    case REPORT_ID_SYSTEM_CONTROL:
      if (system_report == system_report_last_sent)
        // Don't send the report if it hasn't changed
        break;
      if (tud_hid_n_report(USB_ITF_HID, report_id, &system_report,
                           sizeof(system_report))) {
        EVENT_TRACE("[event] hid send system value=0x%04x\n", system_report);
        system_report_last_sent = system_report;
      }
      return;

    case REPORT_ID_CONSUMER_CONTROL:
      if (consumer_report == consumer_report_last_sent)
        // Don't send the report if it hasn't changed
        break;
      if (tud_hid_n_report(USB_ITF_HID, report_id, &consumer_report,
                           sizeof(consumer_report))) {
        EVENT_TRACE("[event] hid send consumer value=0x%04x\n",
                    consumer_report);
        consumer_report_last_sent = consumer_report;
      }
      return;

    case REPORT_ID_MOUSE:
      if (mouse_report.buttons == mouse_buttons_last_sent && mouse_pending_x == 0 &&
          mouse_pending_y == 0 && mouse_pending_wheel == 0 &&
          mouse_pending_pan == 0)
        // Nothing changed since the last mouse report.
        break;

      hid_mouse_report_t next_mouse_report = {
          .buttons = mouse_report.buttons,
          .x = hid_mouse_clamp_pending(mouse_pending_x),
          .y = hid_mouse_clamp_pending(mouse_pending_y),
          .wheel = hid_mouse_clamp_pending(mouse_pending_wheel),
          .pan = hid_mouse_clamp_pending(mouse_pending_pan),
      };

      if (tud_hid_n_report(USB_ITF_HID, report_id, &next_mouse_report,
                           sizeof(next_mouse_report))) {
        EVENT_TRACE(
            "[event] hid send mouse buttons=0x%02x x=%d y=%d wheel=%d pan=%d\n",
            next_mouse_report.buttons, next_mouse_report.x, next_mouse_report.y,
            next_mouse_report.wheel, next_mouse_report.pan);
        mouse_buttons_last_sent = next_mouse_report.buttons;
        mouse_pending_x -= next_mouse_report.x;
        mouse_pending_y -= next_mouse_report.y;
        mouse_pending_wheel -= next_mouse_report.wheel;
        mouse_pending_pan -= next_mouse_report.pan;
      }
      return;

    default:
      break;
    }
  }
}

void hid_init(void) {
  num_6kro_keys = 0;
  memset(&kb_report, 0, sizeof(kb_report));
  memset(&kb_report_last_sent, 0, sizeof(kb_report_last_sent));
  kb_report_queue_head = 0;
  kb_report_queue_size = 0;
  system_report = 0;
  consumer_report = 0;
  memset(&mouse_report, 0, sizeof(mouse_report));
  mouse_keycode_buttons = 0;
  mouse_pointer_buttons = 0;
  mouse_pending_x = 0;
  mouse_pending_y = 0;
  mouse_pending_wheel = 0;
  mouse_pending_pan = 0;
  system_report_last_sent = 0;
  consumer_report_last_sent = 0;
  mouse_buttons_last_sent = 0;
}

void hid_clear_runtime_state(void) {
  num_6kro_keys = 0;
  memset(&kb_report, 0, sizeof(kb_report));
  kb_report_queue_head = 0;
  kb_report_queue_size = 0;
  hid_keyboard_queue_report();

  system_report = 0;
  consumer_report = 0;

  memset(&mouse_report, 0, sizeof(mouse_report));
  mouse_keycode_buttons = 0;
  mouse_pointer_buttons = 0;
  mouse_pending_x = 0;
  mouse_pending_y = 0;
  mouse_pending_wheel = 0;
  mouse_pending_pan = 0;
  hid_mouse_sync_buttons();
}

void hid_keycode_add(uint8_t keycode) {
  const uint16_t hid_code = keycode_to_hid[keycode];

  if (!hid_code)
    // No HID code for this keycode
    return;

  bool found = false;
  switch (keycode) {
  case KEYBOARD_KEYCODE_RANGE:
    for (uint32_t i = 0; i < num_6kro_keys; i++) {
      if (kb_report.keycodes[i] == hid_code) {
        found = true;
        break;
      }
    }

    if (!found && num_6kro_keys < 6) {
      // Only add to 6KRO array if there's room. The NKRO bitmap below
      // always tracks all pressed keys regardless of 6KRO capacity.
      kb_report.keycodes[num_6kro_keys++] = hid_code;
    }
    kb_report.bitmap[hid_code / 8] |= 1 << (hid_code & 7);
    hid_keyboard_queue_report();
    break;

  case MODIFIER_KEYCODE_RANGE:
    kb_report.modifiers |= hid_code;
    hid_keyboard_queue_report();
    break;

  case SYSTEM_KEYCODE_RANGE:
    system_report = hid_code;
    break;

  case CONSUMER_KEYCODE_RANGE:
    consumer_report = hid_code;
    break;

  case MOUSE_KEYCODE_RANGE:
    mouse_keycode_buttons |= hid_code;
    hid_mouse_sync_buttons();
    break;

  default:
    break;
  }
}

void hid_mouse_move(int8_t x, int8_t y, uint8_t buttons) {
  mouse_pending_x += x;
  mouse_pending_y += y;
  mouse_pointer_buttons = buttons;
  hid_mouse_sync_buttons();
}

void hid_mouse_scroll(int8_t wheel, int8_t pan, uint8_t buttons) {
  mouse_pending_wheel += wheel;
  mouse_pending_pan += pan;
  mouse_pointer_buttons = buttons;
  hid_mouse_sync_buttons();
}

void hid_keycode_remove(uint8_t keycode) {
  const uint16_t hid_code = keycode_to_hid[keycode];

  if (!hid_code)
    // No HID code for this keycode
    return;

  switch (keycode) {
  case KEYBOARD_KEYCODE_RANGE:
    for (uint32_t i = 0; i < num_6kro_keys; i++) {
      if (kb_report.keycodes[i] == hid_code) {
        for (uint32_t j = i; j < 5; j++)
          kb_report.keycodes[j] = kb_report.keycodes[j + 1];
        num_6kro_keys--;
        kb_report.keycodes[num_6kro_keys] = 0;
        break;
      }
    }
    kb_report.bitmap[hid_code / 8] &= ~(1 << (hid_code & 7));
    hid_keyboard_queue_report();
    break;

  case MODIFIER_KEYCODE_RANGE:
    kb_report.modifiers &= ~hid_code;
    hid_keyboard_queue_report();
    break;

  case SYSTEM_KEYCODE_RANGE:
    if (system_report == hid_code)
      // Only remove the system report if it matches the one we're trying to
      system_report = 0;
    break;

  case CONSUMER_KEYCODE_RANGE:
    if (consumer_report == hid_code)
      // Only remove the consumer report if it matches the one we're trying to
      consumer_report = 0;
    break;

  case MOUSE_KEYCODE_RANGE:
    mouse_keycode_buttons &= ~hid_code;
    hid_mouse_sync_buttons();
    break;

  default:
    break;
  }
}

void hid_send_reports(void) {
#if !defined(HID_DISABLED)
  if (tud_suspended()) {
    // Wake up the host if it's suspended
    tud_remote_wakeup();
    if (tud_suspended())
      return;
  }

  if (tud_hid_n_ready(USB_ITF_KEYBOARD))
    hid_send_keyboard_report();

  if (tud_hid_n_ready(USB_ITF_HID))
    // Start from the first report ID
    hid_send_hid_report(REPORT_ID_SYSTEM_CONTROL);
#endif
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
  return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, const uint8_t *buffer,
                           uint16_t bufsize) {
  if (instance == USB_ITF_RAW_HID)
    command_process(buffer);
}

void tud_hid_report_complete_cb(uint8_t instance, const uint8_t *report,
                                uint16_t len) {
  if (instance == USB_ITF_KEYBOARD)
    hid_send_keyboard_report();
  else if (instance == USB_ITF_HID)
    // Start from the next report ID
    hid_send_hid_report(report[0] + 1);
}
