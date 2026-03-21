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

#include "advanced_key_combo.h"

#include "advanced_keys.h"

#include "deferred_actions.h"
#include "eeconfig.h"
#include "hardware/hardware.h"
#include "input_routing.h"
#include "layout.h"

#define COMBO_QUEUE_SIZE 16
#define DEFAULT_COMBO_TERM 50
#define COMBO_KEY_NONE 255

typedef struct {
  uint8_t key;
  bool pressed;
  uint32_t time;
  // Whether the event has been consumed by a combo match.
  bool consumed;
} combo_event_t;

static combo_event_t event_queue[COMBO_QUEUE_SIZE];
static uint8_t queue_head;
static uint8_t queue_tail;
static uint8_t queue_count;
static bool pending_activity;
static bool flush_in_progress;

// Bit N is set when key N participates in any combo on the current layer.
static uint8_t combo_key_bitmap[(NUM_KEYS + 7) / 8];
static uint8_t combo_key_bitmap_layer = COMBO_KEY_NONE;

static uint16_t combo_term_ms(const advanced_key_t *ak) {
  return ak->combo.term > 0 ? ak->combo.term : DEFAULT_COMBO_TERM;
}

static int combo_key_count(const advanced_key_t *ak) {
  int count = 0;

  for (int k = 0; k < 4; k++) {
    if (ak->combo.keys[k] != COMBO_KEY_NONE && ak->combo.keys[k] < NUM_KEYS)
      count++;
  }

  return count;
}

static void combo_key_bitmap_rebuild(uint8_t layer) {
  if (combo_key_bitmap_layer == layer)
    return;

  memset(combo_key_bitmap, 0, sizeof(combo_key_bitmap));

  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
    if (ak->type != AK_TYPE_COMBO || ak->layer != layer)
      continue;

    for (int k = 0; k < 4; k++) {
      const uint8_t key = ak->combo.keys[k];
      if (key >= NUM_KEYS)
        continue;

      combo_key_bitmap[key / 8] |= (uint8_t)(1U << (key % 8));
    }
  }

  combo_key_bitmap_layer = layer;
}

static bool is_key_in_any_combo(uint8_t key) {
  if (key >= NUM_KEYS)
    return false;

  return (combo_key_bitmap[key / 8] & (uint8_t)(1U << (key % 8))) != 0;
}

static combo_event_t *queue_peek(uint8_t offset) {
  if (offset >= queue_count)
    return NULL;

  return &event_queue[(queue_head + offset) % COMBO_QUEUE_SIZE];
}

static bool queue_has_unconsumed_press(uint8_t key) {
  for (uint8_t i = 0; i < queue_count; i++) {
    combo_event_t *ev = queue_peek(i);
    if (!ev || ev->consumed)
      continue;
    if (ev->key == key && ev->pressed)
      return true;
  }

  return false;
}

static void queue_pop(void) {
  if (queue_count == 0)
    return;

  queue_head = (queue_head + 1) % COMBO_QUEUE_SIZE;
  queue_count--;
}

static void flush_events(uint8_t count_to_flush);

static void queue_push(uint8_t key, bool pressed, uint32_t time) {
  if (queue_count >= COMBO_QUEUE_SIZE) {
    // Try to free one slot first so overflow does not silently corrupt order.
    flush_events(1);
    if (queue_count >= COMBO_QUEUE_SIZE)
      queue_pop();
  }

  event_queue[queue_tail] = (combo_event_t){
      .key = key,
      .pressed = pressed,
      .time = time,
      .consumed = false,
  };
  queue_tail = (queue_tail + 1) % COMBO_QUEUE_SIZE;
  queue_count++;
}

// Re-entrant flushes are skipped to avoid mutating the queue mid-flush.
static void flush_events(uint8_t count_to_flush) {
  if (flush_in_progress)
    return;

  flush_in_progress = true;

  for (uint8_t i = 0; i < count_to_flush && queue_count > 0; i++) {
    combo_event_t *ev = queue_peek(0);

    if (!ev->consumed && layout_process_key(ev->key, ev->pressed))
      pending_activity = true;

    queue_pop();
  }

  flush_in_progress = false;
}

