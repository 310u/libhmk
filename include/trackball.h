#pragma once

#include "common.h"

void trackball_init(void);
void trackball_task(void);

// トラックボールの状態取得用（Diagnostics向け）
typedef struct {
  bool enabled;
  uint16_t current_cpi;
  int16_t last_dx;
  int16_t last_dy;
} trackball_diagnostic_state_t;

void trackball_get_state(trackball_diagnostic_state_t *state);

// CPI トグル/インクリメント API
void trackball_increase_cpi(void);
void trackball_decrease_cpi(void);
