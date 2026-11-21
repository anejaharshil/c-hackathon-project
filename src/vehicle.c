#include "vehicle.h"
#include "system.h"

#include <stdlib.h>
#include <string.h>

static int is_valid_reg(const char *reg) {
    return reg != NULL && reg[0] != '\0' && strlen(reg) < MAX_REG_LEN;
}

static int is_valid_owner(const char *owner) {
    return owner != NULL && owner[0] != '\0' && strlen(owner) < MAX_OWNER_LEN;
}

static int is_valid_vehicle_type(VehicleType type) {
    return type >= VEHICLE_CAR && type <= VEHICLE_BICYCLE;
}

static int is_valid_user_type(UserType user_type) {
    return user_type >= USER_STUDENT && user_type <= USER_ADMIN;
}

static int reg_taken(const ParkingSystem *sys, const char *reg, size_t *skip) {
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        if (skip && i == *skip) {
            continue;
        }
        if (strcmp(sys->vehicles[i].registration, reg) == 0) {
            return 1;
        }
    }
    return 0;
}

Status vehicle_register(ParkingSystem *sys, const char *registration,
                        VehicleType type, const char *owner, UserType user_type,
                        Id *out_id) {
    if (sys == NULL || registration == NULL || owner == NULL || out_id == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (!is_valid_reg(registration)) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (!is_valid_owner(owner)) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (!is_valid_vehicle_type(type)) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (!is_valid_user_type(user_type)) {
        return STATUS_INVALID_ARGUMENT;
    }
    if (reg_taken(sys, registration, NULL)) {
        return STATUS_ALREADY_EXISTS;
    }

    if (sys->vehicle_count == sys->vehicle_capacity) {
        size_t new_cap = sys->vehicle_capacity == 0 ? 8 : sys->vehicle_capacity * 2;
        if (new_cap > (size_t)-1 / sizeof(Vehicle)) {
            return STATUS_OUT_OF_MEMORY;
        }
        Vehicle *tmp = realloc(sys->vehicles, new_cap * sizeof(Vehicle));
        if (tmp == NULL) {
            return STATUS_OUT_OF_MEMORY;
        }
        sys->vehicles = tmp;
        sys->vehicle_capacity = new_cap;
    }

    Vehicle *v = &sys->vehicles[sys->vehicle_count];
    v->id = system_next_id(sys, "vehicle");
    strncpy(v->registration, registration, MAX_REG_LEN - 1);
    v->registration[MAX_REG_LEN - 1] = '\0';
    v->type = type;
    strncpy(v->owner, owner, MAX_OWNER_LEN - 1);
    v->owner[MAX_OWNER_LEN - 1] = '\0';
    v->user_type = user_type;

    *out_id = v->id;
    sys->vehicle_count++;
    return STATUS_OK;
}

Status vehicle_find_by_reg(const ParkingSystem *sys, const char *registration,
                           size_t *out_index) {
    if (sys == NULL || registration == NULL || out_index == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        if (strcmp(sys->vehicles[i].registration, registration) == 0) {
            *out_index = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

Status vehicle_find_by_id(const ParkingSystem *sys, Id id, size_t *out_index) {
    if (sys == NULL || out_index == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        if (sys->vehicles[i].id == id) {
            *out_index = i;
            return STATUS_OK;
        }
    }
    return STATUS_NOT_FOUND;
}

Status vehicle_remove(ParkingSystem *sys, Id id) {
    if (sys == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t index = 0;
    Status st = vehicle_find_by_id(sys, id, &index);
    if (st != STATUS_OK) {
        return st;
    }

    /* A vehicle with an active session or a live reservation cannot be
     * removed, otherwise the session/reservation would reference a removed
     * vehicle. */
    size_t dummy = 0;
    if (session_find_active_for_vehicle(sys, id, &dummy) == STATUS_OK) {
        return STATUS_CONFLICT;
    }
    for (size_t i = 0; i < sys->reservation_count; i++) {
        if (sys->reservations[i].vehicle_id == id &&
            sys->reservations[i].status != RESERVATION_CANCELLED) {
            return STATUS_CONFLICT;
        }
    }

    /* Shift remaining vehicles down. IDs stay stable. */
    memmove(&sys->vehicles[index], &sys->vehicles[index + 1],
            (sys->vehicle_count - index - 1) * sizeof(Vehicle));
    sys->vehicle_count--;
    return STATUS_OK;
}

const Vehicle *vehicle_get(const ParkingSystem *sys, size_t index) {
    if (sys == NULL || index >= sys->vehicle_count) {
        return NULL;
    }
    return &sys->vehicles[index];
}

size_t vehicle_count(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    return sys->vehicle_count;
}
