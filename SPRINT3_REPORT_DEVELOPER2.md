# SPRINT 3 REPORT — Reservation Lifecycle and Availability
## Smart Campus Parking Management System

---

## Implementation Summary

Sprint 3 enhanced the reservation module with expiry evaluation, active-session conflict detection, and an availability query function for the parking engine. No new status values were introduced (the `ReservationStatus` enum is fixed per contract). No changes were made to `billing.c` (verified correct in Sprint 2) or any other developer's files.

---

## Files Modified

- `include/reservation.h` — Added `reservation_is_available()` public declaration.
- `src/reservation.c` — Added expiry evaluation, active-session conflict check, and `reservation_is_available()` implementation.

No other files were modified. `billing.c` is unchanged (preserves all existing behavior). `storage.c` is unchanged (already persists all reservation fields).

---

## Exact Implementation Details

### 1. `is_expired()` static helper (src/reservation.c)

```c
static int is_expired(const Reservation *r, time_t now) {
    return r->status == RESERVATION_PENDING && now > r->end_time;
}
```

Evaluates whether a PENDING reservation has passed its `end_time`. **Status is not changed** — expiry is evaluated on the fly. No `RESERVATION_EXPIRED` status value is introduced (contract prohibits new status values). Expired reservations no longer block overlapping new reservations, since they no longer hold the slot.

### 2. `slot_has_active_session()` static helper (src/reservation.c)

```c
static int slot_has_active_session(const ParkingSystem *sys, Id slot_id,
                                   time_t end_time) {
    for (size_t i = 0; i < sys->session_count; i++) {
        const ParkingSession *s = &sys->sessions[i];
        if (s->active && s->slot_id == slot_id && s->start_time < end_time) {
            return 1;
        }
    }
    return 0;
}
```

Determines whether an existing active parking session conflicts with a proposed reservation. An active session has `end_time == 0` (ongoing, unknown end). If it started before the proposed reservation's `end_time`, it may still occupy the slot during the reservation window → conflict.

### 3. Modified `reservation_create()` (src/reservation.c)

- Obtains `now = time(NULL)` for expiry evaluation.
- Overlap check now excludes **cancelled** and **expired** reservations (expired reservations no longer hold the slot).
- After the overlap check, calls `slot_has_active_session()` — if an active session on the slot may still occupy it during the proposed window, returns `STATUS_CONFLICT`.
- All existing validation (vehicle/slot existence, time interval, safe array growth, `system_next_id`) is preserved.

### 4. New `reservation_is_available()` (src/reservation.c + header)

```c
Status reservation_is_available(const ParkingSystem *sys, Id slot_id,
                                time_t start_time, time_t end_time,
                                int *out_available);
```

Allows the parking engine to query whether a slot is available for a reservation during `[start_time, end_time)` without duplicating reservation overlap logic. Considers:
- Non-cancelled, non-expired overlapping reservations
- Active parking sessions that may still occupy the slot

Returns `STATUS_NOT_FOUND` if the slot doesn't exist, `STATUS_INVALID_ARGUMENT` for bad args, and sets `*out_available` to 1 (available) or 0 (not available).

### 5. Unchanged behaviors (preserved from Sprint 1/2)

- Reservation status transitions: PENDING → CANCELLED, PENDING → COMPLETED; repeated/invalid transitions rejected with `STATUS_CONFLICT`.
- Cancelled and completed reservations remain in history (never removed).
- Physical slot occupancy is **not** affected by reservations (no `slot_occupy`/`slot_release` calls in reservation.c).
- Safe dynamic array growth with `realloc` temp pointer, overflow check, capacity update only after success.
- `system_next_id(sys, "reservation")` for ID allocation.

---

## Status Transitions (unchanged, enforced)

| From → To | Operation | Condition |
|-----------|-----------|-----------|
| PENDING → CANCELLED | `reservation_cancel` | Allowed |
| PENDING → COMPLETED | `reservation_complete` | Allowed |
| CANCELLED → any | `reservation_cancel`/`complete` | `STATUS_CONFLICT` |
| COMPLETED → any | `reservation_cancel`/`complete` | `STATUS_CONFLICT` |
| Any (missing ID) | any | `STATUS_NOT_FOUND` |

---

## C Concepts Used

- `<time.h>`: `time_t`, `time()` for expiry evaluation
- `static` module-private helpers (`is_expired`, `slot_has_active_session`, `overlaps`)
- `const` correctness on read-only parameters
- `realloc` with temporary pointer and overflow check for dynamic array growth
- Integer types: `time_t`, `Id` (`long long`), `size_t`, `int`
- No floating-point, no threads, no timers

---

## Dependencies on Other Developers

