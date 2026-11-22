# Indian Registration Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enforce real Indian vehicle registrations and campus ID rules, and reorganize each `.c` file into the agreed section order.

**Architecture:** Add a `registration.c` / `registration.h` module that owns the 10-character plate pattern, the state/RTO table, and the SAP/teacher ID checks. `parking.c` calls it when registering and when looking up a vehicle; the UI and storage layers stay thin.

**Tech Stack:** ISO C17, standard library only, existing Makefile, binary save format version 2.

**Spec:** `docs/superpowers/specs/2026-11-22-indian-registration-validation-design.md`

## Global Constraints

- Build with `gcc -std=c17 -Wall -Wextra -Wpedantic -I.`; no external libraries.
- Keep the multi-file split: `main.c`, `parking.c`, `storage.c`, `registration.c`, plus headers.
- Each `.c` file uses this section order: Header Files, Global Variables, Macros, Function Declarations, `main()` (only `main.c`), Functions.
- Registration is exactly 10 characters: `[A-Z]{2}[0-9]{2}[A-Z]{2}[0-9]{4}`.
- The four-character prefix must be a real state/RTO district pair. `UP16` and `UK07` are valid; `UR78` and `UP99` are not.
- Student ID: exactly 9 digits, value `590000000`–`590099999`.
- Teacher/staff ID: exactly 9 digits, any value.
- Visitors have no institutional ID.
- Input is trimmed and uppercased before validation. Stored registrations are uppercase.
- Save format and version stay at `0x504B4453` / `2`.
- Existing saved records load as-is; new registrations are strict.

## Review Focus

- Lowercase and surrounding whitespace in registration and lookup — must normalize or reject predictably.
- Delhi and other source lists that use one-digit RTO codes — `DL01` must be accepted, not only `DL1`.
- Legacy but still-seen state codes — `TS`/`TG`, `DN`/`DD`, and `AN` must work.
- SAP boundaries and teacher IDs with leading zeros — `590000000`, `590099999`, `000000000` must be accepted; one digit outside must not.
- Existing records with old-format registrations — exact legacy lookup must still work after normalization is added.
- Duplicate detection after normalization — `up16aa0001` and `UP16AA0001` must collide.

---

### Task 1: Add the registration validation module

**Files:**
- Create: `registration.h`
- Create: `registration.c`
- Modify: `Makefile`
- Modify: `tests/test_parking_system.c`

**Interfaces:**
- Produces:
  - `int registration_is_valid(const char *text);`
  - `int registration_normalize(char *out, size_t size, const char *text);`
  - `int sap_id_is_valid(const char *text);`
  - `int teacher_id_is_valid(const char *text);`

- [ ] **Step 1: Write the failing validation tests**

Add `#include "registration.h"` to `tests/test_parking_system.c`, then add:

