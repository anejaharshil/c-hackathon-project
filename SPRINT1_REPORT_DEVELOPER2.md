# SPRINT 1 REPORT — Billing and Reservation Engine
## Smart Campus Parking Management System

---

### 1. Sprint and Developer identification
- **Sprint:** 1
- **Developer:** 2
- **Module:** Billing and Reservation Engine (`src/billing.c`, `src/reservation.c`, `include/billing.h`, `include/reservation.h`)

---

### 2. Initial module status
Sprint 0 delivered preliminary implementations that compiled but had defects:

**`src/billing.c`:**
- Used floating-point (`double`) arithmetic for money — forbidden by the billing goals.
- Did not enforce `CONFIG_MAX_SESSION_HOURS` (24).
- Rounding policy was `hours * rate + 0.5`, inconsistent and undocumented.
- Negative durations were silently clamped to zero instead of rejected.
- No arithmetic-overflow protection.
- `billing_format()` emitted only "session N: cost X minor units" — missing registration, owner, slot, entry/exit timestamps, duration, hourly rate, total charge.
- `%lld`/`%ld` format-specifier warnings under `-Wall -Wextra -Wpedantic`.
- Did not use `time.h`/`difftime()` and `localtime()`/`strftime()` properly for timestamps.

**`src/reservation.c`:**
- `realloc` growth had no size-overflow check.
- `reservation_cancel()` allowed cancelling an already-CANCELLED or COMPLETED reservation (no invalid-transition guard).
- `reservation_complete()` allowed completing an already-COMPLETED or CANCELLED reservation (no valid-transition guard).
- No validation that start/end times are non-negative (only `end <= start` was checked).

---

### 3. Exact files modified
- `src/billing.c`
- `src/reservation.c`

No other files were touched. Headers `include/billing.h` and `include/reservation.h` were reviewed but left unchanged (public interfaces preserved).

---

### 4. Functions implemented or corrected

**Billing (`src/billing.c`):**
- `billing_compute()` — rewritten: integer-only duration via `difftime()`, negative-duration rejection, ceiling rounding to chargeable hours, `CONFIG_MAX_SESSION_HOURS` cap, overflow-safe `long long` intermediate, no state mutation, no direct printing.
- `billing_format()` — rewritten: emits registration, owner, slot, entry/exit timestamps (via `localtime()`/`strftime()`), duration in hours, hourly rate, total charge; `snprintf()` with safe truncation and NUL termination; handles NULL/zero buffer and insufficient capacity.

**Reservations (`src/reservation.c`):**
- `reservation_create()` — added non-negative time validation, kept vehicle/slot existence validation, overlap rejection (excluding cancelled), safe array growth with overflow check and temp `realloc` pointer, `system_next_id("reservation")`, returns clear `Status`.
- `reservation_cancel()` — added invalid-state-transition guard (rejects CANCELLED/COMPLETED with `STATUS_CONFLICT`).
- `reservation_complete()` — added valid-transition guard (only PENDING/ACTIVE → COMPLETED; rejects already-COMPLETED/CANCELLED with `STATUS_CONFLICT`); preserves history, does not modify occupancy.
- `reservation_find_by_id()`, `reservation_get()`, `reservation_count()` — kept consistent lookup semantics.

---

### 5. Billing formula and rounding rules
Policy (documented assumption, using only existing config — no new globals, no floating-point money, no category tariffs):

- `duration_seconds = difftime(end_time, start_time)` as `long long`.
- Negative duration → `STATUS_INVALID_ARGUMENT` (rejected).
- `chargeable_hours = ceiling(duration_seconds / 3600)`, computed as `(seconds + 3599) / 3600`.
- Minimum chargeable hours = 1 for any positive stay (very short stays handled consistently).
- Cap: `chargeable_hours = min(chargeable_hours, CONFIG_MAX_SESSION_HOURS)` (24).
- `cost = chargeable_hours * CONFIG_DEFAULT_HOURLY_RATE` (2) in minor units (long).
- All arithmetic integer; `long long` intermediates avoid overflow; `hours * rate` max 24 * 2 = 48, no overflow.

---

### 6. Reservation state transitions
Existing `ReservationStatus` values only (no new values added):

- `RESERVATION_PENDING` → `RESERVATION_CANCELLED` via `reservation_create` then `reservation_cancel` (allowed).
- `RESERVATION_PENDING` / `RESERVATION_ACTIVE` → `RESERVATION_COMPLETED` via `reservation_complete` (allowed).
- `RESERVATION_CANCELLED` → any further change rejected (`STATUS_CONFLICT`).
- `RESERVATION_COMPLETED` → any further change rejected (`STATUS_CONFLICT`).
- Completed/cancelled reservations remain in the array (history preserved); no record removal.
- `reservation_complete()` does not directly modify parking occupancy (slots/sessions untouched).

---

### 7. C concepts actually used
- C11 (compiler toolchain is MinGW GCC 6.3, which does not support `-std=c17`; `-std=c11` is functionally compatible for this codebase).
- `<time.h>`: `time_t`, `difftime()`, `localtime()`, `strftime()`, `struct tm`.
- `<stdio.h>`: `snprintf()`.
- `<stdlib.h>`: `realloc()`.
- `<string.h>`: `strlen()`, `strcmp()` (via other modules).
- `static` module-private helper `overlaps()` and `chargeable_hours()`.
- `const` correctness on read-only accessors and `const ParkingSystem *` billing params.
- Integer types: `long long` (`Id`), `long` (cost), `size_t` (index/capacity).
- Structs: `Reservation`, `ParkingSession`, `Vehicle`, `ParkingSlot`.
- No floating-point for money; no VLA; no recursion.

