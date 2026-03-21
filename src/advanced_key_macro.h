#pragma once

#include "advanced_keys.h"

void advanced_key_macro_abort_all(advanced_key_state_t *states);
void advanced_key_macro_process(const advanced_key_event_t *event,
                                advanced_key_state_t *states);
void advanced_key_macro_tick(const advanced_key_t *ak, ak_state_macro_t *state);
