# Smart Campus Parking Management System

A modular, console-based parking management application written in ISO C17.
Sprint 0 delivered the shared skeleton; Sprint 1 added the menu-driven console
application and atomic binary persistence; Sprint 2 added reservation menu
operations, improved analytics, CSV export, and corruption-safe loading.
Sprint 3 added reservation-aware entry allocation. Sprint 4 added automated
tests, git hygiene, and release preparation.

## Build

```sh
make
```

This compiles with:

```sh
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o parking
```

For AddressSanitizer and UndefinedBehaviorSanitizer builds:

```sh
gcc -std=c17 -Wall -Wextra -Wpedantic -g \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Iinclude src/*.c -o parking_asan
```

Clean with `make clean`.

## Run

```sh
./parking
```

## Console menu

```
SMART CAMPUS PARKING SYSTEM
===========================
1. Register Vehicle
2. Vehicle Entry
3. Vehicle Exit
4. Search Vehicle
5. Display Parking Status
6. Parking Statistics
7. Create Reservation
8. View Reservation
9. Cancel Reservation
10. Reservation Status
11. Export CSV Report
12. Save Data
0. Save and Exit
```

All input uses `fgets()` with validated numeric conversion. Empty strings,
non-numeric input, and EOF are handled gracefully.

## Features

- Vehicle registration with unique registration enforcement
- Vehicle entry/exit with zone-aware slot allocation (ZONE_ACCESSIBLE never
  used as overflow)
- Reservation-aware entry allocation: a slot reserved for another vehicle
  during its protected interval cannot be allocated; the reservation holder
  can use their own reserved slot. Enforced at the `session_start()` boundary.
- Parking status and statistics (capacity, occupancy %, zone utilisation,
  active/completed sessions, revenue, average duration, vehicle categories)
- Reservation create/view/cancel/status using Developer 2's reservation API
- CSV export of completed parking sessions to `reports/completed_sessions.csv`
  with proper RFC 4180 field escaping
- Binary `.dat` persistence with atomic save and corruption-safe loading
- Missing save file = legitimate first run; corrupted file = explicit error,
  original data preserved
- Automated system tests under `tests/`

## Directory layout

- `include/`   public headers (shared model, config, module contracts)
- `src/`       module implementations (console app + all modules)
- `data/`      binary `.dat` persistence files (runtime, git-ignored)
- `reports/`   generated CSV reports (runtime, git-ignored)
- `tests/`     automated unit tests

## Module ownership

- Developer 1: `vehicle`, `slot`, `session` (vehicle and parking)
- Developer 2: `reservation`, `billing` (billing and reservations)
- Developer 3: `system`, `storage`, `analytics`, `main`, `config`, integration

See `CONTRACTS.md` for public APIs, invariants and ownership.

## Standards

- ISO C17, standard C library only, console application.
- No external dependencies, no circular header dependencies, no `.c` includes.
- Bounded string handling, `static` module-private helpers, `extern` via
  `config.h`/`config.c` only.
- Atomic save: writes to `<path>.tmp` then renames to `<path>`.
- Safe load: loads into a temporary system; original state is preserved on
  failure. Enum ranges and record counts are validated on load.

## Binary storage format

- Magic: `0x504B4453` ("PKDS"), version: 1
- Fixed-width records; raw pointers never persisted (only stable integer IDs)
- Strings stored as fixed-length buffers
- Next-ID counters preserved so new IDs continue after a load
- Atomic save via temporary file + rename

## Reservation policy

- No overlapping reservations for the same slot (excluding cancelled and
  expired reservations)
- A reservation does not block its own holder from entering
- Expired and cancelled reservations do not block slot allocation
- Reservation-aware entry allocation enforced at the `session_start()`
  boundary via `reservation_is_available()`

## Automated tests

Build and run:

```sh
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude \
    tests/test_system.c src/analytics.c src/billing.c src/config.c \
    src/reservation.c src/session.c src/slot.c src/storage.c \
    src/system.c src/vehicle.c -o tests/test_system
./tests/test_system
```

Tests use temporary file paths and do not overwrite real user records.

## C concepts demonstrated

- Structures and enumerations with sensible member ordering
- Dynamic memory (`malloc`, `calloc`, `realloc`, `free`)
- Dynamic array capacity management (doubling growth strategy)
- `extern` declarations with single definitions in `.c` files
- `static` functions for module-private helpers
- Multiple `.c` files linked together
- Header guards on all public headers
- Bounded string functions (`strcmp`, `strlen`, `memmove`, `snprintf`)
- File I/O (`fopen`, `fread`, `fwrite`, `fclose`, `rename`, `remove`)
- Time functions (`time`, `difftime`, `localtime`, `strftime`)
- Error handling via `Status` return codes

## Known limitations

- `.dat` format uses native `int64_t` representation (not endian-portable)
- `parking` executable and runtime data files are git-ignored
- No test framework beyond the custom `tests/test_system.c` harness
