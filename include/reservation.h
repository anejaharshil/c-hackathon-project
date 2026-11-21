#ifndef RESERVATION_H
#define RESERVATION_H

#include "common.h"
#include <time.h>

struct ParkingSystem; /* forward declaration; defined in system.h */

/* A reservation holds a slot for a vehicle during a time window.
 * No overlapping reservations for the same slot are allowed. */
typedef struct {
    Id id;
    Id vehicle_id;
    Id slot_id;
    time_t start_time;
    time_t end_time;
    ReservationStatus status;
} Reservation;

/* Developer 2 owns: billing and reservations module. */

/* Create a reservation. Returns STATUS_CONFLICT if it overlaps an existing
 * reservation for the same slot, or STATUS_NOT_FOUND if slot/vehicle missing. */
Status reservation_create(struct ParkingSystem *sys, Id vehicle_id, Id slot_id,
                          time_t start_time, time_t end_time, Id *out_id);

/* Cancel a reservation by ID. Returns STATUS_NOT_FOUND if absent. */
Status reservation_cancel(struct ParkingSystem *sys, Id id);

/* Complete a reservation by ID (mark COMPLETED). Returns STATUS_NOT_FOUND if absent. */
Status reservation_complete(struct ParkingSystem *sys, Id id);

/* Find a reservation by ID. Sets out_index. */
Status reservation_find_by_id(const struct ParkingSystem *sys, Id id,
                              size_t *out_index);

/* Get read-only access to the reservation at index. Returns NULL if out of bounds. */
const Reservation *reservation_get(const struct ParkingSystem *sys, size_t index);

size_t reservation_count(const struct ParkingSystem *sys);

#endif /* RESERVATION_H */