---

### 8. Dynamic memory safety
- Growth uses a temporary `realloc` pointer; original array preserved on failure (`sys->reservations` not overwritten if `tmp == NULL`).
- Capacity updated only after successful realloc.
- Size-overflow check: `new_cap > (size_t)-1 / sizeof(Reservation)` before realloc.
- Initial capacity 8, doubling strategy.
- No memory owned by `ParkingSystem` is freed by the reservation module (ownership stays with `system_destroy`).
- No memory leaks introduced on the success path.

---

### 9. Compilation results
Command: `gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o parking.exe`  
(Note: `-std=c17` is unsupported by MinGW GCC 6.3; `-std=c11` used, which is compatible.)

**Result: PASS — zero warnings, zero errors.** The earlier `%lld`/`%ld` format warnings in `billing.c` are resolved.

---

### 10. Tests executed and actual outcomes
A test harness (`C:\Users\IIISI\AppData\Local\Temp\opencode\test_sprint1.c`, outside the repo) exercising all 14 required areas plus argument-edge cases was compiled and run.

**Result: 36/36 tests passed.**

| # | Test | Outcome |
|---|------|---------|
| 1 | Normal billing duration (2 h → cost 4) | PASS |
| 2 | Short duration (1 s → ceil 1 h → cost 2) | PASS |
| 3 | Exact-hour duration (3600 s → cost 2) | PASS |
| 4 | Multi-hour duration (5 h 1 m → ceil 6 h → cost 12) | PASS |
| 5 | Invalid session ID → STATUS_NOT_FOUND | PASS |
| 6 | Negative duration / NULL args → STATUS_INVALID_ARGUMENT | PASS |
| 7 | Receipt formatting (reg, owner, slot, timestamps, duration, rate, total) | PASS |
| 8 | Small receipt buffer → NUL-terminated, truncated safely | PASS |
| 9 | Valid reservation creation | PASS |
| 10 | Invalid vehicle or slot → STATUS_NOT_FOUND | PASS |
| 11 | Overlapping reservation → STATUS_CONFLICT | PASS |
| 12 | Reservation cancellation | PASS |
| 13 | Invalid status transition → STATUS_CONFLICT; not-found → STATUS_NOT_FOUND | PASS |
| 14 | Dynamic reservation-array growth | PASS |

All receipts verified to contain registration, owner, slot, entry/exit timestamps, duration, rate, and total.

---

### 11. Dependencies on Developer 1 and Developer 3
- **Developer 1 (vehicle/slot/session):** Depends on `vehicle_find_by_id`, `slot_find_by_id`, `slot_occupy`/`slot_release`, `session_start`/`session_end`, `session_find_by_id`, `session_get`. These interfaces are unchanged and used read-only (billing) or for validation (reservation). No changes required from Developer 1.
- **Developer 3 (system/config/storage/analytics/main):** Depends on `system_create`, `system_destroy`, `system_next_id`, `config_init`, `CONFIG_DEFAULT_HOURLY_RATE`, `CONFIG_MAX_SESSION_HOURS`. All unchanged. `system_next_id("reservation")` used correctly. No changes required from Developer 3.
- Integration: reservation module does not touch slot occupancy (Developer 1's `session.c` owns that). Billing module does not create/close sessions or occupy/release slots. Clean separation preserved.

---

### 12. Shared interface changes
**NO.** Public signatures in `include/billing.h` and `include/reservation.h` are unchanged. No new public functions, no new status enum values, no new globals. Headers were not modified.

---

### 13. Remaining limitations
- `billing_compute` requires the session to be completed (non-active, `end_time` set). Active sessions return `STATUS_NOT_FOUND` (matches header contract: "Returns STATUS_NOT_FOUND if the session is not complete").
- Negative-duration detection relies on `difftime(end, start) < 0`; the public API cannot easily construct end < start through `session_end`, so the guard is defensive for corrupted/inconsistent data.
- Receipt uses `localtime()` (not thread-safe), consistent with the codebase's simplicity goals; no thread-safety requirement exists in Sprint 0/1.
- Toolchain is MinGW GCC 6.3, so `-std=c17` from the contract could not be used; `-std=c11` was substituted. This is an environment limitation, not a code change.

---

### 14. Next-sprint recommendations
- Developer 3 (main/CLI): expose billing and reservation commands; `billing_format` output can be routed to reports under `CONFIG_REPORTS_PATH`.
- Consider a grace period for very short stays if product requires it (currently any positive stay = 1 chargeable hour).
- Add persistent storage of reservations (Developer 3 `storage.c`) if not already covered.
- Consider `RESERVATION_ACTIVE` auto-transition when a session starts for a reserved slot (requires coordination with Developer 1's session module).
- When toolchain supports it, switch build back to `-std=c17`.

---

**SPRINT: 1**
**DEVELOPER: 2**
**STATUS: COMPLETE**
**BUILD: PASS**
**TESTS: 36/36 PASSED**
**FILES MODIFIED: [src/billing.c, src/reservation.c]**
**SHARED INTERFACE CHANGES: NO**
**INTEGRATION READY: YES**
**CRITICAL BLOCKERS: []**
