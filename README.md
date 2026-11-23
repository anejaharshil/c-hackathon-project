# Smart Campus Parking Management System

A compact console application written in ISO C17. It registers students,
staff and visitors, assigns parking slots automatically, records entry and
exit times, prints receipts, and stores all records in a binary `.dat` file.

## Files

```
Codes/main.c        Console interface, input handling and menu navigation
Codes/parking.c     Vehicles, slots, sessions, billing and dynamic arrays
Codes/storage.c     Binary .dat loading, saving and validation
Codes/registration.c   Indian registration and campus ID validation
Header Files/parking.h     Shared structures, enums, constants and declarations
Header Files/registration.h   Validation declarations
data/         Runtime save file (directory kept in version control)
tests/        Automated regression tests
Makefile
README.md
```

## Build and run

```sh
make
./parking
```

`make run` builds and starts the application, creating `data/` if needed.

The compiler flags are:

```sh
gcc -std=c17 -Wall -Wextra -Wpedantic -I. Codes/main.c Codes/parking.c Codes/storage.c Codes/registration.c -o parking
```

Sanitizer builds:

```sh
make asan
./parking_asan
```

## Menu

```
1. Register Vehicle
2. Vehicle Entry
3. Vehicle Exit
4. Parking Status
5. Search Vehicle
6. Statistics
7. View Campus Map
0. Exit
```

Every operation clears the screen, shows only its own prompts, prints its
result (a message, receipt, or table), waits for Enter, and then returns to the
main menu.

## Registration

| Category | Required details |
|----------|------------------|
| Student  | SAP ID (`590000000`–`590099999`), owner name, registration number |
| Teacher  | 9-digit teacher ID, owner name, registration number |
| Visitor  | Owner name, registration number |

Vehicle registration numbers follow the Indian format: exactly 10 characters,
`SS DD LL NNNN`, where `SS` is a two-letter state/union-territory code, `DD` is
a two-digit RTO district code, `LL` is a two-letter series, and `NNNN` is a
four-digit number. For example, `UP16FU5674` is valid (Uttar Pradesh, Noida)
but `UR78HJ7834` is not (`UR` is not an Indian state code).

Input is trimmed and converted to uppercase before validation, so `up16fu5674`
and ` UP16Fu5674 ` are both accepted and stored as `UP16FU5674`.

State codes are validated against a known list. District codes are accepted on
faith (best-effort): RTO district tables change over time and a locally
maintained copy cannot be exhaustive, so an otherwise well-formed plate is not
rejected merely because its district code is absent from the table.

Institutional IDs are unique within their own category. Vehicle registration
numbers are unique across the whole system. Each vehicle also records a type
(car, motorcycle or bicycle).

## Parking and billing

The parking area is divided into three sections — **Teacher Parking**,
**Student Parking** and **Visitor Parking** — each with its own slots for
cars, motorcycles and bicycles. A vehicle is assigned to the section matching
its owner's category (teacher/staff → Teacher Parking, student → Student
Parking, visitor → Visitor Parking) and to a slot matching its vehicle type.
Each section/type combination has its own occupancy counter, shown on the
header and in the parking status screen.

Entry assigns the first free slot in the matching section and vehicle-type
pool and records the current time. A vehicle cannot have two active sessions,
and a slot cannot hold two vehicles. Bicycles have no registration number;
their entry and exit are looked up by owner name. Exit releases the slot and
prints a receipt containing the vehicle, owner, category, slot, entry time,
exit time, duration and charge.

| Category | Charge |
|----------|--------|
| Student  | Free (Rs. 0) |
| Staff    | Free (Rs. 0) |
| Visitor  | Rs. 20 per hour, partial hours rounded up |

Billing uses integer arithmetic only. A positive visit of up to one hour costs
Rs. 20; each additional started hour costs another Rs. 20. A zero-length visit
costs Rs. 0. There is no daily cap. An exit timestamp before the entry
timestamp is rejected and the session remains active.

## Persistence

State is loaded automatically at startup from `data/parking.dat`. Every
state-changing operation — registration, entry, exit — is saved to disk
immediately, so data survives a crash or forced termination. The exit save is
confirmed before the application reports success. If a save fails, the user is
warned that changes may be lost. The binary format is:

- magic `0x504B4453`, version `2`
- fixed-width integers and fixed-length string fields
- stable IDs, no pointers
- atomic replacement through a temporary file and `rename`

The older version 1 file stored reservations and four separate ID counters.
It cannot be converted without losing information, so it is not loaded. If an
incompatible or corrupt file is found, the application explains the problem,
renames the file to `data/parking.dat.bak` (or `.bak.1`, `.bak.2`, ...), and
starts with an empty database.

## Campus Map

Option 7 opens a top-down campus masterplan rendered in the terminal. The map
shows the three parking lots geographically separated across campus:

- Teacher Parking (east)
- Student Parking (west)
- Visitor Parking (south)

Each lot contains three vehicle-specific sections — Car, Motorcycle and
Bicycle — for nine parking sections in total. Every bay corresponds to a real
`ParkingSlot` record and is labelled with its slot id (for example `C16` for
student car slot 16).

Available bays are shown empty; occupied bays are shown filled. The occupancy
reflects the live in-memory state, so entering or exiting a vehicle updates the
map immediately. Records persist across restarts through `data/parking.dat`.

On terminals narrower than the map, the layout still renders but may wrap; the
full campus remains visible via the main map view.

## Tests

```sh
make test
make test-asan
```

The suite covers registration, duplicate rejection, institutional ID rules,
automatic allocation, duplicate entry, full capacity, free student and staff
parking, visitor billing at 30 and 90 minutes, slot release, dynamic array
growth, save/restart, post-restart operations, revenue persistence, corrupt and
truncated files, trailing bytes, cross-record validation, save failures, backup
fallback, and old-format rejection.

## C concepts demonstrated

Structures, enums, pointers, `malloc`/`calloc`/`realloc`/`free`, dynamic
arrays, structure member ordering, header files, multiple translation units,
`static` helpers, string handling, `time.h`, `fopen`/`fread`/`fwrite`/`rename`,
loops, conditionals, `switch`, and modular design.

## Limitations

- The save format uses native byte order and is not portable between machines
  with different endianness.
- Registrations are normalized to uppercase, so letter case does not matter.
- A visitor stay whose recorded exit is not after its entry is treated as a
  zero-length stay and costs Rs. 0.
- Slots are divided equally among 3 sections x 3 vehicle types = 9 pools, so
  the total slot count must be at least 9 (default 45).
