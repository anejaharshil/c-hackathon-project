# Smart Campus Parking Management System

Sprint 1: core console interface, storage persistence and system integration.
Sprint 0 delivered the shared skeleton; Sprint 1 adds the menu-driven console
application, atomic binary persistence with load validation, and safe load
semantics that preserve in-memory data on failure.

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
7. Save Data
0. Save and Exit
```

All input uses `fgets()` with validated numeric conversion. Empty strings,
non-numeric input, and EOF are handled gracefully.

## Directory layout

- `include/`   public headers (shared model, config, module contracts)
- `src/`       module implementations (console app + all modules)
- `data/`      binary `.dat` persistence files (runtime)
- `reports/`   generated reports (runtime)
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
