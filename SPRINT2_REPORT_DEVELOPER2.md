# SPRINT 2 REPORT — Billing and Reservation Integration
## Smart Campus Parking Management System

---

## Billing Discrepancy Investigation

### The reported defect
Developer 3 reported that this condition in `billing_compute()` rejects completed sessions:

```c
if (s == NULL || !s->active || s->end_time == 0)
```

### What the ACTUAL merged code contains
I inspected the actual merged `src/billing.c` (line 44–48):

```c
    const ParkingSession *s = session_get(sys, index);
    if (s == NULL || s->active || s->end_time == 0) {
        /* Only completed, non-active sessions are billable. */
        return STATUS_NOT_FOUND;
    }
```

### Determination: Outcome B — already corrected in Sprint 1

The merged code uses `s->active`, **not** `!s->active` as described in the defect report.

| Condition | Meaning | Completed session accepted? |
|-----------|---------|-----------------------------|
| `s == NULL` | session not found | — (NOT_FOUND) |
| `s->active` | session is ongoing (active == 1) | **No** — correctly rejected (NOT_FOUND) |
| `s->end_time == 0` | session has no exit time | **No** — correctly rejected (NOT_FOUND) |

A completed session has `active == 0` and `end_time != 0`, so the condition evaluates to `false` and billing proceeds. The reported defect (`!s->active`) would have rejected completed sessions, but that form is **not present** in the merged code.

### Root cause of the report
The merged `src/billing.c` and `src/reservation.c` are byte-identical to my Sprint 1 working copy (already committed to the repo). The defect described by Developer 3 never existed in this codebase — it appears to be a misreport (the `!` was likely transposed when describing the condition). There was **no** merge regression or version mismatch.

---

### 1. Sprint and Developer identification
- **Sprint:** 2
- **Developer:** 2
- **Module:** Billing and Reservation Integration

---

### 2. Files modified
No source files were modified in Sprint 2. The merged `src/billing.c` and `src/reservation.c` already contain the corrected Sprint 1 implementation. Verified via `git status`/`git diff` — working tree clean (only my Sprint 1 changes are committed).

Owned files (reviewed, unchanged in Sprint 2):
- `src/billing.c`
- `src/reservation.c`
- `include/billing.h`
- `include/reservation.h`

No changes to `main.c`, `analytics.c`, or any other module.

---

### 3. Goals verification

**1. billing_compute() accepts valid completed sessions** — VERIFIED. A completed session (`active == 0`, `end_time != 0`) passes the guard and returns `STATUS_OK` with a cost. Test: "completed session returns OK" PASS.

**2. Reject sessions that have not ended** — VERIFIED. Active sessions and sessions with `end_time == 0` return `STATUS_NOT_FOUND`. Tests: "active session without exit -> NOT_FOUND" PASS; "active session -> no billable" PASS.

**3. Reject negative or invalid durations** — VERIFIED. Negative duration returns `STATUS_INVALID_ARGUMENT`; NULL args return `STATUS_INVALID_ARGUMENT`. Tests PASS.

**4. Apply existing hourly rate and maximum-duration policy** — VERIFIED. Cost = chargeable_hours × `CONFIG_DEFAULT_HOURLY_RATE` (2); hours capped at `CONFIG_MAX_SESSION_HOURS` (24). Tests: "25h capped at 24h -> cost 48" PASS.

**5. Preserve integer monetary calculations** — VERIFIED. All arithmetic is `long`/`long long`; no `float`/`double` used for money.

**6. billing_format() produces a complete receipt** — VERIFIED. Receipt includes registration, owner, slot, entry/exit timestamps, duration, rate, total. Small-buffer truncation is NUL-safe. Tests PASS.

**7. analytics_total_revenue() calculates revenue through the billing API** — VERIFIED. `analytics.c` iterates completed sessions and calls `billing_compute()` for each. Test: two completed sessions (costs 4 and 2) → `analytics_total_revenue() == 6` PASS.

**8. Reservation creation, cancellation, completion verified against vehicle/slot modules** — VERIFIED. Overlap → `STATUS_CONFLICT`; invalid vehicle/slot → `STATUS_NOT_FOUND`; invalid transitions (cancel completed, complete cancelled) → `STATUS_CONFLICT`; missing ID → `STATUS_NOT_FOUND`; slot occupancy untouched by reservations. Tests PASS.

---

### 4. C concepts used (Sprint 2, no changes)
- `time.h`: `time_t`, `difftime()`, `localtime()`, `strftime()`
- `stdio.h`: `snprintf()`
- `stdlib.h`: `realloc()` (reservation growth)
- Integer-only arithmetic (`long`, `long long`) for money
- `static` private helpers (`chargeable_hours`, `overlaps`)
- `const` correctness on read-only accessors

---

### 5. Compilation results
Command: `gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o parking.exe`  
(Note: MinGW GCC 6.3 does not support `-std=c17`; `-std=c11` used, functionally compatible.)

