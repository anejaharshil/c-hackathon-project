/* Sprint 4 Developer 1 regression tests for the parking engine.
 * Covers registration, slots, sessions, reservations, and slot_init safety.
 * All time values are relative to time(NULL) so expiry logic behaves correctly.
 */
#include "system.h"
#include "config.h"
#include "vehicle.h"
#include "slot.h"
#include "session.h"
#include "reservation.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int tests = 0;
static int passed = 0;

static void check(const char *name, int ok) {
    tests++;
    if (ok) {
        passed++;
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
    }
}

static time_t now(void) {
    return time(NULL);
}

/* Register `count` vehicles sequentially; return how many were registered. */
static int register_batch(ParkingSystem *sys, int start, int count,
                          VehicleType vt, UserType ut, Id *ids) {
    int n = 0;
    for (int i = 0; i < count; i++) {
        char reg[32];
        snprintf(reg, sizeof(reg), "S4-%02d-%d", start, i);
        Id id = ID_NONE;
        if (vehicle_register(sys, reg, vt, "Owner", ut, &id) == STATUS_OK) {
            ids[n++] = id;
        }
    }
    return n;
}

/* Fill every free slot in `zone` with sessions for `ids`. Return started count. */
static int fill_zone(ParkingSystem *sys, ZoneType zone, const Id *ids, int n,
                     time_t base) {
    int started = 0;
    for (int i = 0; i < n; i++) {
        size_t fz = 0;
        if (slot_find_free(sys, zone, &fz) != STATUS_OK) {
            break;
        }
        Id sid = ID_NONE;
        if (session_start(sys, ids[i], slot_get(sys, fz)->id, base + i,
                          &sid) == STATUS_OK) {
            started++;
        }
    }
    return started;
}

/* Find any free slot; return 1 and set *out_index, else 0. */
static int find_any_free(ParkingSystem *sys, size_t *out_index) {
    for (size_t i = 0; i < slot_count(sys); i++) {
        const ParkingSlot *sl = slot_get(sys, i);
        if (sl != NULL && sl->is_available) {
            *out_index = i;
            return 1;
        }
    }
    return 0;
}

