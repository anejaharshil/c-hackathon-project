# Smart Campus Parking Management System

Sprint 0: shared project skeleton, common structures, public headers, build
configuration and module contracts. No application features implemented yet.

## Build

```sh
make
```

This compiles with:

```sh
gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude src/*.c -o parking
```

Clean with `make clean`.

## Directory layout

- `include/`   public headers (shared model, config, module contracts)
- `src/`       module implementations (minimal stubs that compile)
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
