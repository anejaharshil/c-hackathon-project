#include "config.h"
#include "system.h"
#include "vehicle.h"
#include "slot.h"
#include "session.h"
#include "billing.h"
#include "storage.h"
#include "analytics.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Menu choices. */
#define MENU_REGISTER   1  /* Register Vehicle */
#define MENU_ENTRY      2  /* Vehicle Entry */
#define MENU_EXIT       3  /* Vehicle Exit */
#define MENU_SEARCH     4  /* Search Vehicle */
#define MENU_STATUS     5  /* Display Parking Status */
#define MENU_STATS      6  /* Parking Statistics */
#define MENU_SAVE       7  /* Save Data */
#define MENU_SAVE_EXIT  0  /* Save and Exit */

/* Input buffer sizes. */
#define LINE_BUF  128
#define NUM_BUF   16

/* ---------------- input helpers ---------------- */

/* Read a line from stdin using fgets(). Strip trailing newline.
 * Returns 1 on success, 0 on EOF or read error. */
static int read_line(char *buf, size_t size) {
    if (fgets(buf, (int)size, stdin) == NULL) {
        return 0;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }
    return 1;
}

/* Skip leading whitespace, return pointer to first non-space char. */
static char *skip_spaces(char *s) {
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

/* Read a non-empty trimmed string with a prompt.
 * Returns 1 on success, 0 on EOF or empty input. */
static int read_string(char *buf, size_t size, const char *prompt) {
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!read_line(buf, size)) {
            return 0; /* EOF */
        }
        char *p = skip_spaces(buf);
        if (*p == '\0') {
            printf("  Input cannot be empty. Try again.\n");
            continue;
        }
        if (p != buf) {
            memmove(buf, p, strlen(p) + 1);
        }
        return 1;
    }
}

/* Read an integer with full validation. Accepts optional leading/trailing
 * whitespace. Rejects empty input, non-numeric characters, and overflow.
 * Returns 1 on success, 0 on EOF or invalid input. */
static int read_integer(int *out, const char *prompt) {
    char buf[NUM_BUF];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!read_line(buf, NUM_BUF)) {
            return 0; /* EOF */
        }
        char *p = skip_spaces(buf);
        if (*p == '\0') {
            printf("  Input cannot be empty. Try again.\n");
            continue;
        }
        char *end = NULL;
        long val = strtol(p, &end, 10);
        if (end == p || *end != '\0') {
            printf("  Invalid number. Try again.\n");
            continue;
        }
        if (val < 0 || val > 999999) {
            printf("  Number out of range. Try again.\n");
            continue;
        }
        *out = (int)val;
        return 1;
    }
}

/* Pause for user acknowledgement. */
static void pause_for_user(void) {
    printf("\nPress Enter to continue...");
    fflush(stdout);
    char dummy[LINE_BUF];
    (void)read_line(dummy, sizeof(dummy));
}

/* Check whether a file exists and is readable. Returns 1 if it exists, 0
 * otherwise (missing, or unreadable for any other reason). */
static int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

/* ---------------- zone mapping ---------------- */

/* Map a vehicle user type to a preferred parking zone.
 * ZONE_ACCESSIBLE is never a preferred zone; it is reserved for
 * mobility-impaired users and must never be handed out as overflow. */
static ZoneType zone_for_user_type(UserType ut) {
    switch (ut) {
        case USER_STUDENT: return ZONE_C;
        case USER_STAFF:   return ZONE_A;
        case USER_VISITOR: return ZONE_VISITOR;
        case USER_ADMIN:   return ZONE_A;
        default:           return ZONE_A;
    }
}

static const char *zone_name(ZoneType z) {
    switch (z) {
        case ZONE_A:        return "Zone A";
        case ZONE_B:        return "Zone B";
        case ZONE_C:        return "Zone C";
        case ZONE_ACCESSIBLE: return "Zone Accessible";
        case ZONE_VISITOR:  return "Zone Visitor";
        default:            return "Unknown";
    }
}

static const char *vehicle_type_name(VehicleType vt) {
    switch (vt) {
        case VEHICLE_CAR:       return "Car";
        case VEHICLE_MOTORCYCLE: return "Motorcycle";
        case VEHICLE_BICYCLE:   return "Bicycle";
        default:                return "Unknown";
    }
}

static const char *user_type_name(UserType ut) {
    switch (ut) {
        case USER_STUDENT: return "Student";
        case USER_STAFF:   return "Staff";
        case USER_VISITOR: return "Visitor";
        case USER_ADMIN:   return "Admin";
        default:           return "Unknown";
    }
}

