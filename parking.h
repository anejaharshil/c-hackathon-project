#ifndef PARKING_H
#define PARKING_H

#include <stddef.h>
#include <time.h>

#define MAX_REG_LEN 16
#define MAX_OWNER_LEN 64
#define MAX_ID_LEN 32

#define MAX_SLOTS 200
#define MAX_RECORDS 1000000u
#define DEFAULT_SLOTS 45
#define VISITOR_RATE 20
#define HOUR_SECONDS 3600

#define DATA_PATH "data/parking.dat"
#define ID_NONE (-1LL)

typedef long long Id;

typedef enum {
    USER_STUDENT,
    USER_STAFF,
    USER_VISITOR
} UserType;

typedef enum {
    VEHICLE_CAR,
    VEHICLE_MOTORCYCLE,
    VEHICLE_BICYCLE
} VehicleType;

typedef enum {
    SECTION_TEACHER,
    SECTION_STUDENT,
    SECTION_VISITOR,
    SECTION_COUNT
} ParkingSection;

#define VEHICLE_TYPE_COUNT 3

typedef enum {
    PARKING_OK = 0,
    PARKING_ERROR,
    PARKING_NOT_FOUND,
    PARKING_DUPLICATE,
    PARKING_ALREADY_PARKED,
    PARKING_NOT_PARKED,
    PARKING_FULL,
    PARKING_LIMIT,
    PARKING_NO_MEMORY,
    PARKING_INVALID,
    PARKING_INVALID_REGISTRATION,
    PARKING_INVALID_ID
} ParkingStatus;

/* Members are ordered widest-first so the compiler adds no padding. */
typedef struct {
    Id id;
    UserType user_type;
    VehicleType vehicle_type;
    char registration[MAX_REG_LEN];
    char owner[MAX_OWNER_LEN];
    char institution_id[MAX_ID_LEN];
} Vehicle;

typedef struct {
    Id id;
    ParkingSection section;
    VehicleType type;
    Id vehicle_id;
    int available;
} ParkingSlot;

typedef struct {
    Id id;
    Id vehicle_id;
    Id slot_id;
    time_t start_time;
    time_t end_time;
    int charge;
    int active;
} ParkingSession;

typedef struct {
    Vehicle *vehicles;
    size_t vehicle_count;
    size_t vehicle_capacity;

    ParkingSlot *slots;
    size_t slot_count;

    ParkingSession *sessions;
    size_t session_count;
    size_t session_capacity;

    Id next_id;
} ParkingSystem;

typedef struct {
    char registration[MAX_REG_LEN];
    char owner[MAX_OWNER_LEN];
    UserType user_type;
    Id slot_id;
    time_t entry_time;
    time_t exit_time;
    long duration_seconds;
    long charge;
} Receipt;

ParkingSystem *parking_create(int slot_count);
void parking_destroy(ParkingSystem *sys);

ParkingStatus parking_register(ParkingSystem *sys, const char *registration,
                               const char *owner, const char *institution_id,
                               UserType user_type, VehicleType vehicle_type,
                               Id *out_id);
Vehicle *parking_find_vehicle(ParkingSystem *sys, const char *registration);
Vehicle *parking_vehicle_by_id(ParkingSystem *sys, Id id);

ParkingStatus parking_enter(ParkingSystem *sys, const char *registration,
                            time_t now, Id *out_slot_id);
ParkingStatus parking_exit(ParkingSystem *sys, const char *registration,
                           time_t now, Receipt *out_receipt);
ParkingSession *parking_active_session(ParkingSystem *sys, Id vehicle_id);
size_t parking_sessions_for(const ParkingSystem *sys, Id vehicle_id);

long parking_compute_charge(UserType user_type, time_t entry, time_t exit);

size_t parking_occupied_slots(const ParkingSystem *sys);
size_t parking_active_sessions(const ParkingSystem *sys);
long parking_total_revenue(const ParkingSystem *sys);

/* Per-section / per-type slot counters */
size_t parking_section_occupied(const ParkingSystem *sys, ParkingSection section);
size_t parking_section_total(const ParkingSystem *sys, ParkingSection section);
size_t parking_section_registered(const ParkingSystem *sys,
                                  ParkingSection section);
size_t parking_section_type_occupied(const ParkingSystem *sys,
                                     ParkingSection section,
                                     VehicleType type);
size_t parking_section_type_total(const ParkingSystem *sys,
                                  ParkingSection section,
                                  VehicleType type);

const char *parking_user_type_name(UserType type);
const char *parking_vehicle_type_name(VehicleType type);
const char *parking_section_name(ParkingSection section);

ParkingStatus storage_save(const ParkingSystem *sys, const char *path);
ParkingStatus storage_load(ParkingSystem *sys, const char *path);
ParkingStatus storage_preserve(const char *path, char *backup_path,
                               size_t backup_size);

#endif /* PARKING_H */
