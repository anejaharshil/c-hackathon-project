#ifndef SYSTEM_H
#define SYSTEM_H

#include "common.h"
#include "vehicle.h"
#include "slot.h"
#include "session.h"
#include "reservation.h"

/* ParkingSystem owns all dynamically allocated record arrays, their counts and
 * capacities, and the next-ID counters used to issue stable integer IDs.
 *
 * Tagged struct so `ParkingSystem` and `struct ParkingSystem` are the same
 * type; model headers forward-declare `struct ParkingSystem` only. */
typedef struct ParkingSystem ParkingSystem;

struct ParkingSystem {
    Vehicle *vehicles;
    size_t vehicle_count;
    size_t vehicle_capacity;

    ParkingSlot *slots;
    size_t slot_count;
    size_t slot_capacity;

    ParkingSession *sessions;
    size_t session_count;
    size_t session_capacity;

    Reservation *reservations;
    size_t reservation_count;
    size_t reservation_capacity;

    Id next_vehicle_id;
    Id next_slot_id;
    Id next_session_id;
    Id next_reservation_id;
};

/* Developer 3 owns: main, storage, analytics, configuration, integration. */

/* Create and initialize an empty ParkingSystem. Caller must call system_destroy. */
Status system_create(ParkingSystem **out_sys);

/* Destroy a ParkingSystem and free all owned memory. Safe to call with NULL. */
void system_destroy(ParkingSystem *sys);

/* Reset all in-memory records to empty (keeps allocated capacity). */
Status system_reset(ParkingSystem *sys);

/* Allocate a fresh stable ID of the given kind. */
Id system_next_id(ParkingSystem *sys, const char *kind);

#endif /* SYSTEM_H */
