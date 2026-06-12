#pragma once

#include "common.h"

#if defined(__has_include)
#if __has_include("tusb.h")
#include "tusb.h"
#include "usb_runtime.h"
#define USB_BOOTSTRAP_CAN_PUMP 1
#endif
#endif

static inline void usb_bootstrap_pump(void) {
#if defined(USB_BOOTSTRAP_CAN_PUMP)
  tud_task();
  usb_runtime_task();
#endif
}