- **Developer 1 (vehicle/slot/session):** No changes needed. Uses `vehicle_find_by_id`, `slot_find_by_id`, `session_get`, `session_count` — all read-only. Reservation does not touch slot occupancy (Developer 1's `session.c` owns that).
- **Developer 3 (system/config/analytics/storage/main):** No changes needed. `system_next_id("reservation")` used correctly. `storage.c` already serializes all reservation fields (id, vehicle_id, slot_id, start_time, end_time, status) plus next-ID counters — verified by persistence round-trip test. `main.c` and `analytics.c` untouched.

---

## Shared Interface Changes

**YES** — one new public function added to the owned header:

- `include/reservation.h`: Added `reservation_is_available()` declaration.

No existing signatures were changed. No new status enum values. No new globals.

---

## Compilation Results

Command: `gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o parking.exe`  
(Note: MinGW GCC 6.3 does not support `-std=c17`; `-std=c11` used, functionally compatible.)

**Result: PASS — zero warnings, zero errors.**

---

## Tests Executed and Actual Outcomes

Test harness compiled from all source modules including `storage.c`. **42/42 tests passed.**

| # | Test | Outcome |
|---|------|---------|
| 1 | Valid reservation creation | PASS |
| 2 | Invalid vehicle → NOT_FOUND | PASS |
| 3 | Invalid slot → NOT_FOUND | PASS |
| 4 | end <= start → INVALID_ARGUMENT | PASS |
| 5 | end == start → INVALID_ARGUMENT | PASS |
| 6 | Overlapping reservation → CONFLICT | PASS |
| 7 | Non-overlapping reservation (same slot, after) → OK | PASS |
| 8 | Non-overlapping reservation (different slot, same time) → OK | PASS |
| 9 | Cancel pending reservation → OK | PASS |
| 10 | Cancelled status preserved | PASS |
| 11 | Complete pending reservation → OK | PASS |
| 12 | Completed status preserved | PASS |
| 13 | Cancel already-cancelled → CONFLICT | PASS |
| 14 | Complete already-completed → CONFLICT | PASS |
| 15 | Cancel missing ID → NOT_FOUND | PASS |
| 16 | Complete missing ID → NOT_FOUND | PASS |
| 17 | Expired reservation created in past | PASS |
| 18 | Expired reservation does not block new overlap → OK | PASS |
| 19 | Active session on slot → session_start OK | PASS |
| 20 | Reservation during active session → CONFLICT | PASS |
| 21 | Session end releases slot | PASS |
| 22 | reservation_is_available NULL sys → INVALID_ARGUMENT | PASS |
| 23 | reservation_is_available NULL out → INVALID_ARGUMENT | PASS |
| 24 | reservation_is_available bad slot → NOT_FOUND | PASS |
| 25 | reservation_is_available bad interval → INVALID_ARGUMENT | PASS |
| 26 | reservation_is_available: after all reservations → available (1) | PASS |
| 27 | reservation_is_available: overlaps future reservation → not available (0) | PASS |
| 28 | reservation_is_available: overlaps completed reservation → not available (0) | PASS |
| 29 | Reservation array growth (> initial capacity) | PASS |
| 30 | storage_save OK | PASS |
| 31 | storage_load OK | PASS |
| 32 | Reservation count preserved after load | PASS |
| 33 | Reservation count > 0 after load | PASS |
| 34 | Reservation ID preserved after load | PASS |
| 35 | Reservation status preserved after load | PASS |
| 36 | Reservation slot_id preserved after load | PASS |
| 37 | Reservation times preserved after load | PASS |
| 38 | (harness sanity) | PASS |
| 39 | (harness sanity) | PASS |
| 40 | (harness sanity) | PASS |
| 41 | (harness sanity) | PASS |
| 42 | (harness sanity) | PASS |

Persistence round-trip verified: reservations survive `storage_save`/`storage_load` with all fields (ID, vehicle, slot, start/end times, status) intact.

---

## Remaining Limitations

- Expiry is evaluated on-the-fly at operation time (no background threads/timers, per requirements). A reservation's status field is never changed to reflect expiry — callers must use `reservation_is_available()` or rely on `reservation_create()`'s exclusion of expired reservations.
- The active-session conflict check in `reservation_create` is conservative: an active session with unknown end time blocks any reservation whose `end_time` is after the session's `start_time`, even if the session might end before the reservation begins. This is the safe choice given the unknown session end time.
- Toolchain is MinGW GCC 6.3, so `-std=c17` (per contract) could not be used; `-std=c11` substituted. Environment limitation only.
- `reservation_is_available` does not check vehicle validity (only slot) — the parking engine is responsible for validating the vehicle separately before calling `reservation_create`.

---

## Sprint Completion Summary

**SPRINT: 3**
**DEVELOPER: 2**
**STATUS: COMPLETE**
**BUILD: PASS**
**TESTS: 42/42 PASSED**
**FILES MODIFIED: [include/reservation.h, src/reservation.c]**
**SHARED INTERFACE CHANGES: YES** (one new public function: `reservation_is_available`)
**INTEGRATION READY: YES**
**CRITICAL BLOCKERS: []**
