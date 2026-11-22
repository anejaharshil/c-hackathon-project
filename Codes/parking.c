// Header Files
#include "Header Files/parking.h"
#include "Header Files/registration.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global Variables
// (none: all file-scope state is local to functions)

// Macros
#define INITIAL_CAPACITY 8

// Function Declarations
static int string_ok(const char *text, size_t max_length);
static int user_type_ok(UserType type);
static int vehicle_type_ok(VehicleType type);
static void copy_text(char *destination, size_t size, const char *source);
static int registration_taken(const ParkingSystem *sys,
                              const char *registration);
static int institution_taken(const ParkingSystem *sys,
                             const char *institution_id,
                             UserType user_type);
static ParkingStatus grow_vehicles(ParkingSystem *sys);
static ParkingStatus grow_sessions(ParkingSystem *sys);
static ParkingSection section_for_user_type(UserType user_type);

// Functions
ParkingSystem *parking_create(int slot_count) {
    if (slot_count <= 0 || slot_count > MAX_SLOTS) {
        return NULL;
    }
    /* Divide slots equally among 3 sections x 3 vehicle types = 9 pools. */
    if (slot_count < 9) {
        return NULL;
    }
    ParkingSystem *sys = calloc(1, sizeof *sys);
    if (sys == NULL) {
        return NULL;
    }
    sys->slots = calloc((size_t)slot_count, sizeof *sys->slots);
    if (sys->slots == NULL) {
        free(sys);
        return NULL;
    }
    sys->slot_count = (size_t)slot_count;
    sys->next_id = 1;

    /* Slots per pool = slot_count / 9, distributed round-robin. */
    size_t slots_per_pool = slot_count / (size_t)(SECTION_COUNT * VEHICLE_TYPE_COUNT);
    size_t extra = slot_count % (size_t)(SECTION_COUNT * VEHICLE_TYPE_COUNT);
    Id next_slot_id = 1;

    for (int s = 0; s < SECTION_COUNT; s++) {
        for (int t = 0; t < VEHICLE_TYPE_COUNT; t++) {
            size_t pool_size = slots_per_pool;
            if ((size_t)(s * VEHICLE_TYPE_COUNT + t) < extra) {
                pool_size++;
            }
            for (size_t i = 0; i < pool_size; i++) {
                size_t index = (size_t)(next_slot_id - 1);
                sys->slots[index].id = next_slot_id++;
                sys->slots[index].section = (ParkingSection)s;
                sys->slots[index].type = (VehicleType)t;
                sys->slots[index].vehicle_id = ID_NONE;
                sys->slots[index].available = 1;
            }
        }
    }
    return sys;
}

void parking_destroy(ParkingSystem *sys) {
    if (sys == NULL) {
        return;
    }
    free(sys->vehicles);
    free(sys->slots);
    free(sys->sessions);
    free(sys);
}

ParkingStatus parking_register(ParkingSystem *sys, const char *registration,
                               const char *owner, const char *institution_id,
                               UserType user_type, VehicleType vehicle_type,
                               Id *out_id) {
    char normalized[16];

    if (sys == NULL || !string_ok(owner, MAX_OWNER_LEN) ||
        !user_type_ok(user_type) || !vehicle_type_ok(vehicle_type)) {
        return PARKING_INVALID;
    }
    if (user_type != USER_VISITOR &&
        !string_ok(institution_id, MAX_ID_LEN)) {
        return PARKING_INVALID;
    }

    /* Bicycles may not carry a registration number; an empty registration is
     * allowed for them. Non-bicycles must always provide a valid one. */
    if (vehicle_type == VEHICLE_BICYCLE) {
        normalized[0] = '\0';
    } else if (!registration_normalize(normalized, sizeof normalized,
                                       registration)) {
        return PARKING_INVALID_REGISTRATION;
    }

    /* Validate the category-specific institutional ID. */
    if (user_type == USER_STUDENT) {
        if (!sap_id_is_valid(institution_id)) {
            return PARKING_INVALID_ID;
        }
    } else if (user_type == USER_STAFF) {
        if (!teacher_id_is_valid(institution_id)) {
            return PARKING_INVALID_ID;
        }
    }

    /* An empty registration (bicycle) is not checked for duplication;
     * registration_taken also returns 0 for empty strings. */
    if (registration_taken(sys, normalized)) {
        return PARKING_DUPLICATE;
    }
    if (user_type != USER_VISITOR &&
        institution_taken(sys, institution_id, user_type)) {
        return PARKING_DUPLICATE;
    }
    if (sys->vehicle_count >= MAX_RECORDS || sys->next_id >= LLONG_MAX) {
        return PARKING_LIMIT;
    }

    ParkingStatus status = grow_vehicles(sys);
    if (status != PARKING_OK) {
        return status;
    }

    Vehicle *vehicle = &sys->vehicles[sys->vehicle_count];
    memset(vehicle, 0, sizeof *vehicle);
    vehicle->id = sys->next_id++;
    vehicle->user_type = user_type;
    vehicle->vehicle_type = vehicle_type;
    copy_text(vehicle->registration, sizeof vehicle->registration, normalized);
    copy_text(vehicle->owner, sizeof vehicle->owner, owner);
    copy_text(vehicle->institution_id, sizeof vehicle->institution_id,
              user_type == USER_VISITOR ? "-" : institution_id);
    sys->vehicle_count++;

    if (out_id != NULL) {
        *out_id = vehicle->id;
    }
    /* Auto-persist successful state-changing operations. */
    if (storage_save(sys, DATA_PATH) != PARKING_OK) {
        fprintf(stderr, "Warning: data could not be saved after registration.\n");
    }
    return PARKING_OK;
}

