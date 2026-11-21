#include "storage.h"
#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STORAGE_MAGIC 0x504B4453u /* "PKDS" */
#define STORAGE_VERSION 1u

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

Status storage_save(const ParkingSystem *sys, const char *path) {
    if (sys == NULL || path == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return STATUS_IO_ERROR;
    }

    int rc = STATUS_OK;
    if (write_u32(f, STORAGE_MAGIC) != 0 || write_u32(f, STORAGE_VERSION) != 0) {
        rc = STATUS_IO_ERROR;
        goto done;
    }

    if (write_i64(f, (int64_t)sys->vehicle_count) != 0) { rc = STATUS_IO_ERROR; goto done; }
    for (size_t i = 0; i < sys->vehicle_count && rc == STATUS_OK; i++) {
        const Vehicle *v = &sys->vehicles[i];
        if (write_i64(f, (int64_t)v->id) != 0 ||
            write_str(f, v->registration, MAX_REG_LEN) != 0 ||
            write_i64(f, (int64_t)v->type) != 0 ||
            write_str(f, v->owner, MAX_OWNER_LEN) != 0 ||
            write_i64(f, (int64_t)v->user_type) != 0) {
            rc = STATUS_IO_ERROR;
        }
    }

    if (write_i64(f, (int64_t)sys->slot_count) != 0) { rc = STATUS_IO_ERROR; goto done; }
    for (size_t i = 0; i < sys->slot_count && rc == STATUS_OK; i++) {
        const ParkingSlot *s = &sys->slots[i];
        if (write_i64(f, (int64_t)s->id) != 0 ||
            write_i64(f, (int64_t)s->zone) != 0 ||
            write_str(f, s->location, MAX_LOCATION_LEN) != 0 ||
            write_i64(f, (int64_t)s->is_available) != 0 ||
            write_i64(f, (int64_t)s->vehicle_id) != 0) {
            rc = STATUS_IO_ERROR;
        }
    }

    if (write_i64(f, (int64_t)sys->session_count) != 0) { rc = STATUS_IO_ERROR; goto done; }
    for (size_t i = 0; i < sys->session_count && rc == STATUS_OK; i++) {
        const ParkingSession *s = &sys->sessions[i];
        if (write_i64(f, (int64_t)s->id) != 0 ||
            write_i64(f, (int64_t)s->vehicle_id) != 0 ||
            write_i64(f, (int64_t)s->slot_id) != 0 ||
            write_i64(f, (int64_t)s->start_time) != 0 ||
            write_i64(f, (int64_t)s->end_time) != 0 ||
            write_i64(f, (int64_t)s->active) != 0) {
            rc = STATUS_IO_ERROR;
        }
    }

    if (write_i64(f, (int64_t)sys->reservation_count) != 0) { rc = STATUS_IO_ERROR; goto done; }
    for (size_t i = 0; i < sys->reservation_count && rc == STATUS_OK; i++) {
        const Reservation *r = &sys->reservations[i];
        if (write_i64(f, (int64_t)r->id) != 0 ||
            write_i64(f, (int64_t)r->vehicle_id) != 0 ||
            write_i64(f, (int64_t)r->slot_id) != 0 ||
            write_i64(f, (int64_t)r->start_time) != 0 ||
            write_i64(f, (int64_t)r->end_time) != 0 ||
            write_i64(f, (int64_t)r->status) != 0) {
            rc = STATUS_IO_ERROR;
        }
    }

    /* Next-ID counters so new IDs continue after a load. */
    if (rc == STATUS_OK) {
        if (write_i64(f, (int64_t)sys->next_vehicle_id) != 0 ||
            write_i64(f, (int64_t)sys->next_slot_id) != 0 ||
            write_i64(f, (int64_t)sys->next_session_id) != 0 ||
            write_i64(f, (int64_t)sys->next_reservation_id) != 0) {
            rc = STATUS_IO_ERROR;
        }
    }

done:
    if (fclose(f) != 0 && rc == STATUS_OK) {
        rc = STATUS_IO_ERROR;
    }
    return rc;
}

