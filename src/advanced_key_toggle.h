#pragma once

#include "advanced_keys.h"

void advanced_key_toggle_process(const advanced_key_event_t *event,
                                 advanced_key_state_t *states);
void advanced_key_toggle_tick(const advanced_key_t *ak,
                              ak_state_toggle_t *state);