Vehicle *parking_find_vehicle(ParkingSystem *sys, const char *registration) {
    char normalized[16];

    if (sys == NULL || registration == NULL) {
        return NULL;
    }
    /* Try a normalized lookup first; fall back to the raw query so
     * legacy records stored before validation remain reachable. */
    if (registration_normalize(normalized, sizeof normalized, registration)) {
        registration = normalized;
    }
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        if (strcmp(sys->vehicles[i].registration, registration) == 0) {
            return &sys->vehicles[i];
        }
    }
    return NULL;
}

size_t parking_find_vehicles_by_owner_and_type(ParkingSystem *sys,
                                               const char *owner,
                                               VehicleType type,
                                               Id *out_ids,
                                               size_t max_out) {
    if (sys == NULL || owner == NULL || out_ids == NULL || max_out == 0) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->vehicle_count && count < max_out; i++) {
        if (sys->vehicles[i].vehicle_type == type &&
            strcmp(sys->vehicles[i].owner, owner) == 0) {
            out_ids[count++] = sys->vehicles[i].id;
        }
    }
    return count;
}

Vehicle *parking_vehicle_by_id(ParkingSystem *sys, Id id) {
    if (sys == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        if (sys->vehicles[i].id == id) {
            return &sys->vehicles[i];
        }
    }
    return NULL;
}

ParkingSession *parking_active_session(ParkingSystem *sys, Id vehicle_id) {
    if (sys == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sys->session_count; i++) {
        ParkingSession *session = &sys->sessions[i];
        if (session->active && session->vehicle_id == vehicle_id) {
            return session;
        }
    }
    return NULL;
}

size_t parking_sessions_for(const ParkingSystem *sys, Id vehicle_id) {
    if (sys == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->session_count; i++) {
        if (sys->sessions[i].vehicle_id == vehicle_id) {
            count++;
        }
    }
    return count;
}

static ParkingSection section_for_user_type(UserType user_type) {
    switch (user_type) {
        case USER_STAFF:
            return SECTION_TEACHER;
        case USER_STUDENT:
            return SECTION_STUDENT;
        case USER_VISITOR:
            return SECTION_VISITOR;
        default:
            return SECTION_VISITOR;
    }
}

ParkingStatus parking_enter(ParkingSystem *sys, const char *registration,
                            time_t now, Id *out_slot_id) {
    if (sys == NULL) {
        return PARKING_INVALID;
    }
    Vehicle *vehicle = parking_find_vehicle(sys, registration);
    if (vehicle == NULL) {
        return PARKING_NOT_FOUND;
    }
    return parking_enter_by_id(sys, vehicle->id, now, out_slot_id);
}

ParkingStatus parking_enter_by_id(ParkingSystem *sys, Id vehicle_id,
                                  time_t now, Id *out_slot_id) {
    if (sys == NULL) {
        return PARKING_INVALID;
    }
    Vehicle *vehicle = parking_vehicle_by_id(sys, vehicle_id);
    if (vehicle == NULL) {
        return PARKING_NOT_FOUND;
    }
    if (parking_active_session(sys, vehicle_id) != NULL) {
        return PARKING_ALREADY_PARKED;
    }

    ParkingSection section = section_for_user_type(vehicle->user_type);

    /* Find an available slot in the vehicle's section and type. */
    ParkingSlot *slot = NULL;
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (sys->slots[i].available &&
            sys->slots[i].section == section &&
            sys->slots[i].type == vehicle->vehicle_type) {
            slot = &sys->slots[i];
            break;
        }
    }
    if (slot == NULL) {
        return PARKING_FULL;
    }
    if (sys->session_count >= MAX_RECORDS || sys->next_id >= LLONG_MAX) {
        return PARKING_LIMIT;
    }

    ParkingStatus status = grow_sessions(sys);
    if (status != PARKING_OK) {
        return status;
    }

    ParkingSession *session = &sys->sessions[sys->session_count];
    session->id = sys->next_id++;
    session->vehicle_id = vehicle_id;
    session->slot_id = slot->id;
    session->start_time = now;
    session->end_time = 0;
    session->charge = 0;
    session->active = 1;
    sys->session_count++;

    slot->available = 0;
    slot->vehicle_id = vehicle_id;

    if (out_slot_id != NULL) {
        *out_slot_id = slot->id;
    }
    /* Auto-persist successful state-changing operations. */
    if (storage_save(sys, DATA_PATH) != PARKING_OK) {
        fprintf(stderr, "Warning: data could not be saved after entry.\n");
    }
    return PARKING_OK;
}

