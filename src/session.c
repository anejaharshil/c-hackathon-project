#include "session.h"
#include "system.h"

#include <stdlib.h>
#include <string.h>

Status session_start(ParkingSystem *sys, Id vehicle_id, Id slot_id,
                     time_t start_time, Id *out_id) {
    if (sys == NULL || out_id == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    /* Validate the vehicle and slot exist. */
    size_t vehicle_index = 0;
    if (vehicle_find_by_id(sys, vehicle_id, &vehicle_index) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }
    size_t slot_index = 0;
    if (slot_find_by_id(sys, slot_id, &slot_index) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }

    Status st;

    /* One active session per vehicle. */
    size_t dummy = 0;
    if (session_find_active_for_vehicle(sys, vehicle_id, &dummy) == STATUS_OK) {
        return STATUS_CONFLICT;
    }

    /* One active vehicle per slot. */
    if (!sys->slots[slot_index].is_available) {
        return STATUS_CONFLICT;
    }

    /* Slot must not be reserved for another vehicle during any future time.
     * Since the session end time is unknown at entry, treat the session as
     * potentially indefinite: any future non-cancelled, non-expired
     * reservation on this slot belonging to another vehicle blocks entry. */
    int available = 0;
    st = reservation_is_available(sys, slot_id, vehicle_id,
                                  start_time, start_time + (100LL * 365 * 24 * 3600),
                                  &available);
    if (st != STATUS_OK) {
        return st;
    }
    if (!available) {
        return STATUS_CONFLICT;
    }

    /* Grow session storage before mutating any state; failure preserves
     * existing data. */
    if (sys->session_count == sys->session_capacity) {
        size_t new_cap = sys->session_capacity == 0 ? 8 : sys->session_capacity * 2;
        if (new_cap > (size_t)-1 / sizeof(ParkingSession)) {
            return STATUS_OUT_OF_MEMORY;
        }
        ParkingSession *tmp = realloc(sys->sessions, new_cap * sizeof(ParkingSession));
        if (tmp == NULL) {
            return STATUS_OUT_OF_MEMORY;
        }
        sys->sessions = tmp;
        sys->session_capacity = new_cap;
    }

    /* Mark the slot occupied first. This cannot fail post-validation, so the
     * state change below is atomic with the session record. */
    if (slot_occupy(sys, slot_index, vehicle_id) != STATUS_OK) {
        return STATUS_ERROR;
    }

    ParkingSession *s = &sys->sessions[sys->session_count];
    s->id = system_next_id(sys, "session");
    s->vehicle_id = vehicle_id;
    s->slot_id = slot_id;
    s->start_time = start_time;
    s->end_time = 0;
    s->active = 1;

    *out_id = s->id;
    sys->session_count++;
    return STATUS_OK;
}

Status session_end(ParkingSystem *sys, Id vehicle_id, time_t end_time) {
    if (sys == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t index = 0;
    Status st = session_find_active_for_vehicle(sys, vehicle_id, &index);
    if (st != STATUS_OK) {
        return st;
    }

    ParkingSession *s = &sys->sessions[index];
    if (end_time < s->start_time) {
        return STATUS_INVALID_ARGUMENT;
    }

    s->end_time = end_time;
    s->active = 0;

    /* Release the slot. A best-effort release: the session is ended regardless
     * so a previously-released slot does not fail the exit. */
    size_t slot_index = 0;
    if (slot_find_by_id(sys, s->slot_id, &slot_index) == STATUS_OK) {
        slot_release(sys, slot_index);
    }
    return STATUS_OK;
}

Status session_find_active_for_vehicle(const ParkingSystem *sys, Id vehicle_id,
                                       size_t *out_index) {
    if (sys == NULL || out_index == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < sys->session_count; i++) {
        if (sys->sessions[i].vehicle_id == vehicle_id && sys->sessions[i].active) {
            *out_index = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

Status session_find_by_id(const ParkingSystem *sys, Id id, size_t *out_index) {
    if (sys == NULL || out_index == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < sys->session_count; i++) {
        if (sys->sessions[i].id == id) {
            *out_index = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

const ParkingSession *session_get(const ParkingSystem *sys, size_t index) {
    if (sys == NULL || index >= sys->session_count) {
        return NULL;
    }
    return &sys->sessions[index];
}

size_t session_count(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    return sys->session_count;
}