static const char *status_text(Status st) {
    switch (st) {
        case STATUS_OK:             return "OK";
        case STATUS_ERROR:          return "Error";
        case STATUS_NOT_FOUND:      return "Not Found";
        case STATUS_ALREADY_EXISTS: return "Already Exists";
        case STATUS_INVALID_ARGUMENT: return "Invalid Argument";
        case STATUS_OUT_OF_MEMORY:  return "Out of Memory";
        case STATUS_CONFLICT:       return "Conflict";
        case STATUS_IO_ERROR:       return "I/O Error";
        default:                    return "Unknown";
    }
}

/* Try to find a free slot. The preferred zone is tried first.
 * ZONE_ACCESSIBLE is never used as overflow: it is reserved for
 * mobility-impaired users only. Returns 1 on success, 0 if no slot. */
static int find_free_slot(const ParkingSystem *sys, ZoneType preferred,
                          size_t *out_index) {
    /* Non-accessible zones in preference order for overflow. */
    ZoneType fallback_zones[4] = { ZONE_A, ZONE_B, ZONE_C, ZONE_VISITOR };

    /* Try the user's preferred zone first. */
    if (slot_find_free(sys, preferred, out_index) == STATUS_OK) {
        return 1;
    }

    /* Overflow: try remaining non-accessible zones, skipping the preferred
     * zone (already tried) and never touching ZONE_ACCESSIBLE. */
    for (int i = 0; i < 4; i++) {
        if (fallback_zones[i] == preferred) {
            continue;
        }
        if (slot_find_free(sys, fallback_zones[i], out_index) == STATUS_OK) {
            return 1;
        }
    }
    return 0;
}

/* ---------------- menu actions ---------------- */

static int do_register_vehicle(ParkingSystem *sys) {
    char reg[MAX_REG_LEN];
    char owner[MAX_OWNER_LEN];
    int vt = 0, ut = 0;
    Id vid = ID_NONE;

    printf("\n--- Register Vehicle ---\n");
    if (!read_string(reg, sizeof(reg), "Registration: ")) {
        printf("  Cancelled (EOF).\n");
        return 1;
    }
    if (!read_string(owner, sizeof(owner), "Owner name: ")) {
        printf("  Cancelled (EOF).\n");
        return 1;
    }
    if (!read_integer(&vt,
            "Vehicle type (0=Car, 1=Motorcycle, 2=Bicycle): ")) {
        printf("  Cancelled (EOF).\n");
        return 1;
    }
    if (!read_integer(&ut,
            "User category (0=Student, 1=Staff, 2=Visitor, 3=Admin): ")) {
        printf("  Cancelled (EOF).\n");
        return 1;
    }

    Status st = vehicle_register(sys, reg, (VehicleType)vt, owner,
                                 (UserType)ut, &vid);
    if (st != STATUS_OK) {
        printf("  Registration failed: %s\n", status_text(st));
        pause_for_user();
        return 1;
    }
    printf("  Vehicle registered. ID=%lld\n", (long long)vid);
    pause_for_user();
    return 1;
}

static int do_vehicle_entry(ParkingSystem *sys) {
    char reg[MAX_REG_LEN];
    size_t vidx = 0;
    size_t sidx = 0;
    Id sid = ID_NONE;

    printf("\n--- Vehicle Entry ---\n");
    if (!read_string(reg, sizeof(reg), "Registration: ")) {
        printf("  Cancelled (EOF).\n");
        return 1;
    }

    Status st = vehicle_find_by_reg(sys, reg, &vidx);
    if (st != STATUS_OK) {
        printf("  Vehicle not found: %s\n", status_text(st));
        pause_for_user();
        return 1;
    }
    const Vehicle *v = vehicle_get(sys, vidx);

    /* One active session per vehicle. */
    size_t dummy = 0;
    if (session_find_active_for_vehicle(sys, v->id, &dummy) == STATUS_OK) {
        printf("  Vehicle already has an active session.\n");
        pause_for_user();
        return 1;
    }

    ZoneType preferred = zone_for_user_type(v->user_type);
    if (!find_free_slot(sys, preferred, &sidx)) {
        printf("  No free slots available.\n");
        pause_for_user();
        return 1;
    }
    const ParkingSlot *slot = slot_get(sys, sidx);
    if (slot == NULL) {
        printf("  Internal error: slot lookup failed.\n");
        pause_for_user();
        return 1;
    }

    st = session_start(sys, v->id, slot->id, time(NULL), &sid);
    if (st != STATUS_OK) {
        printf("  Entry failed: %s\n", status_text(st));
        pause_for_user();
        return 1;
    }
    printf("  Entry granted. Slot=%s (ID=%lld), Entry time=%ld\n",
           zone_name(slot->zone), (long long)slot->id,
           (long)time(NULL));
    pause_for_user();
    return 1;
}