long parking_compute_charge(UserType user_type, time_t entry, time_t exit) {
    if (user_type != USER_VISITOR || exit <= entry) {
        return 0;
    }

    long long seconds = (long long)(exit - entry);
    long long hours;
    if (seconds > LLONG_MAX - (HOUR_SECONDS - 1)) {
        hours = LLONG_MAX / HOUR_SECONDS + 1;
    } else {
        hours = (seconds + HOUR_SECONDS - 1) / HOUR_SECONDS;
    }
    if (hours > LONG_MAX / VISITOR_RATE) {
        return LONG_MAX;
    }
    return (long)(hours * VISITOR_RATE);
}

ParkingStatus parking_exit_by_id(ParkingSystem *sys, Id vehicle_id,
                                 time_t now, Receipt *out_receipt) {
    if (sys == NULL || out_receipt == NULL) {
        return PARKING_INVALID;
    }
    Vehicle *vehicle = parking_vehicle_by_id(sys, vehicle_id);
    if (vehicle == NULL) {
        return PARKING_NOT_FOUND;
    }
    ParkingSession *session = parking_active_session(sys, vehicle_id);
    if (session == NULL) {
        return PARKING_NOT_PARKED;
    }

    /* Reject backwards-clock exits: exit must not be before entry. */
    if (now < session->start_time) {
        return PARKING_INVALID_TIME;
    }

    long charge = parking_compute_charge(vehicle->user_type,
                                         session->start_time, now);
    long duration = now > session->start_time
                        ? (long)(now - session->start_time)
                        : 0;

    session->end_time = now;
    session->charge = charge > INT_MAX ? INT_MAX : (int)charge;
    session->active = 0;

    for (size_t i = 0; i < sys->slot_count; i++) {
        if (sys->slots[i].id == session->slot_id) {
            sys->slots[i].available = 1;
            sys->slots[i].vehicle_id = ID_NONE;
            break;
        }
    }

    copy_text(out_receipt->registration, sizeof out_receipt->registration,
              vehicle->registration);
    copy_text(out_receipt->owner, sizeof out_receipt->owner, vehicle->owner);
    out_receipt->user_type = vehicle->user_type;
    out_receipt->slot_id = session->slot_id;
    out_receipt->entry_time = session->start_time;
    out_receipt->exit_time = now;
    out_receipt->duration_seconds = duration;
    out_receipt->charge = session->charge;
    /* Auto-persist successful state-changing operations. */
    if (storage_save(sys, DATA_PATH) != PARKING_OK) {
        fprintf(stderr, "Warning: data could not be saved after exit.\n");
    }
    return PARKING_OK;
}

ParkingStatus parking_exit(ParkingSystem *sys, const char *registration,
                           time_t now, Receipt *out_receipt) {
    if (sys == NULL) {
        return PARKING_INVALID;
    }
    Vehicle *vehicle = parking_find_vehicle(sys, registration);
    if (vehicle == NULL) {
        return PARKING_NOT_FOUND;
    }
    return parking_exit_by_id(sys, vehicle->id, now, out_receipt);
}

size_t parking_occupied_slots(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (!sys->slots[i].available) {
            count++;
        }
    }
    return count;
}

size_t parking_active_sessions(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->session_count; i++) {
        if (sys->sessions[i].active) {
            count++;
        }
    }
    return count;
}

long parking_total_revenue(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    long total = 0;
    for (size_t i = 0; i < sys->session_count; i++) {
        const ParkingSession *session = &sys->sessions[i];
        if (!session->active) {
            if (total > LONG_MAX - session->charge) {
                return LONG_MAX;
            }
            total += session->charge;
        }
    }
    return total;
}

size_t parking_section_occupied(const ParkingSystem *sys,
                                ParkingSection section) {
    if (sys == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (!sys->slots[i].available && sys->slots[i].section == section) {
            count++;
        }
    }
    return count;
}

size_t parking_section_total(const ParkingSystem *sys,
                             ParkingSection section) {
    if (sys == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (sys->slots[i].section == section) {
            count++;
        }
    }
    return count;
}

