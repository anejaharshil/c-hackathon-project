# Indian Registration and Campus ID Validation

## Intent

Make vehicle registration numbers follow the real Indian format, and replace the
free-form student and staff IDs with the campus rules:

- Students use a SAP ID from `590000000` to `590099999`.
- Teachers/staff use any 9-digit ID.
- Visitors still have no institutional ID.

## Registration rule

A registration is exactly 10 characters:

```
SS DD LL NNNN
```

- `SS` — two-letter state or union-territory code.
- `DD` — two-digit RTO district code, valid for that state.
- `LL` — two-letter series, `A`–`Z`.
- `NNNN` — four digits, `0`–`9`.

Examples:

- `UP16FU5674` — valid: Uttar Pradesh, Noida (Gautam Buddha Nagar).
- `UK07CC1234` — valid: Uttarakhand, Nainital.
- `UR78HJ7834` — invalid: `UR` is not an Indian state code.
- `UP99AB1234` — invalid: `UP99` is not an assigned RTO code.

Input is trimmed and converted to uppercase before validation. The stored and
displayed value is uppercase. Series letters are not restricted beyond `A`–`Z`;
the four-digit number is not restricted beyond digits.

## ID rules

| Category | ID | Rule |
|---|---|---|
| Student | SAP ID | Exactly 9 digits, value `590000000`–`590099999` |
| Teacher/staff | Teacher ID | Exactly 9 digits, any value |
| Visitor | None | No ID |

Institutional IDs remain unique within their category. A SAP ID and a teacher ID
may have the same digits because they are in different categories.

## Approach

Add a separate module:

- `registration.h` — public declarations.
- `registration.c` — pattern checks, SAP/teacher checks, and the RTO table.

A header-only table would be duplicated in every translation unit that includes
it, so the implementation lives in the `.c` file.

Public functions:

```c
int registration_is_valid(const char *text);
int registration_normalize(char *out, size_t size, const char *text);
int sap_id_is_valid(const char *text);
int teacher_id_is_valid(const char *text);
```

The RTO table is a compact per-state list of assigned two-digit district codes,
covering every state and union territory, including the current and legacy
codes that still appear on vehicles (`TS`/`TG`, `DN`/`DD`, `AN`). The validator
looks up the four-character state/district prefix.

`parking_register` normalizes the registration, validates the category-specific
ID, and returns specific statuses for an invalid registration and an invalid ID.
`parking_find_vehicle` normalizes a valid query, so registration, entry, exit,
and search all accept lowercase input. An invalid legacy string still matches
exactly, so existing saved records remain reachable.

## Code layout

The project stays multi-file. Each `.c` file follows the same section order:

```c
// Header Files

// Global Variables

// Macros

// Function Declarations

// main function        (main.c only)

// Functions
```

- File-scope `static` data, including the RTO table in `registration.c`, goes
  under Global Variables.
- Every function gets a declaration before use; definitions follow the
  `main()` section.
- `parking.c`, `storage.c`, and `registration.c` have no `main()` section.
- Section banners use `//` as shown.

## UI

- Registration prompt: `Vehicle registration (e.g. UP16AB1234):`
- Student prompt: `SAP ID (590000000-590099999):`
- Staff prompt: `Teacher ID (9 digits):`
- The staff category is displayed as `Teacher` in the registration flow and
  receipts; the internal enum remains `USER_STAFF`.
- Invalid registration and invalid ID get their own concise messages.

## Persistence and compatibility

The binary format and version stay unchanged. Existing records are loaded as-is
rather than rejected, because the format change applies to new registrations.
New registrations are always validated.

## Tests

Replace placeholder test plates such as `STU001` with real plates. Add cases for:

- valid state/district combinations, including `UP16` and `UK07`;
- invalid state (`UR78`), invalid district (`UP99`), wrong length, wrong
  character classes, and whitespace;
- lowercase input normalized to uppercase for registration and lookup;
- SAP boundaries (`590000000`, `590099999`) and rejection just outside them;
- teacher IDs with 9 digits, including leading zeros, and rejection of 8 or 10
  digits or non-digits;
- duplicate and cross-category ID behavior.

## Non-goals

- No migration of old records.
- No binary format or version change.
- No restriction on series letters beyond `A`–`Z`.
- No check that a registration is currently assigned to a specific vehicle; the
  validator checks the state, RTO district, and pattern only.