```c
static void test_registration_validation(void) {
    CHECK(registration_is_valid("UP16FU5674"),
          "UP16 Noida registration is valid");
    CHECK(registration_is_valid("UK07CC1234"),
          "UK07 Uttarakhand registration is valid");
    CHECK(registration_is_valid("DL01AB1234"),
          "DL01 Delhi registration is valid");
    CHECK(registration_is_valid("MH01AA0001"),
          "MH01 Mumbai registration is valid");
    CHECK(registration_is_valid("KA01AB1234"),
          "KA01 Bengaluru registration is valid");
    CHECK(registration_is_valid("TN01AB1234"),
          "TN01 Chennai registration is valid");
    CHECK(registration_is_valid("AP39AB1234"),
          "AP39 Andhra Pradesh registration is valid");
    CHECK(registration_is_valid("TS09AB1234"),
          "TS09 Telangana registration is valid");
    CHECK(registration_is_valid("AN01AB1234"),
          "AN01 Andaman registration is valid");
    CHECK(!registration_is_valid("UR78HJ7834"),
          "unknown state code is rejected");
    CHECK(!registration_is_valid("UP99AB1234"),
          "unknown RTO district is rejected");
    CHECK(!registration_is_valid("UP16FU567"),
          "nine-character registration is rejected");
    CHECK(!registration_is_valid("UP16FU56745"),
          "eleven-character registration is rejected");
    CHECK(!registration_is_valid("UP16F15674"),
          "digit in the series is rejected");
    CHECK(!registration_is_valid("UP16FU56A4"),
          "letter in the number is rejected");
    CHECK(!registration_is_valid("UP16 FU5674"),
          "space inside the registration is rejected");
    CHECK(!registration_is_valid(""),
          "empty registration is rejected");
    CHECK(!registration_is_valid(NULL),
          "null registration is rejected");

    char normalized[16];
    CHECK(registration_normalize(normalized, sizeof normalized,
                                 "up16fu5674") &&
              strcmp(normalized, "UP16FU5674") == 0,
          "lowercase registration is normalized to uppercase");
    CHECK(registration_normalize(normalized, sizeof normalized,
                                 " Up16Fu5674 ") &&
              strcmp(normalized, "UP16FU5674") == 0,
          "surrounding spaces are trimmed during normalization");
    CHECK(!registration_normalize(normalized, sizeof normalized,
                                  "UR78HJ7834"),
          "invalid registration is not normalized");
}

static void test_id_validation(void) {
    CHECK(sap_id_is_valid("590000000"), "lowest SAP ID is valid");
    CHECK(sap_id_is_valid("590099999"), "highest SAP ID is valid");
    CHECK(!sap_id_is_valid("590100000"), "SAP ID above range is rejected");
    CHECK(!sap_id_is_valid("589999999"), "SAP ID below range is rejected");
    CHECK(!sap_id_is_valid("59000000"), "eight-digit SAP ID is rejected");
    CHECK(!sap_id_is_valid("5900000000"), "ten-digit SAP ID is rejected");
    CHECK(!sap_id_is_valid("59000000A"), "non-numeric SAP ID is rejected");
    CHECK(!sap_id_is_valid(""), "empty SAP ID is rejected");
    CHECK(!sap_id_is_valid(NULL), "null SAP ID is rejected");

    CHECK(teacher_id_is_valid("123456789"), "nine-digit teacher ID is valid");
    CHECK(teacher_id_is_valid("000000000"),
          "teacher ID with leading zeros is valid");
    CHECK(!teacher_id_is_valid("12345678"), "eight-digit teacher ID is rejected");
    CHECK(!teacher_id_is_valid("1234567890"),
          "ten-digit teacher ID is rejected");
    CHECK(!teacher_id_is_valid("12345678A"),
          "non-numeric teacher ID is rejected");
    CHECK(!teacher_id_is_valid(""), "empty teacher ID is rejected");
    CHECK(!teacher_id_is_valid(NULL), "null teacher ID is rejected");
}
```

Call both functions from `main()` before the other tests.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make test`
Expected: compile/link failure because `registration.h` and the four functions do not exist.

- [ ] **Step 3: Create `registration.h`**

```c
#ifndef REGISTRATION_H
#define REGISTRATION_H

#include <stddef.h>

int registration_is_valid(const char *text);
int registration_normalize(char *out, size_t size, const char *text);
int sap_id_is_valid(const char *text);
int teacher_id_is_valid(const char *text);

#endif /* REGISTRATION_H */
```

- [ ] **Step 4: Create `registration.c` with the section layout and RTO table**

Use this structure:

```c
// Header Files
#include "registration.h"
#include <ctype.h>
#include <string.h>

// Global Variables
typedef struct {
    const char *state;
    const char *districts;
} RtoState;

static const RtoState RTO_STATES[] = {
    { "AN", "01 02" },
    { "AP", "01 02 03 ... 39 40" },
    { "AR", "01 02 ..." },
    /* one entry for every state and union territory */
    { "UP", "11 12 13 ... 96" },
    { "UK", "01 02 ... 20" },
    { "WB", "01 02 ..." },
};

// Macros
#define REGISTRATION_LENGTH 10
#define SAP_MIN 590000000L
#define SAP_MAX 590099999L

// Function Declarations
static const char *districts_for(const char *state);
static int district_is_listed(const char *districts, const char *code);