size_t parking_section_registered(const ParkingSystem *sys,
                                  ParkingSection section) {
    if (sys == NULL) {
        return 0;
    }
    UserType user_type;
    switch (section) {
        case SECTION_TEACHER:
            user_type = USER_STAFF;
            break;
        case SECTION_STUDENT:
            user_type = USER_STUDENT;
            break;
        case SECTION_VISITOR:
        default:
            user_type = USER_VISITOR;
            break;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        if (sys->vehicles[i].user_type == user_type) {
            count++;
        }
    }
    return count;
}

size_t parking_section_type_occupied(const ParkingSystem *sys,
                                     ParkingSection section,
                                     VehicleType type) {
    if (sys == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (!sys->slots[i].available &&
            sys->slots[i].section == section &&
            sys->slots[i].type == type) {
            count++;
        }
    }
    return count;
}

size_t parking_section_type_total(const ParkingSystem *sys,
                                  ParkingSection section,
                                  VehicleType type) {
    if (sys == NULL) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (sys->slots[i].section == section && sys->slots[i].type == type) {
            count++;
        }
    }
    return count;
}

const char *parking_user_type_name(UserType type) {
    switch (type) {
        case USER_STUDENT:
            return "Student";
        case USER_STAFF:
            return "Teacher";
        case USER_VISITOR:
            return "Visitor";
        default:
            return "Unknown";
    }
}

const char *parking_vehicle_type_name(VehicleType type) {
    switch (type) {
        case VEHICLE_CAR:
            return "Car";
        case VEHICLE_MOTORCYCLE:
            return "Motorcycle";
        case VEHICLE_BICYCLE:
            return "Bicycle";
        default:
            return "Unknown";
    }
}

const char *parking_section_name(ParkingSection section) {
    switch (section) {
        case SECTION_TEACHER:
            return "Teacher Parking";
        case SECTION_STUDENT:
            return "Student Parking";
        case SECTION_VISITOR:
            return "Visitor Parking";
        default:
            return "Unknown";
    }
}

static int string_ok(const char *text, size_t max_length) {
    return text != NULL && text[0] != '\0' && strlen(text) < max_length;
}

static int user_type_ok(UserType type) {
    return type >= USER_STUDENT && type <= USER_VISITOR;
}

static int vehicle_type_ok(VehicleType type) {
    return type >= VEHICLE_CAR && type <= VEHICLE_BICYCLE;
}

static void copy_text(char *destination, size_t size, const char *source) {
    snprintf(destination, size, "%s", source);
}

static int registration_taken(const ParkingSystem *sys,
                              const char *registration) {
    if (registration == NULL || registration[0] == '\0') {
        return 0;
    }
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        if (strcmp(sys->vehicles[i].registration, registration) == 0) {
            return 1;
        }
    }
    return 0;
}

static int institution_taken(const ParkingSystem *sys,
                             const char *institution_id,
                             UserType user_type) {
    if (user_type == USER_VISITOR) {
        return 0;
    }
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        const Vehicle *vehicle = &sys->vehicles[i];
        if (vehicle->user_type == user_type &&
            strcmp(vehicle->institution_id, institution_id) == 0) {
            return 1;
        }
    }
    return 0;
}

static ParkingStatus grow_vehicles(ParkingSystem *sys) {
    if (sys->vehicle_count < sys->vehicle_capacity) {
        return PARKING_OK;
    }
    if (sys->vehicle_capacity > SIZE_MAX / 2) {
        return PARKING_NO_MEMORY;
    }
    size_t capacity = sys->vehicle_capacity == 0
                          ? INITIAL_CAPACITY
                          : sys->vehicle_capacity * 2;
    if (capacity > SIZE_MAX / sizeof(Vehicle)) {
        return PARKING_NO_MEMORY;
    }
    Vehicle *grown = realloc(sys->vehicles, capacity * sizeof *grown);
    if (grown == NULL) {
        return PARKING_NO_MEMORY;
    }
    sys->vehicles = grown;
    sys->vehicle_capacity = capacity;
    return PARKING_OK;
}

static ParkingStatus grow_sessions(ParkingSystem *sys) {
    if (sys->session_count < sys->session_capacity) {
        return PARKING_OK;
    }
    if (sys->session_capacity > SIZE_MAX / 2) {
        return PARKING_NO_MEMORY;
    }
    size_t capacity = sys->session_capacity == 0
                          ? INITIAL_CAPACITY
                          : sys->session_capacity * 2;
    if (capacity > SIZE_MAX / sizeof(ParkingSession)) {
        return PARKING_NO_MEMORY;
    }
    ParkingSession *grown = realloc(sys->sessions, capacity * sizeof *grown);
    if (grown == NULL) {
        return PARKING_NO_MEMORY;
    }
    sys->sessions = grown;
    sys->session_capacity = capacity;
    return PARKING_OK;
}
