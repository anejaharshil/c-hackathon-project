#include "reservation.h"
#include "system.h"

#include <stdlib.h>
#include <string.h>

/* Returns 1 if [a_start, a_end) overlaps [b_start, b_end). */
static int overlaps(time_t a_start, time_t a_end, time_t b_start, time_t b_end) {
    return a_start < b_end && b_start < a_end;
}

Status reservation_create(ParkingSystem *sys, Id vehicle_id, Id slot_id,
                          time_t start_time, time_t end_time, Id *out_id) {
    if (sys == NULL || out_id == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (start_time < 0 || end_time < 0 || end_time <= start_time) {
        return STATUS_INVALID_ARGUMENT;
    }

    /* Validate vehicle and slot exist. */
    size_t dummy = 0;
    if (vehicle_find_by_id(sys, vehicle_id, &dummy) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }
    if (slot_find_by_id(sys, slot_id, &dummy) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }

    /* No overlapping reservations for the same slot (excluding cancelled). */
    for (size_t i = 0; i < sys->reservation_count; i++) {
        Reservation *r = &sys->reservations[i];
        if (r->slot_id == slot_id && r->status != RESERVATION_CANCELLED &&
            overlaps(start_time, end_time, r->start_time, r->end_time)) {
            return STATUS_CONFLICT;
        }
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
