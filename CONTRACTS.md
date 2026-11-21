# Module Contracts — Smart Campus Parking Management System (Sprint 0)

## Build contract

```
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o parking
```

All sources must compile warning-free under these flags. No external
dependencies; standard C library only.

## Invariants (enforced by owning modules)

1. **Unique vehicle registration numbers** — `vehicle_register` rejects
   duplicates with `STATUS_ALREADY_EXISTS`.
2. **One active session per vehicle** — `session_start` rejects a vehicle that
   already has an active session with `STATUS_CONFLICT`.
3. **One active vehicle per parking slot** — `session_start` rejects an already
   occupied slot with `STATUS_CONFLICT`; `session_end` releases the slot.
4. **Completed sessions remain in history** — sessions are never removed on
   end; they are marked `active = 0` with `end_time` set.
5. **Valid IDs and array bounds** — all `*_find_by_id` / `*_get` functions
   validate bounds and return `STATUS_NOT_FOUND` / `NULL`.
6. **Consistent slot and session state** — slot `is_available`/`vehicle_id`
   stay in sync with session start/end.
7. **No overlapping reservations for the same slot** — `reservation_create`
   rejects overlaps with `STATUS_CONFLICT` (excluding cancelled reservations).

## Return / status convention

All public functions return `Status` (`common.h`):

- `STATUS_OK (0)` — success.
- `STATUS_ERROR (-1)` — generic failure.
- `STATUS_NOT_FOUND (-2)` — record absent.
- `STATUS_ALREADY_EXISTS (-3)` — unique constraint violated.
- `STATUS_INVALID_ARGUMENT (-4)` — NULL pointer / bad argument.
- `STATUS_OUT_OF_MEMORY (-5)` — allocation failure.
- `STATUS_CONFLICT (-6)` — business invariant violated.
- `STATUS_IO_ERROR (-7)` — persistence failure.

`Id` is `long long`; `ID_NONE` (-1) is the null sentinel. Strings are bounded
by `MAX_REG_LEN`, `MAX_MODEL_LEN`, `MAX_OWNER_LEN`, `MAX_LOCATION_LEN`.

## Architectural rules

1. Only one module owns each operation.
2. No circular header dependencies (model headers forward-declare
   `ParkingSystem`; only `system.h` includes them all).
3. No `.c` files included from other `.c` files.
4. Public function signatures explicit and stable.
5. Shared state passed through `ParkingSystem *` pointers.
6. No duplicated business logic across modules.
7. Bounded string handling (`strncpy`/`snprintf` with explicit NUL).
8. `static` functions for module-private helpers.
9. No non-static global variables in headers (`extern` only via
   `config.h`/`config.c`).

## Public API by module

### config (Developer 3)
- `Status config_init(void)`
- `const char *config_summary(void)`
- Extern values: `CONFIG_DATA_PATH`, `CONFIG_REPORTS_PATH`,
  `CONFIG_DEFAULT_HOURLY_RATE`, `CONFIG_MAX_SESSION_HOURS`,
  `CONFIG_SLOT_CAPACITY`

### vehicle (Developer 1)
- `Status vehicle_register(ParkingSystem*, const char*, VehicleType,
  const char*, UserType, Id*)`
- `Status vehicle_find_by_reg(const ParkingSystem*, const char*, size_t*)`
- `Status vehicle_find_by_id(const ParkingSystem*, Id, size_t*)`
- `Status vehicle_remove(ParkingSystem*, Id)`
- `const Vehicle *vehicle_get(const ParkingSystem*, size_t)`
- `size_t vehicle_count(const ParkingSystem*)`

### slot (Developer 1)
- `Status slot_init(ParkingSystem*, int)`
- `Status slot_find_free(const ParkingSystem*, ZoneType, size_t*)`
- `Status slot_find_by_id(const ParkingSystem*, Id, size_t*)`
- `Status slot_occupy(ParkingSystem*, size_t, Id)`
- `Status slot_release(ParkingSystem*, size_t)`
- `const ParkingSlot *slot_get(const ParkingSystem*, size_t)`
- `size_t slot_count(const ParkingSystem*)`

### session (Developer 1)
- `Status session_start(ParkingSystem*, Id vehicle_id, Id slot_id, time_t, Id*)`
- `Status session_end(ParkingSystem*, Id vehicle_id, time_t)`
- `Status session_find_active_for_vehicle(const ParkingSystem*, Id, size_t*)`
- `Status session_find_by_id(const ParkingSystem*, Id, size_t*)`
- `const ParkingSession *session_get(const ParkingSystem*, size_t)`
- `size_t session_count(const ParkingSystem*)`

### reservation (Developer 2)
- `Status reservation_create(ParkingSystem*, Id vehicle_id, Id slot_id,
  time_t, time_t, Id*)`
- `Status reservation_cancel(ParkingSystem*, Id)`
- `Status reservation_complete(ParkingSystem*, Id)`
- `Status reservation_find_by_id(const ParkingSystem*, Id, size_t*)`
- `const Reservation *reservation_get(const ParkingSystem*, size_t)`
- `size_t reservation_count(const ParkingSystem*)`

### billing (Developer 2)
- `Status billing_compute(const ParkingSystem*, Id session_id, long*)`
- `int billing_format(const ParkingSystem*, Id session_id, char*, size_t)`

### system (Developer 3)
- `Status system_create(ParkingSystem**)`
- `void system_destroy(ParkingSystem*)`
- `Status system_reset(ParkingSystem*)`
- `Id system_next_id(ParkingSystem*, const char*)`

### storage (Developer 3)
- `Status storage_save(const ParkingSystem*, const char*)`
- `Status storage_load(ParkingSystem*, const char*)`

### analytics (Developer 3)
- `size_t analytics_active_sessions(const ParkingSystem*)`
- `size_t analytics_occupied_slots(const ParkingSystem*)`
- `long analytics_total_revenue(const ParkingSystem*)`
- `size_t analytics_completed_sessions(const ParkingSystem*)`

## Data ownership

`ParkingSystem` owns all dynamically allocated arrays
(`vehicles`, `slots`, `sessions`, `reservations`), their counts/capacities and
the next-ID counters. Relationships between records use stable integer IDs,
never raw pointers. `system_destroy` frees all owned memory.

## Instructions for Developers 1 and 2

- Implement feature logic only inside your owned modules; do not touch other
  modules' `.c`/`.h` files without agreement.
- Add new public functions to your header and implement them in your `.c`;
  keep signatures explicit and stable.
- Use `ParkingSystem *` for shared state; never duplicate business logic
  (e.g. session/slot consistency lives in `session.c`).
- Reserve IDs via `system_next_id(sys, "vehicle"/"slot"/"session"/"reservation")`.
- Validate arguments and bounds; return the appropriate `Status`.
- Keep module-private helpers `static`.

## Unresolved architectural decisions

- `main.c` entry point and CLI command set (Developer 3 to define in Sprint 1).
- Whether `time_t` is persisted directly or normalized (currently stored as
  `int64_t` in `storage.c`).
- Report file formats and output destination under `reports/`.
- Test framework choice and test file naming under `tests/`.