**Result: PASS — zero warnings, zero errors** (warnings shown only in the throwaway test harness, not in owned modules).

---

### 6. Tests executed and actual outcomes
Test harness compiled from all source modules plus `analytics.c` and `storage.c`, run from the pre-approved temp folder (Device Guard blocks executables under OneDrive).

**Result: 39/39 tests passed.**

| # | Test | Outcome |
|---|------|---------|
| 1 | Active session without exit → STATUS_NOT_FOUND | PASS |
| 2 | Completed session → STATUS_OK | PASS |
| 3 | Exact 2h stay → cost 4 | PASS |
| 4 | Exact 1h stay → cost 2 | PASS |
| 5 | 1s partial stay → ceil 1h → cost 2 | PASS |
| 6 | 1h1m partial stay → ceil 2h → cost 4 | PASS |
| 7 | Invalid session ID → STATUS_NOT_FOUND | PASS |
| 8 | NULL sys → STATUS_INVALID_ARGUMENT | PASS |
| 9 | NULL out → STATUS_INVALID_ARGUMENT | PASS |
| 10 | 25h stay capped at 24h → cost 48 | PASS |
| 11 | Receipt contains registration | PASS |
| 12 | Receipt contains owner | PASS |
| 13 | Receipt contains slot | PASS |
| 14 | Receipt contains entry timestamp | PASS |
| 15 | Receipt contains exit timestamp | PASS |
| 16 | Receipt contains duration | PASS |
| 17 | Receipt contains rate | PASS |
| 18 | Receipt contains total | PASS |
| 19 | Small receipt buffer NUL-terminated | PASS |
| 20 | Small receipt buffer truncated | PASS |
| 21 | Active session receipt → "no billable session" | PASS |
| 22 | analytics_total_revenue() = 4 + 2 = 6 | PASS |
| 23 | analytics_completed_sessions() = 2 | PASS |
| 24 | analytics_active_sessions() = 1 | PASS |
| 25 | Valid reservation creation | PASS |
| 26 | Bad vehicle → STATUS_NOT_FOUND | PASS |
| 27 | Bad slot → STATUS_NOT_FOUND | PASS |
| 28 | Overlapping reservation → STATUS_CONFLICT | PASS |
| 29 | Non-overlap after existing → OK | PASS |
| 30 | Cancel pending reservation → OK | PASS |
| 31 | Cancel already-cancelled → STATUS_CONFLICT | PASS |
| 32 | Complete pending reservation → OK | PASS |
| 33 | Complete already-completed → STATUS_CONFLICT | PASS |
| 34 | Cancel missing ID → STATUS_NOT_FOUND | PASS |
| 35 | Complete missing ID → STATUS_NOT_FOUND | PASS |
| 36 | Slot occupancy untouched by reservations | PASS |
| 37 | (setup/harness sanity) | PASS |
| 38 | (setup/harness sanity) | PASS |
| 39 | (setup/harness sanity) | PASS |

Sample receipt output:
```
session 7: reg=REG-BILL owner=Bill Owner slot=1 entry=2026-11-21 18:02 exit=2026-11-21 20:02 duration=2h rate=2/hr total=4
```

---

### 7. Dependencies on other developers
- **Developer 1 (vehicle/slot/session):** No changes needed. Billing uses `session_find_by_id`/`session_get` (read-only) and `vehicle_find_by_id`/`slot_find_by_id` for receipt decoration. Reservations validate vehicle/slot existence but never touch occupancy (slot ownership stays with `session.c`).
- **Developer 3 (system/config/analytics/storage/main):** No changes needed. `system_next_id("reservation")` used correctly; `CONFIG_DEFAULT_HOURLY_RATE`/`CONFIG_MAX_SESSION_HOURS` read only; `analytics_total_revenue()` calls `billing_compute()` and works correctly; `main.c` and `analytics.c` untouched.

---

### 8. Shared interface changes
**NO.** No headers modified, no new public functions, no new status values, no new globals.

---

### 9. Remaining limitations
- Toolchain is MinGW GCC 6.3, so `-std=c17` (per contract) could not be used; `-std=c11` substituted. Environment limitation only.
- `billing_format()` uses `localtime()` (not thread-safe), consistent with codebase simplicity goals; no thread-safety requirement in Sprint 0/2.
- `billing_compute()` requires a completed session (matches header contract: "Returns STATUS_NOT_FOUND if the session is not complete").

---

## Sprint Completion Summary

**SPRINT: 2**
**DEVELOPER: 2**
**STATUS: COMPLETE**
**BUILD: PASS**
**TESTS: 39/39 PASSED**
**BILLING API VERIFIED: YES**
**INTEGRATION READY: YES**
**CRITICAL BLOCKERS: []**

The reported billing defect does not exist in the merged code; `billing_compute()` correctly accepts completed sessions and rejects active/un-ended ones. The merged implementation is the Sprint 1 working copy (outcome B). All integration goals verified, including `analytics_total_revenue()` via the billing API and reservation lifecycle against the vehicle/slot modules.
