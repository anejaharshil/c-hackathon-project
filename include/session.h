#ifndef SESSION_H
#define SESSION_H

#include "common.h"
#include <time.h>

struct ParkingSystem; /* forward declaration; defined in system.h */

/* A parking session links a vehicle to a slot for a time span.
 * Completed sessions remain in history. */
typedef struct {
    Id id;
    Id vehicle_id;
    Id slot_id;
    time_t start_time;
    time_t end_time;     /* 0 while active */
    int active;          /* 1 = ongoing, 0 = completed */
} ParkingSession;

/* Developer 1 owns: vehicle and parking (session) module. */

/* Start a new session for a vehicle in a slot. Returns STATUS_CONFLICT if the
 * vehicle already has an active session or the slot is occupied. */
Status session_start(struct ParkingSystem *sys, Id vehicle_id, Id slot_id,
                     time_t start_time, Id *out_id);

/* End the active session for a vehicle, setting end_time and marking inactive.
 * Returns STATUS_NOT_FOUND if no active session exists for the vehicle. */
Status session_end(struct ParkingSystem *sys, Id vehicle_id, time_t end_time);

/* Find the active session for a vehicle. Sets out_index.
 * Returns STATUS_NOT_FOUND if none is active. */
Status session_find_active_for_vehicle(const struct ParkingSystem *sys,
                                       Id vehicle_id, size_t *out_index);

/* Find a session by ID. Sets out_index. */
Status session_find_by_id(const struct ParkingSystem *sys, Id id,
                          size_t *out_index);

/* Get read-only access to the session at index. Returns NULL if out of bounds. */
const ParkingSession *session_get(const struct ParkingSystem *sys, size_t index);

size_t session_count(const struct ParkingSystem *sys);

#endif /* SESSION_H */
