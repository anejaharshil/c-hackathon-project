// Header Files
#include "parking.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global Variables
// (none: all file-scope state is local to functions)

// Macros
#define STORAGE_MAGIC 0x504B4453u
#define STORAGE_VERSION 2u
#define BACKUP_ATTEMPTS 100
#define TMP_SUFFIX ".tmp"

// Function Declarations
static int write_u32(FILE *file, uint32_t value);
static int write_u64(FILE *file, uint64_t value);
static int write_i64(FILE *file, int64_t value);
static int write_i32(FILE *file, int32_t value);
static int write_text(FILE *file, const char *text, size_t length);
static int read_u32(FILE *file, uint32_t *value);
static int read_u64(FILE *file, uint64_t *value);
static int read_i64(FILE *file, int64_t *value);
static int read_i32(FILE *file, int32_t *value);
static int read_text(FILE *file, char *buffer, size_t length);
static int save_to_file(const ParkingSystem *sys, FILE *file);
static ParkingStatus load_vehicles(FILE *file, ParkingSystem *loaded,
                                   uint64_t count, Id *max_id);
static ParkingStatus load_slots(FILE *file, ParkingSystem *loaded,
                                uint64_t count, Id *max_id);
static ParkingStatus load_sessions(FILE *file, ParkingSystem *loaded,
                                   uint64_t count, Id *max_id);
static int vehicle_exists(const ParkingSystem *sys, Id id);
static size_t slot_index_by_id(const ParkingSystem *sys, Id id);
static int relationships_valid(const ParkingSystem *sys);

// Functions
ParkingStatus storage_save(const ParkingSystem *sys, const char *path) {
    if (sys == NULL || path == NULL) {
        return PARKING_INVALID;
    }

    size_t path_length = strlen(path);
    size_t suffix_length = strlen(TMP_SUFFIX);
    if (path_length > SIZE_MAX - suffix_length - 1) {
        return PARKING_NO_MEMORY;
    }
    size_t length = path_length + suffix_length + 1;
    char *temporary = malloc(length);
    if (temporary == NULL) {
        return PARKING_NO_MEMORY;
    }
    snprintf(temporary, length, "%s%s", path, TMP_SUFFIX);

    FILE *file = fopen(temporary, "wb");
    if (file == NULL) {
        free(temporary);
        return PARKING_ERROR;
    }

    ParkingStatus status = PARKING_OK;
    if (save_to_file(sys, file) != 0) {
        status = PARKING_ERROR;
    }
    if (fclose(file) != 0 && status == PARKING_OK) {
        status = PARKING_ERROR;
    }

    if (status == PARKING_OK) {
        if (rename(temporary, path) != 0) {
            remove(temporary);
            status = PARKING_ERROR;
        }
    } else {
        remove(temporary);
    }

    free(temporary);
    return status;
}

static int save_to_file(const ParkingSystem *sys, FILE *file) {
    if (write_u32(file, STORAGE_MAGIC) != 0 ||
        write_u32(file, STORAGE_VERSION) != 0 ||
        write_i64(file, (int64_t)sys->next_id) != 0 ||
        write_u64(file, (uint64_t)sys->vehicle_count) != 0 ||
        write_u64(file, (uint64_t)sys->slot_count) != 0 ||
        write_u64(file, (uint64_t)sys->session_count) != 0) {
        return -1;
    }

    for (size_t i = 0; i < sys->vehicle_count; i++) {
        const Vehicle *vehicle = &sys->vehicles[i];
        if (write_i64(file, (int64_t)vehicle->id) != 0 ||
            write_i32(file, (int32_t)vehicle->user_type) != 0 ||
            write_i32(file, (int32_t)vehicle->vehicle_type) != 0 ||
            write_text(file, vehicle->registration,
                       sizeof vehicle->registration) != 0 ||
            write_text(file, vehicle->owner, sizeof vehicle->owner) != 0 ||
            write_text(file, vehicle->institution_id,
                       sizeof vehicle->institution_id) != 0) {
            return -1;
        }
    }

    for (size_t i = 0; i < sys->slot_count; i++) {
        const ParkingSlot *slot = &sys->slots[i];
        if (write_i64(file, (int64_t)slot->id) != 0 ||
            write_i64(file, (int64_t)slot->vehicle_id) != 0 ||
            write_i32(file, (int32_t)slot->available) != 0 ||
            write_i32(file, (int32_t)slot->section) != 0 ||
            write_i32(file, (int32_t)slot->type) != 0) {
            return -1;
        }
    }

    for (size_t i = 0; i < sys->session_count; i++) {
        const ParkingSession *session = &sys->sessions[i];
        if (write_i64(file, (int64_t)session->id) != 0 ||
            write_i64(file, (int64_t)session->vehicle_id) != 0 ||
            write_i64(file, (int64_t)session->slot_id) != 0 ||
            write_i64(file, (int64_t)session->start_time) != 0 ||
            write_i64(file, (int64_t)session->end_time) != 0 ||
            write_i32(file, (int32_t)session->charge) != 0 ||
            write_i32(file, (int32_t)session->active) != 0) {
            return -1;
        }
    }
    return 0;
}

