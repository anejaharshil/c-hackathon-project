#include "storage.h"
#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STORAGE_MAGIC 0x504B4453u /* "PKDS" */
#define STORAGE_VERSION 1u

/* Maximum records accepted on load, to guard against absurd count fields
 * that would cause huge allocations or integer overflow. */
#define MAX_RECORDS 1000000u

/* Temporary file suffix for atomic save. */
#define TMP_SUFFIX ".tmp"

/* Binary .dat layout. Raw pointers are never persisted; only stable IDs and
 * fixed-width record fields. Strings are stored as fixed-length buffers. */

static int write_u32(FILE *f, uint32_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}

static int write_i64(FILE *f, int64_t v) {
    return fwrite(&v, sizeof(v), 1, f) == 1 ? 0 : -1;
}

static int write_str(FILE *f, const char *s, size_t len) {
    if (fwrite(s, 1, len, f) != len) {
        return -1;
    }
    return 0;
}

static int read_u32(FILE *f, uint32_t *v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}

static int read_i64(FILE *f, int64_t *v) {
    return fread(v, sizeof(*v), 1, f) == 1 ? 0 : -1;
}

static int read_str(FILE *f, char *s, size_t len) {
    if (fread(s, 1, len, f) != len) {
        return -1;
    }
    return 0;
}

/* Validate an enum value read from disk. Returns 0 if out of range. */
static int enum_in_range(int64_t v, int64_t lo, int64_t hi) {
    return v >= lo && v <= hi;
}

/* ---------------- save (atomic via temp file) ---------------- */

static int save_to_file(const ParkingSystem *sys, FILE *f) {
    if (write_u32(f, STORAGE_MAGIC) != 0 || write_u32(f, STORAGE_VERSION) != 0) {
        return -1;
    }

    if (write_i64(f, (int64_t)sys->vehicle_count) != 0) return -1;
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        const Vehicle *v = &sys->vehicles[i];
        if (write_i64(f, (int64_t)v->id) != 0 ||
            write_str(f, v->registration, MAX_REG_LEN) != 0 ||
            write_i64(f, (int64_t)v->type) != 0 ||
            write_str(f, v->owner, MAX_OWNER_LEN) != 0 ||
            write_i64(f, (int64_t)v->user_type) != 0) {
            return -1;
        }
    }

    if (write_i64(f, (int64_t)sys->slot_count) != 0) return -1;
    for (size_t i = 0; i < sys->slot_count; i++) {
        const ParkingSlot *s = &sys->slots[i];
        if (write_i64(f, (int64_t)s->id) != 0 ||
            write_i64(f, (int64_t)s->zone) != 0 ||
            write_str(f, s->location, MAX_LOCATION_LEN) != 0 ||
            write_i64(f, (int64_t)s->is_available) != 0 ||
            write_i64(f, (int64_t)s->vehicle_id) != 0) {
            return -1;
        }
    }

    if (write_i64(f, (int64_t)sys->session_count) != 0) return -1;
    for (size_t i = 0; i < sys->session_count; i++) {
        const ParkingSession *s = &sys->sessions[i];
        if (write_i64(f, (int64_t)s->id) != 0 ||
            write_i64(f, (int64_t)s->vehicle_id) != 0 ||
            write_i64(f, (int64_t)s->slot_id) != 0 ||
            write_i64(f, (int64_t)s->start_time) != 0 ||
            write_i64(f, (int64_t)s->end_time) != 0 ||
            write_i64(f, (int64_t)s->active) != 0) {
            return -1;
        }
    }

    if (write_i64(f, (int64_t)sys->reservation_count) != 0) return -1;
    for (size_t i = 0; i < sys->reservation_count; i++) {
        const Reservation *r = &sys->reservations[i];
        if (write_i64(f, (int64_t)r->id) != 0 ||
            write_i64(f, (int64_t)r->vehicle_id) != 0 ||
            write_i64(f, (int64_t)r->slot_id) != 0 ||
            write_i64(f, (int64_t)r->start_time) != 0 ||
            write_i64(f, (int64_t)r->end_time) != 0 ||
            write_i64(f, (int64_t)r->status) != 0) {
            return -1;
        }
    }

    /* Next-ID counters so new IDs continue after a load. */
    if (write_i64(f, (int64_t)sys->next_vehicle_id) != 0 ||
        write_i64(f, (int64_t)sys->next_slot_id) != 0 ||
        write_i64(f, (int64_t)sys->next_session_id) != 0 ||
        write_i64(f, (int64_t)sys->next_reservation_id) != 0) {
        return -1;
    }
    return 0;
}

