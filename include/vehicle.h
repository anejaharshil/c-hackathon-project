#ifndef VEHICLE_H
#define VEHICLE_H

#include "common.h"

struct ParkingSystem; /* forward declaration; defined in system.h */

/* A registered vehicle. Registration numbers must be unique across the system. */
typedef struct {
    Id id;
    char registration[MAX_REG_LEN];
    VehicleType type;
    char owner[MAX_OWNER_LEN];
    UserType user_type;
} Vehicle;

/* Developer 1 owns: vehicle module. */

/* Register a new vehicle in the system. Returns STATUS_ALREADY_EXISTS if the
 * registration is already taken. The vehicle is appended to the system array. */
Status vehicle_register(struct ParkingSystem *sys, const char *registration,
                        VehicleType type, const char *owner, UserType user_type,
                        Id *out_id);

/* Find a vehicle by registration. Returns STATUS_NOT_FOUND if absent.
 * Sets out_index to the position in sys->vehicles. */
Status vehicle_find_by_reg(const struct ParkingSystem *sys, const char *registration,
                           size_t *out_index);

/* Find a vehicle by ID. Sets out_index. */
Status vehicle_find_by_id(const struct ParkingSystem *sys, Id id, size_t *out_index);

/* Remove a vehicle by ID (and its session, if any). */
Status vehicle_remove(struct ParkingSystem *sys, Id id);

/* Get read-only access to the vehicle at index. Returns NULL if out of bounds. */
const Vehicle *vehicle_get(const struct ParkingSystem *sys, size_t index);

size_t vehicle_count(const struct ParkingSystem *sys);

#endif /* VEHICLE_H */