ParkingStatus storage_load(ParkingSystem *sys, const char *path) {
    if (sys == NULL || path == NULL) {
        return PARKING_INVALID;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return errno == ENOENT ? PARKING_NOT_FOUND : PARKING_ERROR;
    }

    ParkingSystem loaded;
    memset(&loaded, 0, sizeof loaded);
    loaded.next_id = 1;
    ParkingStatus status = PARKING_OK;
    uint32_t magic = 0;
    uint32_t version = 0;

    if (read_u32(file, &magic) != 0 || read_u32(file, &version) != 0 ||
        magic != STORAGE_MAGIC || version != STORAGE_VERSION) {
        status = PARKING_INVALID;
        goto done;
    }

    int64_t next_id = 0;
    uint64_t vehicle_count = 0;
    uint64_t slot_count = 0;
    uint64_t session_count = 0;
    if (read_i64(file, &next_id) != 0 ||
        read_u64(file, &vehicle_count) != 0 ||
        read_u64(file, &slot_count) != 0 ||
        read_u64(file, &session_count) != 0) {
        status = PARKING_INVALID;
        goto done;
    }
    if (next_id < 1 ||
        vehicle_count > MAX_RECORDS ||
        slot_count == 0 || slot_count > MAX_SLOTS ||
        session_count > MAX_RECORDS ||
        vehicle_count > SIZE_MAX / sizeof(Vehicle) ||
        session_count > SIZE_MAX / sizeof(ParkingSession)) {
        status = PARKING_INVALID;
        goto done;
    }

    Id max_id = 0;
    status = load_vehicles(file, &loaded, vehicle_count, &max_id);
    if (status != PARKING_OK) {
        goto done;
    }
    status = load_slots(file, &loaded, slot_count, &max_id);
    if (status != PARKING_OK) {
        goto done;
    }
    status = load_sessions(file, &loaded, session_count, &max_id);
    if (status != PARKING_OK) {
        goto done;
    }
    if (!relationships_valid(&loaded)) {
        status = PARKING_INVALID;
        goto done;
    }

    int extra = fgetc(file);
    if (extra != EOF || ferror(file)) {
        status = PARKING_INVALID;
        goto done;
    }

    loaded.next_id = (Id)next_id;
    if (loaded.next_id <= max_id) {
        loaded.next_id = max_id + 1;
    }

    free(sys->vehicles);
    free(sys->slots);
    free(sys->sessions);
    *sys = loaded;
    loaded.vehicles = NULL;
    loaded.slots = NULL;
    loaded.sessions = NULL;
    status = PARKING_OK;

done:
    (void)fclose(file);
    if (status != PARKING_OK) {
        free(loaded.vehicles);
        free(loaded.slots);
        free(loaded.sessions);
    }
    return status;
}

ParkingStatus storage_preserve(const char *path, char *backup_path,
                               size_t backup_size) {
    if (path == NULL || backup_path == NULL || backup_size == 0) {
        return PARKING_INVALID;
    }

    FILE *probe = fopen(path, "rb");
    if (probe == NULL) {
        return PARKING_NOT_FOUND;
    }
    fclose(probe);

    for (int attempt = 0; attempt < BACKUP_ATTEMPTS; attempt++) {
        int written;
        if (attempt == 0) {
            written = snprintf(backup_path, backup_size, "%s.bak", path);
        } else {
            written = snprintf(backup_path, backup_size, "%s.bak.%d",
                               path, attempt);
        }
        if (written < 0 || (size_t)written >= backup_size) {
            return PARKING_ERROR;
        }

        FILE *existing = fopen(backup_path, "rb");
        if (existing != NULL) {
            fclose(existing);
            continue;
        }
        if (rename(path, backup_path) == 0) {
            return PARKING_OK;
        }
        return PARKING_ERROR;
    }
    return PARKING_ERROR;
}

static int write_u32(FILE *file, uint32_t value) {
    return fwrite(&value, sizeof value, 1, file) == 1 ? 0 : -1;
}

static int write_u64(FILE *file, uint64_t value) {
    return fwrite(&value, sizeof value, 1, file) == 1 ? 0 : -1;
}

static int write_i64(FILE *file, int64_t value) {
    return fwrite(&value, sizeof value, 1, file) == 1 ? 0 : -1;
}