static int do_vehicle_exit(ParkingSystem *sys) {
    char reg[MAX_REG_LEN];
    size_t vidx = 0;
    size_t sidx = 0;
    Id sid = ID_NONE;

    printf("\n--- Vehicle Exit ---\n");
    if (!read_string(reg, sizeof(reg), "Registration: ")) {
        printf("  Cancelled (EOF).\n");
        return 1;
    }

    Status st = vehicle_find_by_reg(sys, reg, &vidx);
    if (st != STATUS_OK) {
        printf("  Vehicle not found: %s\n", status_text(st));
        pause_for_user();
        return 1;
    }
    const Vehicle *v = vehicle_get(sys, vidx);

    st = session_find_active_for_vehicle(sys, v->id, &sidx);
    if (st != STATUS_OK) {
        printf("  No active session for this vehicle: %s\n", status_text(st));
        pause_for_user();
        return 1;
    }
    const ParkingSession *sess = session_get(sys, sidx);
    if (sess == NULL) {
        printf("  Internal error: session lookup failed.\n");
        pause_for_user();
        return 1;
    }
    sid = sess->id;

    /* Close the session first (sets end_time, releases slot). */
    time_t now = time(NULL);
    st = session_end(sys, v->id, now);
    if (st != STATUS_OK) {
        printf("  Session close failed: %s\n", status_text(st));
        pause_for_user();
        return 1;
    }

    /* Billing requires a completed session (end_time set, active=0).
     * billing_compute() is the authoritative calculation. */
    long cost = 0;
    st = billing_compute(sys, sid, &cost);
    if (st != STATUS_OK) {
        printf("  Billing failed: %s\n", status_text(st));
        pause_for_user();
        return 1;
    }

    char bill[256];
    int n = billing_format(sys, sid, bill, sizeof(bill));
    (void)n;
    if (n > 0) {
        printf("  %s\n", bill);
    }
    printf("  Total charge: %ld minor units\n", cost);
    pause_for_user();
    return 1;
}

static int do_search_vehicle(ParkingSystem *sys) {
    char reg[MAX_REG_LEN];
    size_t vidx = 0;

    printf("\n--- Search Vehicle ---\n");
    if (!read_string(reg, sizeof(reg), "Registration: ")) {
        printf("  Cancelled (EOF).\n");
        return 1;
    }

    Status st = vehicle_find_by_reg(sys, reg, &vidx);
    if (st != STATUS_OK) {
        printf("  Vehicle not found: %s\n", status_text(st));
        pause_for_user();
        return 1;
    }
    const Vehicle *v = vehicle_get(sys, vidx);
    if (v == NULL) {
        printf("  Internal error: vehicle lookup failed.\n");
        pause_for_user();
        return 1;
    }

    printf("  ID: %lld\n", (long long)v->id);
    printf("  Registration: %s\n", v->registration);
    printf("  Type: %s\n", vehicle_type_name(v->type));
    printf("  Owner: %s\n", v->owner);
    printf("  User category: %s\n", user_type_name(v->user_type));

    /* Check active session. */
    size_t sidx = 0;
    if (session_find_active_for_vehicle(sys, v->id, &sidx) == STATUS_OK) {
        const ParkingSession *sess = session_get(sys, sidx);
        if (sess != NULL) {
            size_t slot_index = 0;
            if (slot_find_by_id(sys, sess->slot_id, &slot_index) == STATUS_OK) {
                const ParkingSlot *slot = slot_get(sys, slot_index);
                if (slot != NULL) {
                    printf("  Status: PARKED\n");
                    printf("  Slot: %s (ID=%lld)\n",
                           zone_name(slot->zone), (long long)slot->id);
                    printf("  Entry time: %ld\n", (long)sess->start_time);
                }
            }
        }
    } else {
        printf("  Status: Not parked\n");
    }
    pause_for_user();
    return 1;
}

static int do_parking_status(const ParkingSystem *sys) {
    size_t total = slot_count(sys);
    size_t occupied = analytics_occupied_slots(sys);
    size_t available = total > occupied ? total - occupied : 0;

    printf("\n--- Parking Status ---\n");
    printf("  Total slots: %zu\n", total);
    printf("  Occupied:    %zu\n", occupied);
    printf("  Available:   %zu\n", available);

    /* Per-zone breakdown. */
    ZoneType zones[5] = { ZONE_A, ZONE_B, ZONE_C, ZONE_ACCESSIBLE, ZONE_VISITOR };
    for (int i = 0; i < 5; i++) {
        size_t ztotal = 0, zocc = 0;
        for (size_t j = 0; j < total; j++) {
            const ParkingSlot *s = slot_get(sys, j);
            if (s == NULL || s->zone != zones[i]) continue;
            ztotal++;
            if (!s->is_available) zocc++;
        }
        if (ztotal > 0) {
            printf("  %s: %zu/%zu occupied\n", zone_name(zones[i]), zocc, ztotal);
        }
    }
    pause_for_user();
    return 1;
}

