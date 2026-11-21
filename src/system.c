#include "system.h"

#include <stdlib.h>
#include <string.h>

static Id next_id_counter(ParkingSystem *sys, const char *kind) {
    if (strcmp(kind, "vehicle") == 0) {
        return sys->next_vehicle_id++;
    }
    if (strcmp(kind, "slot") == 0) {
        return sys->next_slot_id++;
    }
    if (strcmp(kind, "session") == 0) {
        return sys->next_session_id++;
    }
    if (strcmp(kind, "reservation") == 0) {
        return sys->next_reservation_id++;
    }
    return ID_NONE;
}

Status system_create(ParkingSystem **out_sys) {
    if (out_sys == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    ParkingSystem *sys = calloc(1, sizeof(ParkingSystem));
    if (sys == NULL) {
        return STATUS_OUT_OF_MEMORY;
    }
    /* IDs start at 1 so ID_NONE (-1) remains a valid sentinel. */
    sys->next_vehicle_id = 1;
    sys->next_slot_id = 1;
    sys->next_session_id = 1;
    sys->next_reservation_id = 1;
    *out_sys = sys;
    return STATUS_OK;
}

void system_destroy(ParkingSystem *sys) {
    if (sys == NULL) {
        return;
    }
    free(sys->vehicles);
    free(sys->slots);
    free(sys->sessions);
    free(sys->reservations);
    free(sys);
}

Status system_reset(ParkingSystem *sys) {
    if (sys == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    sys->vehicle_count = 0;
    sys->slot_count = 0;
    sys->session_count = 0;
    sys->reservation_count = 0;
    /* Keep allocated capacity; reset ID counters so IDs stay stable within a run. */
    sys->next_vehicle_id = 1;
    sys->next_slot_id = 1;
    sys->next_session_id = 1;
    sys->next_reservation_id = 1;
    return STATUS_OK;
}

Id system_next_id(ParkingSystem *sys, const char *kind) {
    if (sys == NULL || kind == NULL) {
        return ID_NONE;
    }
    return next_id_counter(sys, kind);
}
