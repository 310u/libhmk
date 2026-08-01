#include "diagnostic_mode.h"

#include "hardware/hardware.h"
#include "hid.h"
#include "layout.h"
#include "matrix.h"

#define DIAGNOSTIC_MODE_TIMEOUT_MS 30000u

static bool diagnostic_mode_active;
static uint32_t diagnostic_mode_last_activity;

void diagnostic_mode_init(void) {
#if defined(HMK_DIAG_CHANNEL_IDENTITY) && !defined(HMK_DIAG_RUNTIME)
  diagnostic_mode_active = true;
#else
  diagnostic_mode_active = false;
#endif
  diagnostic_mode_last_activity = timer_read();
}

bool diagnostic_mode_is_active(void) { return diagnostic_mode_active; }

void diagnostic_mode_set_active(bool active) {
  if (diagnostic_mode_active == active) {
    diagnostic_mode_last_activity = timer_read();
    return;
  }

  layout_reset_runtime_state();
  hid_send_reports();
  if (!active)
    matrix_recalibrate(false);
  diagnostic_mode_active = active;
  diagnostic_mode_last_activity = timer_read();
}

void diagnostic_mode_touch(void) {
  if (diagnostic_mode_active)
    diagnostic_mode_last_activity = timer_read();
}

void diagnostic_mode_task(void) {
  if (diagnostic_mode_active &&
      timer_read() - diagnostic_mode_last_activity >=
          DIAGNOSTIC_MODE_TIMEOUT_MS)
    diagnostic_mode_set_active(false);
}
