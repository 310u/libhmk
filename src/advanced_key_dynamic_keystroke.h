#pragma once

#include "advanced_keys.h"

void advanced_key_dynamic_keystroke_clear(void);
void advanced_key_dynamic_keystroke_process(const advanced_key_event_t *event,
                                            advanced_key_state_t *states);