Status storage_save(const ParkingSystem *sys, const char *path) {
    if (sys == NULL || path == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    /* Build temp path: path + ".tmp". */
    size_t path_len = strlen(path);
    size_t tmp_len = path_len + strlen(TMP_SUFFIX) + 1;
    char *tmp_path = malloc(tmp_len);
    if (tmp_path == NULL) {
        return STATUS_OUT_OF_MEMORY;
    }
    snprintf(tmp_path, tmp_len, "%s%s", path, TMP_SUFFIX);

    FILE *f = fopen(tmp_path, "wb");
    if (f == NULL) {
        free(tmp_path);
        return STATUS_IO_ERROR;
    }

    int rc = STATUS_OK;
    if (save_to_file(sys, f) != 0) {
        rc = STATUS_IO_ERROR;
    }
    if (fclose(f) != 0 && rc == STATUS_OK) {
        rc = STATUS_IO_ERROR;
    }

    if (rc == STATUS_OK) {
        /* Atomic replace: temp -> original. */
        if (rename(tmp_path, path) != 0) {
            remove(tmp_path);
            rc = STATUS_IO_ERROR;
        }
    } else {
        remove(tmp_path);
    }

    free(tmp_path);
    return rc;
}

/* ---------------- load (safe: original untouched on failure) ---------------- */

static Status load_vehicles(FILE *f, ParkingSystem *sys, int64_t count) {
    if (count < 0 || (uint64_t)count > MAX_RECORDS) {
        return STATUS_IO_ERROR;
    }
    if (count == 0) {
        return STATUS_OK;
    }
    sys->vehicle_count = (size_t)count;
    sys->vehicle_capacity = (size_t)count;
    sys->vehicles = calloc(sys->vehicle_capacity, sizeof(Vehicle));
    if (sys->vehicles == NULL) {
        return STATUS_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        Vehicle *v = &sys->vehicles[i];
        int64_t tmp = 0;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        v->id = (Id)tmp;
        if (read_str(f, v->registration, MAX_REG_LEN) != 0) return STATUS_IO_ERROR;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        if (!enum_in_range(tmp, 0, 2)) return STATUS_IO_ERROR;
        v->type = (VehicleType)(int)tmp;
        if (read_str(f, v->owner, MAX_OWNER_LEN) != 0) return STATUS_IO_ERROR;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        if (!enum_in_range(tmp, 0, 3)) return STATUS_IO_ERROR;
        v->user_type = (UserType)(int)tmp;
    }
    return STATUS_OK;
}

static Status load_slots(FILE *f, ParkingSystem *sys, int64_t count) {
    if (count < 0 || (uint64_t)count > MAX_RECORDS) {
        return STATUS_IO_ERROR;
    }
    if (count == 0) {
        return STATUS_OK;
    }
    sys->slot_count = (size_t)count;
    sys->slot_capacity = (size_t)count;
    sys->slots = calloc(sys->slot_capacity, sizeof(ParkingSlot));
    if (sys->slots == NULL) {
        return STATUS_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < sys->slot_count; i++) {
        ParkingSlot *s = &sys->slots[i];
        int64_t tmp = 0;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->id = (Id)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        if (!enum_in_range(tmp, 0, 4)) return STATUS_IO_ERROR;
        s->zone = (ZoneType)(int)tmp;
        if (read_str(f, s->location, MAX_LOCATION_LEN) != 0) return STATUS_IO_ERROR;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->is_available = (tmp != 0) ? 1 : 0;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->vehicle_id = (Id)tmp;
    }
    return STATUS_OK;
}

static Status load_sessions(FILE *f, ParkingSystem *sys, int64_t count) {
    if (count < 0 || (uint64_t)count > MAX_RECORDS) {
        return STATUS_IO_ERROR;
    }
    if (count == 0) {
        return STATUS_OK;
    }
    sys->session_count = (size_t)count;
    sys->session_capacity = (size_t)count;
    sys->sessions = calloc(sys->session_capacity, sizeof(ParkingSession));
    if (sys->sessions == NULL) {
        return STATUS_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < sys->session_count; i++) {
        ParkingSession *s = &sys->sessions[i];
        int64_t tmp = 0;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->id = (Id)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->vehicle_id = (Id)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->slot_id = (Id)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->start_time = (time_t)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->end_time = (time_t)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        s->active = (tmp != 0) ? 1 : 0;
    }
    return STATUS_OK;
}

static Status load_reservations(FILE *f, ParkingSystem *sys, int64_t count) {
    if (count < 0 || (uint64_t)count > MAX_RECORDS) {
        return STATUS_IO_ERROR;
    }
    if (count == 0) {
        return STATUS_OK;
    }
    sys->reservation_count = (size_t)count;
    sys->reservation_capacity = (size_t)count;
    sys->reservations = calloc(sys->reservation_capacity, sizeof(Reservation));
    if (sys->reservations == NULL) {
        return STATUS_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < sys->reservation_count; i++) {
        Reservation *r = &sys->reservations[i];
        int64_t tmp = 0;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        r->id = (Id)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        r->vehicle_id = (Id)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        r->slot_id = (Id)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        r->start_time = (time_t)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        r->end_time = (time_t)tmp;
        if (read_i64(f, &tmp) != 0) return STATUS_IO_ERROR;
        if (!enum_in_range(tmp, 0, 3)) return STATUS_IO_ERROR;
        r->status = (ReservationStatus)(int)tmp;
    }
    return STATUS_OK;
}

/* Load into a fresh temporary system. On success, swap contents into sys
 * so the original is never left partially destroyed on failure. */
Status storage_load(ParkingSystem *sys, const char *path) {
    if (sys == NULL || path == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return STATUS_IO_ERROR;
    }

    /* Temporary system built during load. Declared before any goto done so
     * that every error path can safely free its (possibly NULL) members. */
    ParkingSystem tmp;
    tmp.vehicles = NULL; tmp.vehicle_count = 0; tmp.vehicle_capacity = 0;
    tmp.slots = NULL; tmp.slot_count = 0; tmp.slot_capacity = 0;
    tmp.sessions = NULL; tmp.session_count = 0; tmp.session_capacity = 0;
    tmp.reservations = NULL; tmp.reservation_count = 0; tmp.reservation_capacity = 0;
    tmp.next_vehicle_id = 1;
    tmp.next_slot_id = 1;
    tmp.next_session_id = 1;
    tmp.next_reservation_id = 1;

    int rc = STATUS_OK;
    uint32_t magic = 0;
    uint32_t version = 0;
    if (read_u32(f, &magic) != 0 || magic != STORAGE_MAGIC ||
        read_u32(f, &version) != 0 || version != STORAGE_VERSION) {
        rc = STATUS_IO_ERROR;
        goto done;
    }

    int64_t count = 0;
    if (read_i64(f, &count) != 0 || count < 0) { rc = STATUS_IO_ERROR; goto done; }
    if ((rc = load_vehicles(f, &tmp, count)) != STATUS_OK) goto done;

    if (read_i64(f, &count) != 0 || count < 0) { rc = STATUS_IO_ERROR; goto done; }
    if ((rc = load_slots(f, &tmp, count)) != STATUS_OK) goto done;

    if (read_i64(f, &count) != 0 || count < 0) { rc = STATUS_IO_ERROR; goto done; }
    if ((rc = load_sessions(f, &tmp, count)) != STATUS_OK) goto done;

    if (read_i64(f, &count) != 0 || count < 0) { rc = STATUS_IO_ERROR; goto done; }
    if ((rc = load_reservations(f, &tmp, count)) != STATUS_OK) goto done;

    /* Next-ID counters (preserved from the saved file). */
    int64_t idtmp = 0;
    if (read_i64(f, &idtmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
    tmp.next_vehicle_id = (Id)idtmp;
    if (read_i64(f, &idtmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
    tmp.next_slot_id = (Id)idtmp;
    if (read_i64(f, &idtmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
    tmp.next_session_id = (Id)idtmp;
    if (read_i64(f, &idtmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
    tmp.next_reservation_id = (Id)idtmp;

    /* Success: swap into sys. Free sys's old arrays first. */
    free(sys->vehicles);
    free(sys->slots);
    free(sys->sessions);
    free(sys->reservations);
    *sys = tmp;

done:
    if (fclose(f) != 0 && rc == STATUS_OK) {
        rc = STATUS_IO_ERROR;
    }
    if (rc != STATUS_OK) {
        /* Temp load failed: free any partial temp allocations, leave sys intact. */
        free(tmp.vehicles);
        free(tmp.slots);
        free(tmp.sessions);
        free(tmp.reservations);
    }
    return rc;
}
