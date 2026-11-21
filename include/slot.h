#ifndef SLOT_H
#define SLOT_H

#include "common.h"

struct ParkingSystem; /* forward declaration; defined in system.h */

/* A parking slot. At most one active vehicle occupies a slot at any time. */
typedef struct {
    Id id;
    ZoneType zone;
    char location[MAX_LOCATION_LEN];
    int is_available;      /* 1 = free, 0 = occupied */
    Id vehicle_id;         /* ID of the vehicle currently occupying it, or ID_NONE */
} ParkingSlot;

/* Developer 1 owns: parking/slot module. */

/* Allocate the initial set of slots in the system. Returns STATUS_OK on success. */
Status slot_init(struct ParkingSystem *sys, int capacity);

/* Find a free slot in the given zone. Returns STATUS_NOT_FOUND if none free.
 * Sets out_index to the position in sys->slots. */
Status slot_find_free(const struct ParkingSystem *sys, ZoneType zone,
                      size_t *out_index);

/* Find a slot by ID. Sets out_index. */
Status slot_find_by_id(const struct ParkingSystem *sys, Id id, size_t *out_index);

/* Mark slot occupied by a vehicle. Returns STATUS_CONFLICT if already occupied. */
Status slot_occupy(struct ParkingSystem *sys, size_t index, Id vehicle_id);

/* Mark slot free (release its vehicle). */
Status slot_release(struct ParkingSystem *sys, size_t index);

/* Get read-only access to the slot at index. Returns NULL if out of bounds. */
const ParkingSlot *slot_get(const struct ParkingSystem *sys, size_t index);

size_t slot_count(const struct ParkingSystem *sys);

#endif /* SLOT_H */
