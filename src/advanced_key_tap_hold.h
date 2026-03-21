#pragma once

#include "advanced_keys.h"

void advanced_key_tap_hold_clear(void);
void advanced_key_tap_hold_process(const advanced_key_event_t *event,
                                   advanced_key_state_t *states);
void advanced_key_tap_hold_tick(const advanced_key_t *ak, uint8_t ak_index,
                                ak_state_tap_hold_t *state,
                                bool has_non_tap_hold_press,
                                bool has_non_tap_hold_release);
void advanced_key_tap_hold_update_last_key_time(uint32_t time);
bool advanced_key_tap_hold_has_undecided(const advanced_key_state_t *states);
