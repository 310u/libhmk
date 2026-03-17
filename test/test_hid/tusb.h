#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  HID_REPORT_TYPE_INVALID = 0,
  HID_REPORT_TYPE_INPUT,
  HID_REPORT_TYPE_OUTPUT,
  HID_REPORT_TYPE_FEATURE,
} hid_report_type_t;

typedef struct __attribute__((packed)) {
  uint8_t buttons;
  int8_t x;
  int8_t y;
  int8_t wheel;
  int8_t pan;
} hid_mouse_report_t;

bool tud_hid_n_report(uint8_t instance, uint8_t report_id, const void *report,
                      uint16_t len);
bool tud_hid_n_ready(uint8_t instance);
bool tud_suspended(void);
void tud_remote_wakeup(void);
void tud_task(void);