static int write_i32(FILE *file, int32_t value) {
    return fwrite(&value, sizeof value, 1, file) == 1 ? 0 : -1;
}

static int write_text(FILE *file, const char *text, size_t length) {
    return fwrite(text, 1, length, file) == length ? 0 : -1;
}

static int read_u32(FILE *file, uint32_t *value) {
    return fread(value, sizeof *value, 1, file) == 1 ? 0 : -1;
}

static int read_u64(FILE *file, uint64_t *value) {
    return fread(value, sizeof *value, 1, file) == 1 ? 0 : -1;
}

static int read_i64(FILE *file, int64_t *value) {
    return fread(value, sizeof *value, 1, file) == 1 ? 0 : -1;
}

static int read_i32(FILE *file, int32_t *value) {
    return fread(value, sizeof *value, 1, file) == 1 ? 0 : -1;
}

static int read_text(FILE *file, char *buffer, size_t length) {
    if (fread(buffer, 1, length, file) != length) {
        return -1;
    }
    if (memchr(buffer, '\0', length) == NULL) {
        return -1;
    }
    size_t used = strlen(buffer);
    memset(buffer + used, 0, length - used);
    return 0;
}

static ParkingStatus load_vehicles(FILE *file, ParkingSystem *loaded,
                                   uint64_t count, Id *max_id) {
    if (count == 0) {
        return PARKING_OK;
    }
    loaded->vehicles = calloc((size_t)count, sizeof *loaded->vehicles);
    if (loaded->vehicles == NULL) {
        return PARKING_NO_MEMORY;
    }
    loaded->vehicle_count = (size_t)count;
    loaded->vehicle_capacity = (size_t)count;

    Id previous_id = 0;
    for (size_t i = 0; i < loaded->vehicle_count; i++) {
        Vehicle *vehicle = &loaded->vehicles[i];
        int64_t id = 0;
        int32_t user_type = 0;
        int32_t vehicle_type = 0;

        if (read_i64(file, &id) != 0 ||
            read_i32(file, &user_type) != 0 ||
            read_i32(file, &vehicle_type) != 0 ||
            read_text(file, vehicle->registration,
                      sizeof vehicle->registration) != 0 ||
            read_text(file, vehicle->owner, sizeof vehicle->owner) != 0 ||
            read_text(file, vehicle->institution_id,
                      sizeof vehicle->institution_id) != 0) {
            return PARKING_INVALID;
        }
        if (id <= previous_id || id > INT64_MAX - 2 ||
            user_type < USER_STUDENT || user_type > USER_VISITOR ||
            vehicle_type < VEHICLE_CAR || vehicle_type > VEHICLE_BICYCLE ||
            vehicle->registration[0] == '\0' ||
            vehicle->owner[0] == '\0' ||
            vehicle->institution_id[0] == '\0') {
            return PARKING_INVALID;
        }
        previous_id = (Id)id;

        vehicle->id = (Id)id;
        vehicle->user_type = (UserType)user_type;
        vehicle->vehicle_type = (VehicleType)vehicle_type;
        if (vehicle->id > *max_id) {
            *max_id = vehicle->id;
        }
    }
    return PARKING_OK;
}

static ParkingStatus load_slots(FILE *file, ParkingSystem *loaded,
                                uint64_t count, Id *max_id) {
    loaded->slots = calloc((size_t)count, sizeof *loaded->slots);
    if (loaded->slots == NULL) {
        return PARKING_NO_MEMORY;
    }
    loaded->slot_count = (size_t)count;

    Id previous_id = 0;
    for (size_t i = 0; i < loaded->slot_count; i++) {
        ParkingSlot *slot = &loaded->slots[i];
        int64_t id = 0;
        int64_t vehicle_id = ID_NONE;
        int32_t available = 0;
        int32_t section = 0;
        int32_t vtype = 0;

        if (read_i64(file, &id) != 0 ||
            read_i64(file, &vehicle_id) != 0 ||
            read_i32(file, &available) != 0 ||
            read_i32(file, &section) != 0 ||
            read_i32(file, &vtype) != 0) {
            return PARKING_INVALID;
        }
        if (id <= previous_id || id > INT64_MAX - 2 ||
            (available != 0 && available != 1) ||
            (available == 1 && vehicle_id != ID_NONE) ||
            (available == 0 && vehicle_id <= 0) ||
            section < 0 || section >= SECTION_COUNT ||
            vtype < VEHICLE_CAR || vtype > VEHICLE_BICYCLE) {
            return PARKING_INVALID;
        }
        previous_id = (Id)id;

        slot->id = (Id)id;
        slot->vehicle_id = (Id)vehicle_id;
        slot->available = available;
        slot->section = (ParkingSection)section;
        slot->type = (VehicleType)vtype;
        if (slot->id > *max_id) {
            *max_id = slot->id;
        }
    }
    return PARKING_OK;
}

