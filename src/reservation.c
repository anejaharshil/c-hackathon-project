#include "reservation.h"
#include "system.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Returns 1 if [a_start, a_end) overlaps [b_start, b_end). */
static int overlaps(time_t a_start, time_t a_end, time_t b_start, time_t b_end) {
    return a_start < b_end && b_start < a_end;
}

/* Returns 1 if a PENDING reservation has passed its end_time.
 * Status is not changed; expiry is evaluated on the fly. */
static int is_expired(const Reservation *r, time_t now) {
    return r->status == RESERVATION_PENDING && now > r->end_time;
}

/* Returns 1 if an active session exists on slot_id that started before end_time,
 * meaning it may still occupy the slot during [start_time, end_time). */
static int slot_has_active_session(const ParkingSystem *sys, Id slot_id,
                                   time_t end_time) {
    for (size_t i = 0; i < sys->session_count; i++) {
        const ParkingSession *s = &sys->sessions[i];
        if (s->active && s->slot_id == slot_id && s->start_time < end_time) {
            return 1;
        }
    }
    return 0;
}

Status reservation_create(ParkingSystem *sys, Id vehicle_id, Id slot_id,
                          time_t start_time, time_t end_time, Id *out_id) {
    if (sys == NULL || out_id == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (start_time < 0 || end_time < 0 || end_time <= start_time) {
        return STATUS_INVALID_ARGUMENT;
    }

    time_t now = time(NULL);

    /* Validate vehicle and slot exist. */
    size_t dummy = 0;
    if (vehicle_find_by_id(sys, vehicle_id, &dummy) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }
    if (slot_find_by_id(sys, slot_id, &dummy) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }

    /* No overlapping reservations for the same slot (excluding cancelled and
     * expired reservations; an expired reservation no longer holds the slot). */
    for (size_t i = 0; i < sys->reservation_count; i++) {
        Reservation *r = &sys->reservations[i];
        if (r->slot_id == slot_id && r->status != RESERVATION_CANCELLED &&
            !is_expired(r, now) &&
            overlaps(start_time, end_time, r->start_time, r->end_time)) {
            return STATUS_CONFLICT;
        }
    }

    /* An active session on the slot may still occupy it during the proposed
     * window (its end time is unknown). Reject to avoid conflict. */
    if (slot_has_active_session(sys, slot_id, end_time)) {
        return STATUS_CONFLICT;
    }

    /* Grow the dynamic reservation array safely. */
    if (sys->reservation_count == sys->reservation_capacity) {
        size_t new_cap = sys->reservation_capacity == 0 ? 8
                                                        : sys->reservation_capacity * 2;
        if (new_cap > (size_t)-1 / sizeof(Reservation)) {
            return STATUS_OUT_OF_MEMORY;
        }
        Reservation *tmp = realloc(sys->reservations,
                                   new_cap * sizeof(Reservation));
        if (tmp == NULL) {
            return STATUS_OUT_OF_MEMORY;
        }
        sys->reservations = tmp;
        sys->reservation_capacity = new_cap;
    }

    Reservation *r = &sys->reservations[sys->reservation_count];
    r->id = system_next_id(sys, "reservation");
    r->vehicle_id = vehicle_id;
    r->slot_id = slot_id;
    r->start_time = start_time;
    r->end_time = end_time;
    r->status = RESERVATION_PENDING;

    *out_id = r->id;
    sys->reservation_count++;
    return STATUS_OK;
}

Status reservation_cancel(ParkingSystem *sys, Id id) {
    if (sys == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t index = 0;
    Status st = reservation_find_by_id(sys, id, &index);
    if (st != STATUS_OK) {
        return st;
    }
    Reservation *r = &sys->reservations[index];
    if (r->status == RESERVATION_CANCELLED ||
        r->status == RESERVATION_COMPLETED) {
        return STATUS_CONFLICT;
    }
    r->status = RESERVATION_CANCELLED;
    return STATUS_OK;
}

Status reservation_complete(ParkingSystem *sys, Id id) {
    if (sys == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t index = 0;
    Status st = reservation_find_by_id(sys, id, &index);
    if (st != STATUS_OK) {
        return st;
    }
    Reservation *r = &sys->reservations[index];
    if (r->status == RESERVATION_COMPLETED ||
        r->status == RESERVATION_CANCELLED) {
        return STATUS_CONFLICT;
    }
    r->status = RESERVATION_COMPLETED;
    return STATUS_OK;
}

Status reservation_find_by_id(const ParkingSystem *sys, Id id,
                              size_t *out_index) {
    if (sys == NULL || out_index == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < sys->reservation_count; i++) {
        if (sys->reservations[i].id == id) {
            *out_index = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

const Reservation *reservation_get(const ParkingSystem *sys, size_t index) {
    if (sys == NULL || index >= sys->reservation_count) {
        return NULL;
    }
    return &sys->reservations[index];
}

size_t reservation_count(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    return sys->reservation_count;
}

Status reservation_is_available(const ParkingSystem *sys, Id slot_id,
                                Id vehicle_id, time_t start_time, time_t end_time,
                                int *out_available) {
    if (sys == NULL || out_available == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (start_time < 0 || end_time < 0 || end_time <= start_time) {
        return STATUS_INVALID_ARGUMENT;
    }

    size_t dummy = 0;
    if (slot_find_by_id(sys, slot_id, &dummy) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }

    time_t now = time(NULL);

    /* Check for conflicting non-cancelled, non-expired reservations.
     * Reservations owned by vehicle_id are excluded so the holder can use
     * their own reserved slot. */
    for (size_t i = 0; i < sys->reservation_count; i++) {
        const Reservation *r = &sys->reservations[i];
        if (r->slot_id == slot_id && r->vehicle_id != vehicle_id &&
            r->status != RESERVATION_CANCELLED &&
            !is_expired(r, now) &&
            overlaps(start_time, end_time, r->start_time, r->end_time)) {
            *out_available = 0;
            return STATUS_OK;
        }
    }

    /* Check for an active session that may still occupy the slot. */
    if (slot_has_active_session(sys, slot_id, end_time)) {
        *out_available = 0;
        return STATUS_OK;
    }

    *out_available = 1;
    return STATUS_OK;
}
