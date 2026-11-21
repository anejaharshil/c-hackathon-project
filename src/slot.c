#include "slot.h"
#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Zone-to-user-category mapping (documented):
 *   ZONE_A         : general parking (students / staff)
 *   ZONE_B         : general parking (staff)
 *   ZONE_C         : general parking (students)
 *   ZONE_ACCESSIBLE: accessible / mobility-impaired users ONLY.
 *                    Never handed out as general overflow.
 *   ZONE_VISITOR   : visitor parking.
 *
 * slot_init distributes slots round-robin across these zones so every zone
 * is represented. slot_find_free matches the requested zone exactly and never
 * falls back to another zone, so accessible parking is not silently consumed
 * by general demand.
 */
static const ZoneType ZONE_ORDER[5] = {
    ZONE_A, ZONE_B, ZONE_C, ZONE_ACCESSIBLE, ZONE_VISITOR
};

Status slot_init(ParkingSystem *sys, int capacity) {
    if (sys == NULL || capacity <= 0) {
        return STATUS_INVALID_ARGUMENT;
    }
    /* If slots are already initialised, refuse to erase occupied state.
     * Re-initialising would wipe slot occupancy and desynchronise any active
     * sessions that reference those slots. An already-initialised, fully-free
     * system is left untouched. */
    if (sys->slots != NULL && sys->slot_count > 0) {
        for (size_t i = 0; i < sys->slot_count; i++) {
            if (!sys->slots[i].is_available) {
                return STATUS_CONFLICT;
            }
        }
        return STATUS_OK;
    }
    free(sys->slots);
    sys->slots = calloc((size_t)capacity, sizeof(ParkingSlot));
    if (sys->slots == NULL) {
        sys->slot_count = 0;
        sys->slot_capacity = 0;
        return STATUS_OUT_OF_MEMORY;
    }
    sys->slot_count = (size_t)capacity;
    sys->slot_capacity = (size_t)capacity;

    for (int i = 0; i < capacity; i++) {
        ParkingSlot *s = &sys->slots[i];
        s->id = system_next_id(sys, "slot");
        s->zone = ZONE_ORDER[i % 5];
        snprintf(s->location, sizeof(s->location), "Zone %c-%d",
                 'A' + (i % 5), i + 1);
        s->is_available = 1;
        s->vehicle_id = ID_NONE;
    }
    return STATUS_OK;
}

Status slot_find_free(const ParkingSystem *sys, ZoneType zone, size_t *out_index) {
    if (sys == NULL || out_index == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (sys->slots[i].is_available && sys->slots[i].zone == zone) {
            *out_index = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

Status slot_find_by_id(const ParkingSystem *sys, Id id, size_t *out_index) {
    if (sys == NULL || out_index == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (sys->slots[i].id == id) {
            *out_index = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

Status slot_occupy(ParkingSystem *sys, size_t index, Id vehicle_id) {
    if (sys == NULL || index >= sys->slot_count) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (!sys->slots[index].is_available) {
        return STATUS_CONFLICT;
    }
    sys->slots[index].is_available = 0;
    sys->slots[index].vehicle_id = vehicle_id;
    return STATUS_OK;
}

Status slot_release(ParkingSystem *sys, size_t index) {
    if (sys == NULL || index >= sys->slot_count) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (sys->slots[index].is_available) {
        return STATUS_CONFLICT;
    }
    sys->slots[index].is_available = 1;
    sys->slots[index].vehicle_id = ID_NONE;
    return STATUS_OK;
}

const ParkingSlot *slot_get(const ParkingSystem *sys, size_t index) {
    if (sys == NULL || index >= sys->slot_count) {
        return NULL;
    }
    return &sys->slots[index];
}

size_t slot_count(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    return sys->slot_count;
}