// Functions
/* definitions */
```

The district lists must come from the public RTO district list and be
normalized to two digits. Include `TS` and `TG` with the Telangana list, `DN`
and `DD` with the merged UT list, and `AN` with `01 02`. Verify `UP16`, `UK07`,
`DL01`, `MH01`, `KA01`, `TN01`, `AP39`, `TS09`, `AN01`, `DD01` are present and
`UP99` is absent.

`registration_normalize` must trim leading and trailing whitespace, reject any
length other than 10, uppercase letters with `toupper((unsigned char)c)`, and
then call `registration_is_valid`. It must NUL-terminate `out` and return 0
without writing a partial valid result on failure. Return 0 if `out` is NULL,
`text` is NULL, or `size` is less than 11. Use `(unsigned char)` casts with
`isupper`, `isdigit` and `toupper`.

`registration_is_valid` checks the exact character classes, builds a
four-character prefix, finds the state entry, and checks the district list.
`sap_id_is_valid` checks nine digits and the numeric range.
`teacher_id_is_valid` checks exactly nine digits.

- [ ] **Step 5: Add `registration.c` to every build**

In `Makefile`:

```make
SOURCES = main.c parking.c storage.c registration.c
$(TEST): tests/test_parking_system.c parking.c storage.c registration.c \
         parking.h registration.h
	$(CC) $(CFLAGS) tests/test_parking_system.c parking.c storage.c \
		registration.c -o $@
```

Add `registration.c` to the `asan` and `test-asan` commands as well.

- [ ] **Step 6: Run the validation tests**

Run: `make test`
Expected: all tests pass, including the new validation tests.

- [ ] **Step 7: Commit**

```bash
git add registration.h registration.c Makefile tests/test_parking_system.c
git commit -m "feat: add Indian registration and campus ID validation"
```

---

### Task 2: Enforce validation in parking registration and lookup

**Files:**
- Modify: `parking.h`
- Modify: `parking.c`
- Modify: `tests/test_parking_system.c`

**Interfaces:**
- Consumes: `registration_normalize`, `sap_id_is_valid`, `teacher_id_is_valid` from Task 1.
- Produces: `PARKING_INVALID_REGISTRATION`, `PARKING_INVALID_ID` in `ParkingStatus`.

- [ ] **Step 1: Write the failing integration tests**

Replace the placeholder plates in `test_registration`, `test_entry_and_exit`,
`test_capacity`, `test_growth`, `test_persistence`,
`test_revenue_persistence`, `test_corruption`, `test_boundaries`,
`test_persistence_failures`, and `test_inconsistent_state` using this mapping:

| Old | New | ID |
|---|---|---|
| `STU001` | `UP16AA0001` | `590000001` |
| `STF001` | `UP16AA0002` | `123456789` |
| `VIS001` | `UP16AA0003` | none |
| `STU002` | `UP16AA0004` | `590000001` |
| `STF002` | `UP16AA0005` | `590000001` |
| `BAD001` | `UP16AA0006` | `590000004` |
| `BAD002` | `UP16AA0007` | `590000005` |
| `CAR001` | `UP16AA0010` | `590000010` |
| `CAR002` | `UP16AA0011` | `590000011` |
| `CAR003` | `UP16AA0012` | `590000012` |
| `SAFE01` | `UP16AA0030` | `590000030` |
| `OK001` | `UP16AA0040` | `590000040` |
| `OK002` | `UP16AA0041` | `590000041` |
| `FREE01` | `UP16AA0042` | `590000042` |
| `FREE02` | `UP16AA0043` | `590000043` |
| `LIMIT1` | `UP16AA0044` | `590000044` |
| `FAIL01` | `UP16AA0050` | `590000050` |
| `REF001` | `UP16AA0060` | `590000060` |
| `REF002` | `UP16AA0061` | `590000061` |

For `test_growth`, generate `UP16AA%04d` for `i = 0..199` and SAP IDs
`590000000 + i`. For persistence, use `UP16AA0020` for the added visitor.
Replace the unknown lookup strings with `UP16ZZ9999` (valid shape, unregistered).

The duplicate-registration test needs a different SAP ID from the original
record, so use `590000002` for the `UP16AA0001` duplicate attempt. Change the
existing empty-registration and overlong-registration expectations to
`PARKING_INVALID_REGISTRATION`, and the missing-student-ID expectation to
`PARKING_INVALID_ID`.

Add a legacy-lookup test:

```c
static void test_legacy_lookup(void) {
    ParkingSystem *sys = make_system(2);
    add_vehicle(sys, "UP16AA0080", USER_STUDENT, "590000080");
    snprintf(sys->vehicles[0].registration,
             sizeof sys->vehicles[0].registration, "%s", "STU001");
    CHECK(parking_find_vehicle(sys, "STU001") != NULL,
          "legacy record remains reachable by exact match");
    parking_destroy(sys);
}
```

Call it from `main()`.

Add these assertions to `test_registration`:

```c
CHECK(parking_register(sys, "up16aa0070", "Lower", "590000070",
                       USER_STUDENT, VEHICLE_CAR, NULL) == PARKING_OK,
      "lowercase registration is accepted");