// Returns:
// 0 = no match
// 1 = candidate (partial match still within term)
// 2 = full match
static int check_combo_match(const advanced_key_t *ak, uint32_t current_time) {
  int keys_found = 0;
  const int keys_required = combo_key_count(ak);
  bool active_part[4] = {0};
  uint32_t key_times[4] = {0};

  if (keys_required == 0)
    return 0;

  for (uint8_t i = 0; i < queue_count; i++) {
    combo_event_t *ev = queue_peek(i);
    if (!ev || ev->consumed || !ev->pressed)
      continue;

    bool is_part = false;
    for (int k = 0; k < 4; k++) {
      if (ak->combo.keys[k] != ev->key)
        continue;

      is_part = true;
      if (!active_part[k]) {
        active_part[k] = true;
        key_times[k] = ev->time;
      }
      break;
    }

    if (!is_part)
      return 0;
  }

  for (int k = 0; k < 4; k++) {
    if (ak->combo.keys[k] < NUM_KEYS && active_part[k])
      keys_found++;
  }

  const uint16_t term = combo_term_ms(ak);

  if (keys_found == keys_required) {
    uint32_t min_t = 0;
    uint32_t max_t = 0;
    bool first = true;

    for (int k = 0; k < 4; k++) {
      if (!active_part[k])
        continue;

      if (first) {
        min_t = key_times[k];
        max_t = key_times[k];
        first = false;
      } else {
        if (key_times[k] < min_t)
          min_t = key_times[k];
        if (key_times[k] > max_t)
          max_t = key_times[k];
      }
    }

    return (max_t - min_t) <= term ? 2 : 0;
  }

  if (keys_found > 0) {
    uint32_t min_t = 0;
    bool first = true;

    for (int k = 0; k < 4; k++) {
      if (!active_part[k])
        continue;

      if (first || key_times[k] < min_t) {
        min_t = key_times[k];
        first = false;
      }
    }

    if (!first && (current_time - min_t) <= term)
      return 1;
  }

  return 0;
}

static void mark_combo_events_consumed(const advanced_key_t *ak) {
  for (uint8_t q = 0; q < queue_count; q++) {
    combo_event_t *ev = queue_peek(q);
    if (!ev || ev->consumed)
      continue;

    for (int k = 0; k < 4; k++) {
      if (ak->combo.keys[k] == ev->key) {
        ev->consumed = true;
        break;
      }
    }
  }
}

static void execute_combo_match(const advanced_key_t *ak) {
  deferred_action_t release = {
      .type = DEFERRED_ACTION_TYPE_RELEASE,
      .key = INPUT_ROUTING_VIRTUAL_KEY,
      .keycode = ak->combo.output_keycode,
  };

  mark_combo_events_consumed(ak);

  if (deferred_action_push(&release))
    input_keycode_press(ak->combo.output_keycode);

  pending_activity = true;
  flush_events(queue_count);
}

static void process_combo_logic(uint32_t current_time) {
  const uint8_t current_layer = layout_get_current_layer();
  const advanced_key_t *best_match = NULL;
  int best_match_len = 0;
  bool pending_candidates = false;
  uint16_t max_pending_term = DEFAULT_COMBO_TERM;

  combo_key_bitmap_rebuild(current_layer);

  for (uint32_t i = 0; i < NUM_ADVANCED_KEYS; i++) {
    const advanced_key_t *ak = &CURRENT_PROFILE.advanced_keys[i];
    if (ak->type != AK_TYPE_COMBO || ak->layer != current_layer)
      continue;

    const int status = check_combo_match(ak, current_time);
    if (status == 2) {
      const int match_len = combo_key_count(ak);
      if (!best_match || match_len > best_match_len) {
        best_match = ak;
        best_match_len = match_len;
      }
      continue;
    }

    if (status == 1) {
      pending_candidates = true;
      const uint16_t term = combo_term_ms(ak);
      if (term > max_pending_term)
        max_pending_term = term;
    }
  }

  if (best_match) {
    combo_event_t *head = queue_peek(0);
    if (pending_candidates && head &&
        (current_time - head->time) <= max_pending_term)
      return;

    execute_combo_match(best_match);
    return;
  }

  if (pending_candidates) {
    combo_event_t *head = queue_peek(0);
    if (head && (current_time - head->time) > max_pending_term)
      flush_events(1);
    return;
  }

  flush_events(queue_count);
}

void advanced_key_combo_clear(void) {
  queue_head = 0;
  queue_tail = 0;
  queue_count = 0;
  pending_activity = false;
  flush_in_progress = false;
  memset(event_queue, 0, sizeof(event_queue));
  memset(combo_key_bitmap, 0, sizeof(combo_key_bitmap));
  combo_key_bitmap_layer = COMBO_KEY_NONE;
}

bool advanced_key_combo_process(uint8_t key, bool pressed, uint32_t time) {
  const uint8_t current_layer = layout_get_current_layer();

  combo_key_bitmap_rebuild(current_layer);

  if (queue_count == 0 && !is_key_in_any_combo(key))
    return false;

  if (!is_key_in_any_combo(key)) {
    if (pressed && queue_count > 0)
      flush_events(queue_count);
    return false;
  }

  if (!pressed) {
    if (queue_has_unconsumed_press(key)) {
      queue_push(key, false, time);
      flush_events(queue_count);
      return true;
    }

    if (layout_process_key(key, false))
      pending_activity = true;

    if (queue_count > 0)
      process_combo_logic(time);

    return true;
  }

  queue_push(key, true, time);
  process_combo_logic(time);
  return true;
}

bool advanced_key_combo_task(void) {
  pending_activity = false;

  if (queue_count > 0)
    process_combo_logic(timer_read());

  return pending_activity;
}

void advanced_key_combo_invalidate_cache(void) {
  combo_key_bitmap_layer = COMBO_KEY_NONE;
}
