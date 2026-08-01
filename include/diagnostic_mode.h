#pragma once

#include "common.h"

void diagnostic_mode_init(void);
bool diagnostic_mode_is_active(void);
void diagnostic_mode_set_active(bool active);
void diagnostic_mode_touch(void);
void diagnostic_mode_task(void);