CHECK(parking_find_vehicle(sys, "up16aa0070") != NULL,
      "lowercase lookup finds an uppercase stored registration");
CHECK(parking_register(sys, "UP16AA0070", "Duplicate", "590000071",
                       USER_STUDENT, VEHICLE_CAR, NULL) == PARKING_DUPLICATE,
      "normalized duplicate registration is rejected");
CHECK(parking_register(sys, "UR78HJ7834", "Bad", "590000072",
                       USER_STUDENT, VEHICLE_CAR, NULL) ==
          PARKING_INVALID_REGISTRATION,
      "invalid state code is rejected at registration");
CHECK(parking_register(sys, "UP99AB1234", "Bad", "590000073",
                       USER_STUDENT, VEHICLE_CAR, NULL) ==
          PARKING_INVALID_REGISTRATION,
      "invalid RTO district is rejected at registration");
CHECK(parking_register(sys, "UP16AA0074", "Bad", "590100000",
                       USER_STUDENT, VEHICLE_CAR, NULL) ==
          PARKING_INVALID_ID,
      "SAP ID outside the allowed range is rejected");
CHECK(parking_register(sys, "UP16AA0075", "Bad", "12345678",
                       USER_STAFF, VEHICLE_CAR, NULL) ==
          PARKING_INVALID_ID,
      "eight-digit teacher ID is rejected");
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `make test`
Expected: failures because the new statuses do not exist and the old fixtures no longer match.

- [ ] **Step 3: Add the statuses and validation in `parking.h` / `parking.c`**

Add to `ParkingStatus`:

```c
PARKING_INVALID_REGISTRATION,
PARKING_INVALID_ID
```

In `parking_register`, before duplicate checks:

- Normalize the registration with `registration_normalize`.
- If that fails, return `PARKING_INVALID_REGISTRATION`.
- For students, require `sap_id_is_valid`; for staff, require
  `teacher_id_is_valid`; otherwise return `PARKING_INVALID_ID`.
- Use the normalized registration for `registration_taken` and the stored
  `registration` field.

In `parking_find_vehicle`, normalize a valid query and compare against the
stored value. If normalization fails, fall back to the raw query so exact
legacy records remain findable.

Keep the existing owner, category, vehicle-type and visitor checks. The order
must be: basic null/owner/category/type checks, registration validation, ID
validation, duplicate checks, limits.

- [ ] **Step 4: Run the tests**

Run: `make test`
Expected: all tests pass.

- [ ] **Step 5: Commit**

```bash
git add parking.h parking.c tests/test_parking_system.c
git commit -m "feat: validate registrations and campus IDs on registration"
```

---

### Task 3: Update the console UI and documentation

**Files:**
- Modify: `main.c`
- Modify: `README.md`

**Interfaces:**
- Consumes: `PARKING_INVALID_REGISTRATION`, `PARKING_INVALID_ID`.

- [ ] **Step 1: Update the prompts and messages**

In `main.c`:

- Registration prompt: `Vehicle registration (e.g. UP16AB1234): `
- Student prompt: `SAP ID (590000000-590099999): `
- Staff prompt: `Teacher ID (9 digits): `
- Menu category: `1 Student, 2 Teacher, 3 Visitor`
- Add `status_message` cases:
  - `PARKING_INVALID_REGISTRATION`: `Invalid registration. Use the format UP16AB1234 with a valid state and RTO code.`
  - `PARKING_INVALID_ID`: `Invalid ID. Students need a 9-digit SAP ID (590000000-590099999); teachers need a 9-digit ID.`

Change `parking_user_type_name(USER_STAFF)` to return `Teacher` so the menu,
status table, search result and receipt agree. Keep the enum name `USER_STAFF`.

- [ ] **Step 2: Update the README**

Add `registration.c` and `registration.h` to the file list. Replace the
registration table with:

| Category | Required details |
|---|---|
| Student | SAP ID (`590000000`–`590099999`), owner name, registration number |
| Teacher | 9-digit teacher ID, owner name, registration number |
| Visitor | Owner name, registration number |

Document the 10-character plate format, the real state/RTO rule, uppercase
normalization, and the fact that old records load as-is. Remove the
"letter case matters" limitation.

- [ ] **Step 3: Build and run a manual smoke test**

Run: `make`
Run: `./parking`

Register a student with `up16fu5674`, SAP `590000001`; verify it is stored as
`UP16FU5674`, enter it with lowercase, exit it, and search for it. Register a
visitor with `UR78HJ7834` and verify the registration is rejected. Register a
student with SAP `590100000` and verify the ID is rejected.

- [ ] **Step 4: Commit**

```bash
git add main.c README.md
git commit -m "docs: document Indian registration and campus ID rules"
```

---

### Task 4: Apply the section layout to every `.c` file

**Files:**
- Modify: `main.c`
- Modify: `parking.c`
- Modify: `storage.c`
- Modify: `registration.c`

**Interfaces:**
- No behavior changes. All existing tests must keep passing.

- [ ] **Step 1: Reorganize `main.c`**

Order:

```c
// Header Files
#include "parking.h"
#include <ctype.h>
...

// Global Variables
static int saving_enabled = 1;

// Macros
#define TEXT_SIZE 32
...

// Function Declarations
static void clear_screen(void);
static int read_line(...);
...

// main function
int main(void) { ... }

// Functions
/* all static function definitions */
```

Move `main()` above the static definitions and add every prototype it needs.

- [ ] **Step 2: Reorganize `parking.c` and `storage.c`**

Use the same order. Put `INITIAL_CAPACITY` and storage constants under Macros,
static helper prototypes under Function Declarations, and all definitions under
Functions. There is no `main()` section in these files.

- [ ] **Step 3: Reorganize `registration.c`**

Keep the RTO table under Global Variables, constants under Macros, static
helper prototypes under Function Declarations, and the four public functions
under Functions.

- [ ] **Step 4: Run the full suite**

Run: `make test`
Expected: all tests pass, unchanged.

- [ ] **Step 5: Commit**

```bash
git add main.c parking.c storage.c registration.c
git commit -m "refactor: apply consistent section layout to C files"
```

---

### Task 5: Final verification

**Files:**
- No source changes unless verification finds a defect.

- [ ] **Step 1: Strict build**

Run: `make clean && make`
Expected: no warnings.

- [ ] **Step 2: Tests**

Run: `make test`
Expected: all tests pass.

- [ ] **Step 3: Sanitizers**

Run: `make test-asan`
Expected: all tests pass with no ASan/UBSan output.

- [ ] **Step 4: Static analyzer**

Run:

```bash
gcc -std=c17 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. \
    main.c parking.c storage.c registration.c -o /tmp/parking_analyzer
```

Expected: clean.

- [ ] **Step 5: Manual restart and legacy checks**

Run the app, register/enter/exit a vehicle, restart, and verify the record
persists. Verify `up16fu5674` works for entry, exit and search. Verify
`UR78HJ7834` and `UP99AB1234` are rejected. Verify a teacher ID with leading
zeros is accepted.

- [ ] **Step 6: Commit any verification fix**

If a fix was needed:

```bash
git add -A
git commit -m "fix: resolve verification findings"
```

Otherwise, no commit is needed.
