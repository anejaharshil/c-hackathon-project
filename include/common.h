#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

/* Shared enums for the Smart Campus Parking Management System. */

typedef enum {
    USER_STUDENT,
    USER_STAFF,
    USER_VISITOR,
    USER_ADMIN
} UserType;

typedef enum {
    VEHICLE_CAR,
    VEHICLE_MOTORCYCLE,
    VEHICLE_BICYCLE
} VehicleType;

typedef enum {
    ZONE_A,
    ZONE_B,
    ZONE_C,
    ZONE_ACCESSIBLE,
    ZONE_VISITOR
} ZoneType;

typedef enum {
    RESERVATION_PENDING,
    RESERVATION_ACTIVE,
    RESERVATION_COMPLETED,
    RESERVATION_CANCELLED
} ReservationStatus;

/* Stable integer ID type used for relationships between records. */
typedef long long Id;

/* Null ID sentinel. */
#define ID_NONE (-1LL)

/* Status / error return conventions. All public functions return Status. */
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR = -1,
    STATUS_NOT_FOUND = -2,
    STATUS_ALREADY_EXISTS = -3,
    STATUS_INVALID_ARGUMENT = -4,
    STATUS_OUT_OF_MEMORY = -5,
    STATUS_CONFLICT = -6,
    STATUS_IO_ERROR = -7
} Status;

/* Maximum length for bounded string fields. */
#define MAX_REG_LEN 16      /* e.g. "AB12 CDE" */
#define MAX_MODEL_LEN 32
#define MAX_OWNER_LEN 64
#define MAX_LOCATION_LEN 64

#endif /* COMMON_H */
