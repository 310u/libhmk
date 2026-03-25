# Event Pipeline

This document describes the key event pipeline that runs between `matrix_scan()`
and `hid_send_reports()` in `libhmk`.

## Stages

1. `collect`
   `matrix.c` updates `key_matrix[key].is_pressed`, `distance`, and
   `event_time`. `layout_collect_events()` compares that physical state against
   `key_press_states` and emits only the edges that have not been consumed yet.

2. `sort`
   `layout_sort_events()` processes the collected edges in chronological order.
   When multiple edges collapse into the same millisecond tick, release edges
   sort before press edges. Equal-timestamp presses sort by deeper travel first;
   equal-timestamp releases sort by shallower travel first.

3. `process`
   Each sorted edge first passes through combo handling. Non-combo edges then go
   through `layout_process_key()`, which resolves layer actions, remembers the
   active keycode or advanced-key binding, and dispatches the press or release.

4. `pending`
   If any Tap-Hold key is still undecided, non-Tap-Hold presses are buffered in
   FIFO order instead of being dispatched immediately. If a buffered press later
   receives a release before the Tap-Hold resolves, that release is buffered as
   well.

5. `flush`
   Once no Tap-Hold key remains undecided, buffered events are replayed through
   `layout_process_key()` in their original FIFO order. Combo fallback also uses
   this replay path when a combo candidate times out without matching.

6. `report`
   `hid_send_reports()` runs after synchronous event processing and pending
   flushes, but before `deferred_action_process()`. Deferred actions therefore
   become visible on the next scan. `hid.c` snapshots keyboard state so a press
   report and its matching release report are both preserved even while the host
   interface is busy.

## Invariants

- `key_matrix[key].is_pressed` is the current physical truth from the matrix.
- `key_press_states[key]` is the last physical truth already consumed by
  `layout_task()`.
- Every edge emitted by `layout_collect_events()` is consumed exactly once by
  either the normal path, the combo queue, or the pending buffer.
- Pending events are replayed in the same FIFO order in which they were
  buffered.
- `active_keycodes[key]` and `active_advanced_keys[key]` bind releases to the
  same logical output path that handled the original press.
- HID sending is non-blocking. If the host is not ready, unsent keyboard
  snapshots stay queued until they can be delivered.

## Press/Release Consistency Rules

- A normal key release must use the keycode captured at press time, even if the
  active layer or profile changed before release.
- An advanced-key release must use the advanced-key binding captured at press
  time, even if the current layer now maps that position differently.
- If a non-Tap-Hold press is buffered because of an undecided Tap-Hold, a later
  release for that same key must also be buffered until the press can be
  replayed.
- Combo-participating keys must either become a combo output or fall back to the
  normal path; they must never disappear silently.
- HID must preserve both sides of a transient tap even if the press is observed
  while the USB keyboard interface is busy and the release is sent later.

## Related Source

- `src/matrix.c`
- `src/layout.c`
- `src/advanced_key_combo.c`
- `src/advanced_key_tap_hold.c`
- `src/hid.c`