static int do_parking_statistics(const ParkingSystem *sys) {
    size_t total = slot_count(sys);
    size_t occupied = analytics_occupied_slots(sys);
    size_t available = total > occupied ? total - occupied : 0;
    size_t active = analytics_active_sessions(sys);
    size_t completed = analytics_completed_sessions(sys);
    long revenue = analytics_total_revenue(sys);

    printf("\n--- Parking Statistics ---\n");
    printf("  Total slots:       %zu\n", total);
    printf("  Occupied slots:    %zu\n", occupied);
    printf("  Available slots:   %zu\n", available);
    printf("  Active sessions:   %zu\n", active);
    printf("  Completed sessions:%zu\n", completed);
    printf("  Total revenue:     %ld minor units\n", revenue);
    pause_for_user();
    return 1;
}

static int do_save_data(ParkingSystem *sys) {
    printf("\n--- Save Data ---\n");
    Status st = storage_save(sys, CONFIG_DATA_PATH);
    if (st != STATUS_OK) {
        printf("  Save failed: %s\n", status_text(st));
    } else {
        printf("  State saved to %s\n", CONFIG_DATA_PATH);
    }
    pause_for_user();
    return 1;
}

/* ---------------- menu display ---------------- */

static void print_menu(void) {
    printf("\n");
    printf("SMART CAMPUS PARKING SYSTEM\n");
    printf("===========================\n");
    printf("1. Register Vehicle\n");
    printf("2. Vehicle Entry\n");
    printf("3. Vehicle Exit\n");
    printf("4. Search Vehicle\n");
    printf("5. Display Parking Status\n");
    printf("6. Parking Statistics\n");
    printf("7. Save Data\n");
    printf("0. Save and Exit\n");
    printf("---------------------------\n");
}

/* ---------------- main ---------------- */

int main(void) {
    if (config_init() != STATUS_OK) {
        fprintf(stderr, "Configuration initialisation failed.\n");
        return 1;
    }

    ParkingSystem *sys = NULL;
    if (system_create(&sys) != STATUS_OK) {
        fprintf(stderr, "Failed to create parking system.\n");
        return 1;
    }

    /* Attempt to load saved state.
     * Distinguish between "file does not exist" (legitimate first run) and
     * "file exists but is invalid" (loading failure; do not silently
     * overwrite with a fresh system). */
    int have_file = file_exists(CONFIG_DATA_PATH);
    Status st = storage_load(sys, CONFIG_DATA_PATH);
    if (!have_file) {
        /* No save file: legitimate first run. Initialise fresh slots. */
        if (slot_init(sys, CONFIG_SLOT_CAPACITY) != STATUS_OK) {
            fprintf(stderr, "Failed to initialise parking slots.\n");
            system_destroy(sys);
            return 1;
        }
        printf("No save file found. Fresh system initialised with "
               "%d slots.\n", CONFIG_SLOT_CAPACITY);
    } else if (st != STATUS_OK) {
        /* File exists but could not be loaded: report the error. Do not
         * overwrite the existing file. */
        fprintf(stderr, "Save file '%s' is invalid or corrupted: %s\n",
                CONFIG_DATA_PATH, status_text(st));
        fprintf(stderr, "Please restore or remove the file and restart.\n");
        system_destroy(sys);
        return 1;
    } else {
        printf("Saved state loaded from %s\n", CONFIG_DATA_PATH);
    }

    int running = 1;
    while (running) {
        print_menu();
        int choice = 0;
        if (!read_integer(&choice, "Enter choice: ")) {
            printf("\nEOF received. Saving and exiting...\n");
            running = 0;
            continue;
        }

        switch (choice) {
            case MENU_REGISTER:
                do_register_vehicle(sys);
                break;
            case MENU_ENTRY:
                do_vehicle_entry(sys);
                break;
            case MENU_EXIT:
                do_vehicle_exit(sys);
                break;
            case MENU_SEARCH:
                do_search_vehicle(sys);
                break;
            case MENU_STATUS:
                do_parking_status(sys);
                break;
            case MENU_STATS:
                do_parking_statistics(sys);
                break;
            case MENU_SAVE:
                do_save_data(sys);
                break;
            case MENU_SAVE_EXIT: {
                Status s2 = storage_save(sys, CONFIG_DATA_PATH);
                if (s2 != STATUS_OK) {
                    printf("Save failed: %s\n", status_text(s2));
                } else {
                    printf("State saved to %s\n", CONFIG_DATA_PATH);
                }
                running = 0;
                break;
            }
            default:
                printf("  Invalid choice. Try again.\n");
                break;
        }
    }

    system_destroy(sys);
    printf("Goodbye.\n");
    return 0;
}