static ParkingStatus load_sessions(FILE *file, ParkingSystem *loaded,
                                   uint64_t count, Id *max_id) {
    if (count == 0) {
        return PARKING_OK;
    }
    loaded->sessions = calloc((size_t)count, sizeof *loaded->sessions);
    if (loaded->sessions == NULL) {
        return PARKING_NO_MEMORY;
    }
    loaded->session_count = (size_t)count;
    loaded->session_capacity = (size_t)count;

    Id previous_id = 0;
    for (size_t i = 0; i < loaded->session_count; i++) {
        ParkingSession *session = &loaded->sessions[i];
        int64_t id = 0;
        int64_t vehicle_id = 0;
        int64_t slot_id = 0;
        int64_t start_time = 0;
        int64_t end_time = 0;
        int32_t charge = 0;
        int32_t active = 0;

        if (read_i64(file, &id) != 0 ||
            read_i64(file, &vehicle_id) != 0 ||
            read_i64(file, &slot_id) != 0 ||
            read_i64(file, &start_time) != 0 ||
            read_i64(file, &end_time) != 0 ||
            read_i32(file, &charge) != 0 ||
            read_i32(file, &active) != 0) {
            return PARKING_INVALID;
        }
        if (id <= previous_id || id > INT64_MAX - 2 || vehicle_id <= 0 ||
            slot_id <= 0 || charge < 0 ||
            (active != 0 && active != 1) ||
            (active == 1 && end_time != 0)) {
            return PARKING_INVALID;
        }
        previous_id = (Id)id;

        session->id = (Id)id;
        session->vehicle_id = (Id)vehicle_id;
        session->slot_id = (Id)slot_id;
        session->start_time = (time_t)start_time;
        session->end_time = (time_t)end_time;
        session->charge = charge;
        session->active = active;
        if (session->id > *max_id) {
            *max_id = session->id;
        }
    }
    return PARKING_OK;
}

static int vehicle_exists(const ParkingSystem *sys, Id id) {
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        if (sys->vehicles[i].id == id) {
            return 1;
        }
    }
    return 0;
}

static ParkingSection section_for_user_type_from_sys(UserType user_type) {
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

static size_t slot_index_by_id(const ParkingSystem *sys, Id id) {
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (sys->slots[i].id == id) {
            return i;
        }
    }
    return SIZE_MAX;
}

static int relationships_valid(const ParkingSystem *sys) {
    size_t active = 0;
    for (size_t i = 0; i < sys->session_count; i++) {
        if (sys->sessions[i].active) {
            active++;
        }
    }
    if (active > sys->slot_count) {
        return 0;
    }

    for (size_t i = 0; i < sys->session_count; i++) {
        const ParkingSession *session = &sys->sessions[i];
        if (!session->active) {
            continue;
        }
        if (!vehicle_exists(sys, session->vehicle_id)) {
            return 0;
        }
        size_t index = slot_index_by_id(sys, session->slot_id);
        if (index == SIZE_MAX) {
            return 0;
        }
        const ParkingSlot *slot = &sys->slots[index];
        if (slot->available || slot->vehicle_id != session->vehicle_id) {
            return 0;
        }
        /* The slot's section/type must match the vehicle's user_type/type. */
        const Vehicle *vehicle = NULL;
        for (size_t v = 0; v < sys->vehicle_count; v++) {
            if (sys->vehicles[v].id == session->vehicle_id) {
                vehicle = &sys->vehicles[v];
                break;
            }
        }
        if (vehicle == NULL) {
            return 0;
        }
        ParkingSection expected_section = section_for_user_type_from_sys(vehicle->user_type);
        if (slot->section != expected_section || slot->type != vehicle->vehicle_type) {
            return 0;
        }
        for (size_t j = i + 1; j < sys->session_count; j++) {
            const ParkingSession *other = &sys->sessions[j];
            if (other->active &&
                (other->vehicle_id == session->vehicle_id ||
                 other->slot_id == session->slot_id)) {
                return 0;
            }
        }
    }

    for (size_t i = 0; i < sys->slot_count; i++) {
        const ParkingSlot *slot = &sys->slots[i];
        if (slot->available) {
            continue;
        }
        if (!vehicle_exists(sys, slot->vehicle_id)) {
            return 0;
        }
        size_t matches = 0;
        for (size_t j = 0; j < sys->session_count; j++) {
            if (sys->sessions[j].active &&
                sys->sessions[j].slot_id == slot->id) {
                matches++;
            }
        }
        if (matches != 1) {
            return 0;
        }
    }
    return 1;
}