Status storage_load(ParkingSystem *sys, const char *path) {
    if (sys == NULL || path == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return STATUS_IO_ERROR;
    }

    int rc = STATUS_OK;
    uint32_t magic = 0;
    uint32_t version = 0;
    if (read_u32(f, &magic) != 0 || magic != STORAGE_MAGIC ||
        read_u32(f, &version) != 0 || version != STORAGE_VERSION) {
        rc = STATUS_IO_ERROR;
        goto done;
    }

    system_reset(sys);

    int64_t count = 0;
    if (read_i64(f, &count) != 0 || count < 0) { rc = STATUS_IO_ERROR; goto done; }
    if (count > 0) {
        sys->vehicle_count = (size_t)count;
        sys->vehicle_capacity = (size_t)count;
        sys->vehicles = calloc(sys->vehicle_capacity, sizeof(Vehicle));
        if (sys->vehicles == NULL) { rc = STATUS_OUT_OF_MEMORY; goto done; }
        for (size_t i = 0; i < sys->vehicle_count; i++) {
            Vehicle *v = &sys->vehicles[i];
            int64_t tmp = 0;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            v->id = (Id)tmp;
            if (read_str(f, v->registration, MAX_REG_LEN) != 0) { rc = STATUS_IO_ERROR; goto done; }
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            v->type = (VehicleType)(int)tmp;
            if (read_str(f, v->owner, MAX_OWNER_LEN) != 0) { rc = STATUS_IO_ERROR; goto done; }
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            v->user_type = (UserType)(int)tmp;
        }
    }

    if (read_i64(f, &count) != 0 || count < 0) { rc = STATUS_IO_ERROR; goto done; }
    if (count > 0) {
        sys->slot_count = (size_t)count;
        sys->slot_capacity = (size_t)count;
        sys->slots = calloc(sys->slot_capacity, sizeof(ParkingSlot));
        if (sys->slots == NULL) { rc = STATUS_OUT_OF_MEMORY; goto done; }
        for (size_t i = 0; i < sys->slot_count; i++) {
            ParkingSlot *s = &sys->slots[i];
            int64_t tmp = 0;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->id = (Id)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->zone = (ZoneType)(int)tmp;
            if (read_str(f, s->location, MAX_LOCATION_LEN) != 0) { rc = STATUS_IO_ERROR; goto done; }
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->is_available = (int)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->vehicle_id = (Id)tmp;
        }
    }

    if (read_i64(f, &count) != 0 || count < 0) { rc = STATUS_IO_ERROR; goto done; }
    if (count > 0) {
        sys->session_count = (size_t)count;
        sys->session_capacity = (size_t)count;
        sys->sessions = calloc(sys->session_capacity, sizeof(ParkingSession));
        if (sys->sessions == NULL) { rc = STATUS_OUT_OF_MEMORY; goto done; }
        for (size_t i = 0; i < sys->session_count; i++) {
            ParkingSession *s = &sys->sessions[i];
            int64_t tmp = 0;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->id = (Id)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->vehicle_id = (Id)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->slot_id = (Id)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->start_time = (time_t)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->end_time = (time_t)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            s->active = (int)tmp;
        }
    }

    if (read_i64(f, &count) != 0 || count < 0) { rc = STATUS_IO_ERROR; goto done; }
    if (count > 0) {
        sys->reservation_count = (size_t)count;
        sys->reservation_capacity = (size_t)count;
        sys->reservations = calloc(sys->reservation_capacity, sizeof(Reservation));
        if (sys->reservations == NULL) { rc = STATUS_OUT_OF_MEMORY; goto done; }
        for (size_t i = 0; i < sys->reservation_count; i++) {
            Reservation *r = &sys->reservations[i];
            int64_t tmp = 0;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            r->id = (Id)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            r->vehicle_id = (Id)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            r->slot_id = (Id)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            r->start_time = (time_t)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            r->end_time = (time_t)tmp;
            if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
            r->status = (ReservationStatus)(int)tmp;
        }
    }

    /* Next-ID counters. */
    int64_t tmp = 0;
    if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
    sys->next_vehicle_id = (Id)tmp;
    if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
    sys->next_slot_id = (Id)tmp;
    if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
    sys->next_session_id = (Id)tmp;
    if (read_i64(f, &tmp) != 0) { rc = STATUS_IO_ERROR; goto done; }
    sys->next_reservation_id = (Id)tmp;

done:
    if (fclose(f) != 0 && rc == STATUS_OK) {
        rc = STATUS_IO_ERROR;
    }
    return rc;
}