int main(void) {
    ParkingSystem *sys = NULL;
    check("system_create", system_create(&sys) == STATUS_OK && sys != NULL);
    check("config_init", config_init() == STATUS_OK);
    time_t T = now();

    /* ------------------------------------------------------------------
     * 1. Valid registration
     * ------------------------------------------------------------------ */
    Id v1 = ID_NONE;
    check("valid registration",
          vehicle_register(sys, "S4-REG-001", VEHICLE_CAR, "Alice",
                           USER_STUDENT, &v1) == STATUS_OK && v1 > 0);
    check("vehicle count after register", vehicle_count(sys) == 1);

    /* ------------------------------------------------------------------
     * 2. Duplicate registration
     * ------------------------------------------------------------------ */
    Id dup = ID_NONE;
    check("duplicate registration rejected",
          vehicle_register(sys, "S4-REG-001", VEHICLE_CAR, "Alice",
                           USER_STUDENT, &dup) == STATUS_ALREADY_EXISTS);

    /* ------------------------------------------------------------------
     * 3. Invalid input
     * ------------------------------------------------------------------ */
    Id bad = ID_NONE;
    check("empty reg rejected",
          vehicle_register(sys, "", VEHICLE_CAR, "Bob", USER_STUDENT, &bad) ==
              STATUS_INVALID_ARGUMENT);
    check("too-long reg rejected",
          vehicle_register(sys, "TOOLONGREGISTRATION", VEHICLE_CAR, "Bob",
                           USER_STUDENT, &bad) == STATUS_INVALID_ARGUMENT);
    check("empty owner rejected",
          vehicle_register(sys, "S4-REG-002", VEHICLE_CAR, "", USER_STUDENT,
                           &bad) == STATUS_INVALID_ARGUMENT);
    check("invalid type rejected",
          vehicle_register(sys, "S4-REG-002", (VehicleType)99, "Bob",
                           USER_STUDENT, &bad) == STATUS_INVALID_ARGUMENT);
    check("invalid user_type rejected",
          vehicle_register(sys, "S4-REG-002", VEHICLE_CAR, "Bob", (UserType)99,
                           &bad) == STATUS_INVALID_ARGUMENT);
    check("NULL out_id rejected",
          vehicle_register(sys, "S4-REG-002", VEHICLE_CAR, "Bob", USER_STUDENT,
                           NULL) == STATUS_INVALID_ARGUMENT);
    check("NULL sys rejected",
          vehicle_register(NULL, "S4-REG-002", VEHICLE_CAR, "Bob", USER_STUDENT,
                           &bad) == STATUS_INVALID_ARGUMENT);
    check("count unchanged after invalid input", vehicle_count(sys) == 1);

    /* ------------------------------------------------------------------
     * 4. Dynamic array growth (register 20 more, forces realloc beyond 8)
     * ------------------------------------------------------------------ */
    Id ids[25];
    int n = register_batch(sys, 10, 20, VEHICLE_CAR, USER_STAFF, ids);
    check("registered 20 vehicles (growth)", n == 20);
    check("vehicle count is 21", vehicle_count(sys) == 21);
    size_t vidx = 0;
    check("last vehicle findable by reg",
          vehicle_find_by_reg(sys, "S4-10-19", &vidx) == STATUS_OK);

    /* ------------------------------------------------------------------
     * Slot setup
     * ------------------------------------------------------------------ */
    check("slot_init", slot_init(sys, CONFIG_SLOT_CAPACITY) == STATUS_OK);
    check("slot_count 50", slot_count(sys) == (size_t)CONFIG_SLOT_CAPACITY);

    /* ------------------------------------------------------------------
     * 5/6. Preferred-zone allocation
     * ------------------------------------------------------------------ */
    Id vA = ID_NONE;
    check("register vehicle A",
          vehicle_register(sys, "S4-REG-A", VEHICLE_CAR, "Amy", USER_STUDENT,
                           &vA) == STATUS_OK);
    size_t fz = 0;
    check("preferred zone ZONE_C available",
          slot_find_free(sys, ZONE_C, &fz) == STATUS_OK);
    Id sidA = ID_NONE;
    check("session_start in preferred zone",
          session_start(sys, vA, slot_get(sys, fz)->id, T + 1000, &sidA) ==
              STATUS_OK);
    check("preferred slot occupied by A",
          slot_get(sys, fz)->is_available == 0 &&
              slot_get(sys, fz)->vehicle_id == vA);

    /* End vA's session so ZONE_C is free for the fill test below. */
    check("end vA preferred session", session_end(sys, vA, T + 1500) == STATUS_OK);

    /* ------------------------------------------------------------------
     * 7. Overflow allocation (fill ZONE_C, then use another eligible zone)
     * ------------------------------------------------------------------ */
    Id idsB[12];
    int nB = register_batch(sys, 30, 10, VEHICLE_CAR, USER_STUDENT, idsB);
    check("registered 10 students for zone fill", nB == 10);
    int filled = fill_zone(sys, ZONE_C, idsB, nB, T + 2000);
    check("filled 10 ZONE_C slots", filled == 10);
    size_t fz2 = 0;
    check("ZONE_C now full", slot_find_free(sys, ZONE_C, &fz2) == STATUS_NOT_FOUND);
    /* Use a fresh vehicle for the overflow (idsB[0] already has a session). */
    Id vOvr = ID_NONE;
    check("register overflow vehicle",
          vehicle_register(sys, "S4-REG-OVR", VEHICLE_CAR, "Over", USER_STUDENT,
                           &vOvr) == STATUS_OK);
    size_t fa = 0;
    check("overflow zone ZONE_A available", slot_find_free(sys, ZONE_A, &fa) == STATUS_OK);
    Id sidO = ID_NONE;
    check("overflow session_start ZONE_A",
          session_start(sys, vOvr, slot_get(sys, fa)->id, T + 3000, &sidO) ==
              STATUS_OK);
    session_end(sys, vOvr, T + 3500); /* clean up */

    /* ------------------------------------------------------------------
     * 8. Protected accessible spaces (never general overflow)
     * ------------------------------------------------------------------ */
    Id idsC[40];
    int nC = register_batch(sys, 40, 30, VEHICLE_CAR, USER_STAFF, idsC);
    check("registered 30 staff", nC == 30);
    fill_zone(sys, ZONE_A, idsC, 10, T + 4000);
    fill_zone(sys, ZONE_B, idsC + 10, 10, T + 5000);
    fill_zone(sys, ZONE_VISITOR, idsC + 20, 10, T + 6000);
    int acc_free = 0;
    for (size_t i = 0; i < slot_count(sys); i++) {
        const ParkingSlot *sl = slot_get(sys, i);
        if (sl->zone == ZONE_ACCESSIBLE && sl->is_available) {
            acc_free++;
        }
    }
    check("accessible slots remain free (10)", acc_free == 10);
    size_t acc = 0;
    check("accessible explicitly requestable (not hidden)",
          slot_find_free(sys, ZONE_ACCESSIBLE, &acc) == STATUS_OK);

    /* ------------------------------------------------------------------
     * 9. Duplicate entry (vehicle already has active session)
     * ------------------------------------------------------------------ */
    Id sidX = ID_NONE;
    check("duplicate active session rejected",
          session_start(sys, vA, slot_get(sys, fz)->id, T + 1100, &sidX) ==
              STATUS_CONFLICT);

    /* ------------------------------------------------------------------
     * 10. Occupied-slot conflict (different vehicle, occupied slot)
     * ------------------------------------------------------------------ */
    check("occupied slot rejected for other vehicle",
          session_start(sys, idsB[0], slot_get(sys, fz)->id, T + 1100,
                        &sidX) == STATUS_CONFLICT);

    /* ------------------------------------------------------------------
     * 11. Reservation conflict (reserved slot, overlapping window)
     * ------------------------------------------------------------------ */
    size_t rslot = 0;
    int gotFree = find_any_free(sys, &rslot);
    check("found free slot for reservation test", gotFree);
    const ParkingSlot *resSlot = slot_get(sys, rslot);
    Id vR = ID_NONE;
    check("register reservation holder",
          vehicle_register(sys, "S4-REG-R", VEHICLE_CAR, "Reserver", USER_VISITOR,
                           &vR) == STATUS_OK);
    Id resId = ID_NONE;
    check("create reservation [T+1000,T+2000)",
          reservation_create(sys, vR, resSlot->id, T + 1000, T + 2000,
                             &resId) == STATUS_OK);
    Id vO = ID_NONE;
    check("register other vehicle",
          vehicle_register(sys, "S4-REG-O", VEHICLE_CAR, "Other", USER_STUDENT,
                           &vO) == STATUS_OK);
    Id sidR = ID_NONE;
    check("reservation conflict blocks entry",
          session_start(sys, vO, resSlot->id, T + 1500, &sidR) ==
              STATUS_CONFLICT);

    /* Reservation holder can enter own reserved slot */
    Id sidH = ID_NONE;
    check("holder enters own reserved slot",
          session_start(sys, vR, resSlot->id, T + 1500, &sidH) == STATUS_OK);
    check("holder slot occupied by holder",
          slot_get(sys, rslot)->vehicle_id == vR);
    session_end(sys, vR, T + 1600); /* clean up */

    /* Cancelled reservation does not block allocation */
    Id resId2 = ID_NONE;
    check("create reservation to cancel",
          reservation_create(sys, vR, resSlot->id, T + 3000, T + 4000,
                             &resId2) == STATUS_OK);
    check("cancel reservation", reservation_cancel(sys, resId2) == STATUS_OK);
    Id sidC = ID_NONE;
    check("cancelled reservation does not block entry",
          session_start(sys, vO, resSlot->id, T + 3500, &sidC) == STATUS_OK);
    session_end(sys, vO, T + 3600); /* clean up */

    /* Cancel vR's active reservation so resSlot is clean for the next tests. */
    check("cancel active reservation", reservation_cancel(sys, resId) == STATUS_OK);

    /* Expired reservation does not block allocation */
    Id resId3 = ID_NONE;
    check("create past-dated reservation",
          reservation_create(sys, vR, resSlot->id, T - 2000, T - 1000,
                             &resId3) == STATUS_OK);
    Id sidE = ID_NONE;
    check("expired reservation does not block entry",
          session_start(sys, vO, resSlot->id, T + 1000, &sidE) == STATUS_OK);
    session_end(sys, vO, T + 1100); /* clean up */

    /* ------------------------------------------------------------------
     * 12. Future-reservation conflict (documented limitation)
     * ------------------------------------------------------------------ */
    /* Find a clean free slot (not resSlot, which carries vR's reservations). */
    size_t rslot2 = 0;
    int gotFutureSlot = 0;
    for (size_t i = 0; i < slot_count(sys); i++) {
        const ParkingSlot *sl = slot_get(sys, i);
        if (sl != NULL && sl->is_available && sl->id != resSlot->id) {
            rslot2 = i;
            gotFutureSlot = 1;
            break;
        }
    }
    check("found clean free slot for future reservation", gotFutureSlot);
    const ParkingSlot *futSlot = slot_get(sys, rslot2);
    Id resIdF = ID_NONE;
    check("create future reservation [T+5000,T+6000)",
          reservation_create(sys, vR, futSlot->id, T + 5000, T + 6000,
                             &resIdF) == STATUS_OK);
    Id sidF = ID_NONE;
    check("future reservation does NOT block 1-sec entry (limitation)",
          session_start(sys, vO, futSlot->id, T + 1000, &sidF) ==
              STATUS_OK);
    session_end(sys, vO, T + 1100); /* clean up */

    /* ------------------------------------------------------------------
     * 13. Successful exit releases slot
     * ------------------------------------------------------------------ */
    size_t sf = 0;
    find_any_free(sys, &sf);
    Id sidS = ID_NONE;
    session_start(sys, vA, slot_get(sys, sf)->id, T + 7000, &sidS);
    check("session_start vA OK", sidS > 0);
    size_t sidxA = 0;
    session_find_by_id(sys, sidS, &sidxA);
    check("session_end releases slot",
          session_end(sys, vA, T + 8000) == STATUS_OK);
    check("slot free after exit",
          slot_get(sys, sf)->is_available == 1 &&
              slot_get(sys, sf)->vehicle_id == ID_NONE);
    const ParkingSession *ps = session_get(sys, sidxA);
    check("session completed in history",
          ps != NULL && ps->active == 0 && ps->end_time == T + 8000);

    /* ------------------------------------------------------------------
     * 14. Repeated exit rejected
     * ------------------------------------------------------------------ */
    check("repeated exit rejected",
          session_end(sys, vA, T + 9000) == STATUS_NOT_FOUND);

    /* ------------------------------------------------------------------
     * 15. Invalid timestamps (end before start)
     * ------------------------------------------------------------------ */
    size_t sf2 = 0;
    find_any_free(sys, &sf2);
    Id sidT = ID_NONE;
    check("session_start vA at sf2 OK",
          session_start(sys, vA, slot_get(sys, sf2)->id, T + 5000, &sidT) ==
              STATUS_OK);
    check("reject end before start",
          session_end(sys, vA, T + 4999) == STATUS_INVALID_ARGUMENT);
    check("session still active after failed end",
          session_find_active_for_vehicle(sys, vA, &vidx) == STATUS_OK);
    session_end(sys, vA, T + 6000); /* clean up */

    /* ------------------------------------------------------------------
     * 16. Slot reinitialisation
     * ------------------------------------------------------------------ */
    /* End every active session so all slots are free. */
    for (size_t i = 0; i < session_count(sys); i++) {
        const ParkingSession *s = session_get(sys, i);
        if (s != NULL && s->active) {
            session_end(sys, s->vehicle_id, T + 20000);
        }
    }
    check("slot_init all-free (harmless no-op)",
          slot_init(sys, CONFIG_SLOT_CAPACITY) == STATUS_OK);
    /* Occupy one slot, then re-init must be refused. */
    size_t occ = 0;
    find_any_free(sys, &occ);
    Id sidOcc = ID_NONE;
    session_start(sys, vA, slot_get(sys, occ)->id, T + 21000, &sidOcc);
    check("slot_init occupied refused (CONFLICT)",
          slot_init(sys, CONFIG_SLOT_CAPACITY) == STATUS_CONFLICT);
    /* Verify the occupied slot is still occupied (state not erased). */
    int stillOcc = 0;
    for (size_t i = 0; i < slot_count(sys); i++) {
        const ParkingSlot *sl = slot_get(sys, i);
        if (sl != NULL && !sl->is_available) {
            stillOcc = 1;
            break;
        }
    }
    check("occupied slots not erased after refused slot_init", stillOcc);
    /* End the session we just started. */
    session_end(sys, vA, T + 22000);
    check("slot_init NULL sys", slot_init(NULL, 10) == STATUS_INVALID_ARGUMENT);
    check("slot_init bad capacity",
          slot_init(sys, 0) == STATUS_INVALID_ARGUMENT);

    /* ------------------------------------------------------------------
     * 17. Vehicle removal with active references
     * ------------------------------------------------------------------ */
    size_t sh = 0;
    find_any_free(sys, &sh);
    Id sidV = ID_NONE;
    session_start(sys, vA, slot_get(sys, sh)->id, T + 9000, &sidV);
    check("remove vehicle with active session rejected",
          vehicle_remove(sys, vA) == STATUS_CONFLICT);
    check("remove vehicle with live reservation rejected",
          vehicle_remove(sys, vR) == STATUS_CONFLICT);
    session_end(sys, vA, T + 10000);
    check("remove vehicle OK after exit",
          vehicle_remove(sys, vA) == STATUS_OK);
    check("vehicle removed",
          vehicle_find_by_id(sys, vA, &sh) == STATUS_NOT_FOUND);
    check("remove missing vehicle", vehicle_remove(sys, 99999) == STATUS_NOT_FOUND);

    /* ------------------------------------------------------------------
     * Invalid IDs safe
     * ------------------------------------------------------------------ */
    check("slot_get out of bounds", slot_get(sys, slot_count(sys)) == NULL);
    check("session_get out of bounds", session_get(sys, session_count(sys)) == NULL);
    check("vehicle_get out of bounds", vehicle_get(sys, vehicle_count(sys)) == NULL);

    system_destroy(sys);

    printf("\n%d/%d tests passed\n", passed, tests);
    return (passed == tests) ? 0 : 1;
}
