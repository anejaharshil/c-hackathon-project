# Smart Campus Parking Management System

Sprint 3: console extensions, analytics and CSV reporting.
Sprint 0 delivered the shared skeleton; Sprint 1 added the menu-driven console
application and atomic binary persistence; Sprint 2 added reservation menu
operations, improved analytics, CSV export, and corruption-safe loading.

## Build

```sh
make
```

This compiles with:

```sh
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o parking
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
- Parking status and statistics (capacity, occupancy %, zone utilisation,
  active/completed sessions, revenue, average duration, vehicle categories)
- Reservation create/view/cancel/status using Developer 2's reservation API
- CSV export of completed parking sessions to `reports/completed_sessions.csv`
- Binary `.dat` persistence with atomic save and corruption-safe loading
- Missing save file = legitimate first run; corrupted file = explicit error,
  original data preserved

## Directory layout

- `include/`   public headers (shared model, config, module contracts)
- `src/`       module implementations (console app + all modules)
- `data/`      binary `.dat` persistence files (runtime)
- `reports/`   generated CSV reports (runtime)
- `tests/`     unit tests (to be added by developers)

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

## Integration notes

- Reservations are saved and restored correctly through `storage_save()`
  / `storage_load()`.
- Reservation-aware entry allocation (checking reservations before assigning
  a slot on vehicle entry) is not yet supported by public interfaces;
  reservations do not currently prevent vehicle entry. Reported as an
  integration dependency for Developer 1/2 coordination.
- CSV export uses `billing_compute()` as the authoritative fee calculation.
