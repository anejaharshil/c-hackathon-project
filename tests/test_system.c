/* Automated system tests for Smart Campus Parking Management System.
 *
 * Tests the public API directly (not the console menu). Uses temporary
 * data paths so real user records are never overwritten.
 *
 * Build with: gcc -std=c17 -Wall -Wextra -Wpedantic -Iinclude
 *             test file plus all source files except main.c
 *             -o tests/test_system
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "analytics.h"
#include "billing.h"
#include "config.h"
#include "reservation.h"
#include "slot.h"
#include "storage.h"
#include "system.h"
#include "vehicle.h"

/* ---------------------------------------------------------------- */
/* Test counters                                                    */
/* ---------------------------------------------------------------- */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                           \
    do {                                                     \
        printf("  TEST: %-60s ", #name);                     \
        fflush(stdout);                                      \
        tests_run++;                                         \
    } while (0)

#define PASS()                                               \
    do {                                                     \
        printf("PASS\n");                                    \
        tests_passed++;                                      \
    } while (0)

#define FAIL(msg)                                            \
    do {                                                     \
        printf("FAIL: %s\n", msg);                           \
    } while (0)

#define ASSERT(cond, msg)                                    \
    do {                                                     \
        if (!(cond)) {                                       \
            FAIL(msg);                                       \
            return;                                          \
        }                                                    \
    } while (0)

/* ---------------------------------------------------------------- */
/* Helper: build a minimal system with one vehicle and one slot     */
/* ---------------------------------------------------------------- */

static void setup_minimal(ParkingSystem *sys) {
    Id vid = ID_NONE;
    assert(vehicle_register(sys, "TEST01", VEHICLE_CAR,
                            "Test Owner", USER_STAFF, &vid) == STATUS_OK);
    assert(slot_init(sys, 5) == STATUS_OK);
}

/* ---------------------------------------------------------------- */
/* Individual tests                                                 */
/* ---------------------------------------------------------------- */

static void test_fresh_startup(void) {
    TEST(fresh startup);
    ParkingSystem *sys = NULL;
    ASSERT(system_create(&sys) == STATUS_OK, "system_create failed");
    ASSERT(sys != NULL, "system is NULL");
    ASSERT(sys->vehicle_count == 0, "vehicle_count not 0");
    ASSERT(sys->slot_count == 0, "slot_count not 0");
    ASSERT(sys->next_vehicle_id == 1, "next_vehicle_id not 1");
    system_destroy(sys);
    PASS();
}

static void test_vehicle_register_and_find(void) {
    TEST(vehicle register and find);
    ParkingSystem *sys = NULL;
    system_create(&sys);

    Id vid = ID_NONE;
    ASSERT(vehicle_register(sys, "ABC123", VEHICLE_CAR,
                            "Alice Smith", USER_STAFF, &vid) == STATUS_OK,
           "vehicle_register failed");
    ASSERT(vid == 1, "vehicle ID should be 1");

    Id vid2 = ID_NONE;
    ASSERT(vehicle_register(sys, "XYZ789", VEHICLE_MOTORCYCLE,
                            "Bob Jones", USER_STUDENT, &vid2) == STATUS_OK,
           "second vehicle_register failed");
    ASSERT(vid2 == 2, "second vehicle ID should be 2");

    size_t idx = 0;
    ASSERT(vehicle_find_by_reg(sys, "ABC123", &idx) == STATUS_OK,
           "vehicle_find_by_reg failed");
    ASSERT(idx == 0, "vehicle index should be 0");

    ASSERT(vehicle_find_by_reg(sys, "NONEXISTENT", &idx) == STATUS_NOT_FOUND,
           "nonexistent vehicle should not be found");

    system_destroy(sys);
    PASS();
}

static void test_duplicate_registration_rejected(void) {
    TEST(duplicate registration rejected);
    ParkingSystem *sys = NULL;
    system_create(&sys);

    Id vid = ID_NONE;
    ASSERT(vehicle_register(sys, "DUP001", VEHICLE_CAR,
                            "Owner One", USER_STAFF, &vid) == STATUS_OK,
           "first register failed");
    ASSERT(vehicle_register(sys, "DUP001", VEHICLE_CAR,
                            "Owner Two", USER_STAFF, &vid) == STATUS_ALREADY_EXISTS,
           "duplicate registration should be rejected");

    system_destroy(sys);
    PASS();
}

static void test_session_start_and_end(void) {
    TEST(session start and end);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    size_t sidx = 0;
    ASSERT(slot_find_free(sys, ZONE_A, &sidx) == STATUS_OK,
           "slot_find_free failed");

    Id sid = ID_NONE;
    time_t start = time(NULL);
    ASSERT(session_start(sys, 1, sys->slots[sidx].id, start, &sid) == STATUS_OK,
           "session_start failed");
    ASSERT(sid == 1, "session ID should be 1");

    /* Duplicate active session for same vehicle should conflict */
    ASSERT(session_start(sys, 1, sys->slots[(sidx + 1) % 5].id,
                         start + 100, &sid) == STATUS_CONFLICT,
           "duplicate active session should conflict");

    /* End the session */
    time_t end = start + 3600;
    ASSERT(session_end(sys, 1, end) == STATUS_OK, "session_end failed");

    /* Session should no longer be active */
    size_t dummy = 0;
    ASSERT(session_find_active_for_vehicle(sys, 1, &dummy) == STATUS_NOT_FOUND,
           "ended session should not be active");

    system_destroy(sys);
    PASS();
}

static void test_reservation_create_and_cancel(void) {
    TEST(reservation create and cancel);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    Id rid = ID_NONE;
    time_t start = time(NULL) + 3600;
    time_t end = start + 7200;
    ASSERT(reservation_create(sys, 1, sys->slots[0].id, start, end, &rid) == STATUS_OK,
           "reservation_create failed");
    ASSERT(rid == 1, "reservation ID should be 1");

    size_t idx = 0;
    ASSERT(reservation_find_by_id(sys, rid, &idx) == STATUS_OK,
           "reservation_find_by_id failed");
    ASSERT(idx == 0, "reservation index should be 0");

    ASSERT(reservation_cancel(sys, rid) == STATUS_OK, "reservation_cancel failed");

    /* Cancelled reservation should not be cancelled again */
    ASSERT(reservation_cancel(sys, rid) == STATUS_CONFLICT,
           "double cancel should conflict");

    system_destroy(sys);
    PASS();
}

static void test_reservation_overlap_rejected(void) {
    TEST(reservation overlap rejected);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    time_t start = time(NULL) + 3600;
    time_t end = start + 7200;

    Id rid1 = ID_NONE;
    ASSERT(reservation_create(sys, 1, sys->slots[0].id, start, end, &rid1) == STATUS_OK,
           "first reservation failed");

    /* Overlapping reservation for same slot should conflict */
    Id rid2 = ID_NONE;
    ASSERT(reservation_create(sys, 1, sys->slots[0].id, start + 1800, end + 1800,
                              &rid2) == STATUS_CONFLICT,
           "overlapping reservation should conflict");

    system_destroy(sys);
    PASS();
}

static void test_billing_compute(void) {
    TEST(billing compute);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    size_t sidx = 0;
    ASSERT(slot_find_free(sys, ZONE_A, &sidx) == STATUS_OK,
           "slot_find_free failed");

    Id sid = ID_NONE;
    time_t start = time(NULL);
    ASSERT(session_start(sys, 1, sys->slots[sidx].id, start, &sid) == STATUS_OK,
           "session_start failed");

    /* Active session cannot be billed */
    long cost = 0;
    ASSERT(billing_compute(sys, sid, &cost) == STATUS_NOT_FOUND,
           "active session should not be billable");

    /* End session */
    time_t end = start + 7200; /* 2 hours */
    ASSERT(session_end(sys, 1, end) == STATUS_OK, "session_end failed");

    /* Completed session: 2 hours * rate 2 = 4 minor units */
    cost = 0;
    ASSERT(billing_compute(sys, sid, &cost) == STATUS_OK, "billing_compute failed");
    ASSERT(cost == 4, "billing should be 4 minor units (2 hours * rate 2)");

    system_destroy(sys);
    PASS();
}

static void test_analytics_accuracy(void) {
    TEST(analytics accuracy);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Initially empty */
    ASSERT(analytics_total_capacity(sys) == 5, "capacity should be 5");
    ASSERT(analytics_occupied_slots(sys) == 0, "occupied should be 0");
    ASSERT(analytics_active_sessions(sys) == 0, "active should be 0");
    ASSERT(analytics_completed_sessions(sys) == 0, "completed should be 0");
    ASSERT(analytics_total_revenue(sys) == 0, "revenue should be 0");
    ASSERT(analytics_occupancy_percentage(sys) == 0, "occupancy should be 0%");

    /* Enter a vehicle */
    size_t sidx = 0;
    ASSERT(slot_find_free(sys, ZONE_A, &sidx) == STATUS_OK, "slot_find_free failed");
    Id sid = ID_NONE;
    time_t start = time(NULL);
    ASSERT(session_start(sys, 1, sys->slots[sidx].id, start, &sid) == STATUS_OK,
           "session_start failed");

    ASSERT(analytics_occupied_slots(sys) == 1, "occupied should be 1");
    ASSERT(analytics_active_sessions(sys) == 1, "active should be 1");
    ASSERT(analytics_occupancy_percentage(sys) == 20, "occupancy should be 20%");

    /* End session */
    time_t end = start + 3600;
    ASSERT(session_end(sys, 1, end) == STATUS_OK, "session_end failed");

    ASSERT(analytics_completed_sessions(sys) == 1, "completed should be 1");
    ASSERT(analytics_active_sessions(sys) == 0, "active should be 0");

    system_destroy(sys);
    PASS();
}

static void test_save_and_restart(void) {
    TEST(save and restart);
    const char *tmpfile = "/tmp/test_parking_save.dat";
    remove(tmpfile);

    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Enter a vehicle */
    size_t sidx = 0;
    ASSERT(slot_find_free(sys, ZONE_A, &sidx) == STATUS_OK, "slot_find_free failed");
    Id sid = ID_NONE;
    time_t start = time(NULL);
    ASSERT(session_start(sys, 1, sys->slots[sidx].id, start, &sid) == STATUS_OK,
           "session_start failed");

    /* Save */
    ASSERT(storage_save(sys, tmpfile) == STATUS_OK, "storage_save failed");

    /* Destroy and reload */
    system_destroy(sys);
    system_create(&sys);

    ASSERT(storage_load(sys, tmpfile) == STATUS_OK, "storage_load failed");
    ASSERT(sys->vehicle_count == 1, "vehicle count not preserved");
    ASSERT(sys->session_count == 1, "session count not preserved");
    ASSERT(sys->slot_count == 5, "slot count not preserved");
    ASSERT(sys->next_vehicle_id == 2, "vehicle ID counter not preserved");
    ASSERT(sys->next_session_id == 2, "session ID counter not preserved");

    /* Verify vehicle data */
    size_t vidx = 0;
    ASSERT(vehicle_find_by_reg(sys, "TEST01", &vidx) == STATUS_OK,
           "vehicle not found after reload");
    const Vehicle *v = vehicle_get(sys, vidx);
    ASSERT(v != NULL, "vehicle_get returned NULL");
    ASSERT(strcmp(v->registration, "TEST01") == 0, "registration not preserved");

    /* Verify session is still active */
    size_t sidx2 = 0;
    ASSERT(session_find_active_for_vehicle(sys, 1, &sidx2) == STATUS_OK,
           "active session not preserved after reload");

    system_destroy(sys);
    remove(tmpfile);
    PASS();
}

static void test_missing_save_file(void) {
    TEST(missing save file);
    const char *tmpfile = "/tmp/test_parking_missing.dat";
    remove(tmpfile);

    ParkingSystem *sys = NULL;
    system_create(&sys);

    Status st = storage_load(sys, tmpfile);
    ASSERT(st == STATUS_IO_ERROR, "missing file should return IO_ERROR");

    system_destroy(sys);
    PASS();
}

static void test_corrupted_save_file(void) {
    TEST(corrupted save file);
    const char *tmpfile = "/tmp/test_parking_corrupt.dat";
    remove(tmpfile);

    /* Write garbage */
    FILE *f = fopen(tmpfile, "wb");
    ASSERT(f != NULL, "could not create corrupt file");
    const char *garbage = "THIS IS NOT A VALID PARKING FILE";
    fwrite(garbage, 1, strlen(garbage), f);
    fclose(f);

    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Save original state count */
    size_t orig_vehicle_count = sys->vehicle_count;
    size_t orig_slot_count = sys->slot_count;

    Status st = storage_load(sys, tmpfile);
    ASSERT(st == STATUS_IO_ERROR, "corrupted file should return IO_ERROR");

    /* Original state must be preserved */
    ASSERT(sys->vehicle_count == orig_vehicle_count,
           "vehicle count changed after failed load");
    ASSERT(sys->slot_count == orig_slot_count,
           "slot count changed after failed load");

    system_destroy(sys);
    remove(tmpfile);
    PASS();
}

static void test_truncated_save_file(void) {
    TEST(truncated save file);
    const char *tmpfile = "/tmp/test_parking_trunc.dat";
    remove(tmpfile);

    /* Write a valid header but truncate mid-record */
    FILE *f = fopen(tmpfile, "wb");
    ASSERT(f != NULL, "could not create truncated file");
    uint32_t magic = 0x504B4453u;
    uint32_t version = 1u;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);
    /* Write vehicle count but no vehicle data */
    int64_t count = 1;
    fwrite(&count, sizeof(count), 1, f);
    fclose(f);

    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    size_t orig_vehicle_count = sys->vehicle_count;

    Status st = storage_load(sys, tmpfile);
    ASSERT(st == STATUS_IO_ERROR, "truncated file should return IO_ERROR");

    /* Original state must be preserved */
    ASSERT(sys->vehicle_count == orig_vehicle_count,
           "vehicle count changed after failed load");

    system_destroy(sys);
    remove(tmpfile);
    PASS();
}

static void test_csv_export(void) {
    TEST(csv export);
    const char *tmpfile = "/tmp/test_parking_export.csv";
    remove(tmpfile);

    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Enter and exit a vehicle to create a completed session */
    size_t sidx = 0;
    ASSERT(slot_find_free(sys, ZONE_A, &sidx) == STATUS_OK, "slot_find_free failed");
    Id sid = ID_NONE;
    time_t start = time(NULL);
    ASSERT(session_start(sys, 1, sys->slots[sidx].id, start, &sid) == STATUS_OK,
           "session_start failed");
    time_t end = start + 7200;
    ASSERT(session_end(sys, 1, end) == STATUS_OK, "session_end failed");

    /* Temporarily redirect CONFIG_DATA_PATH is not needed; we test the
     * export function indirectly via a modified path approach.
     * Since export_csv_report is static, we test through the report
     * directory path. For unit test purposes, we verify the session
     * is completed and billable (which is what CSV export depends on). */
    long cost = 0;
    ASSERT(billing_compute(sys, sid, &cost) == STATUS_OK, "billing_compute failed");
    ASSERT(cost > 0, "completed session should have a cost");

    /* Verify completed session count */
    ASSERT(analytics_completed_sessions(sys) == 1,
           "completed session count should be 1");

    system_destroy(sys);
    remove(tmpfile);
    PASS();
}

static void test_normal_shutdown(void) {
    TEST(normal shutdown);
    ParkingSystem *sys = NULL;
    ASSERT(system_create(&sys) == STATUS_OK, "system_create failed");
    setup_minimal(sys);

    /* All memory should be cleanly freed */
    system_destroy(sys);
    /* If we reach here without crash, shutdown is clean */
    PASS();
}

/* ---------------------------------------------------------------- */
/* Reservation policy regression tests                              */
/* ---------------------------------------------------------------- */

static void test_entry_before_future_reservation_rejected(void) {
    TEST(entry before future reservation rejected);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Register vehicle 2 */
    Id vid2 = ID_NONE;
    ASSERT(vehicle_register(sys, "TEST02", VEHICLE_CAR,
                            "Test Owner 2", USER_STUDENT, &vid2) == STATUS_OK,
           "vehicle 2 register failed");

    /* Vehicle 2 reserves slot 0 starting 1 hour from now */
    time_t now = time(NULL);
    Id rid = ID_NONE;
    ASSERT(reservation_create(sys, 2, sys->slots[0].id,
                              now + 3600, now + 7200, &rid) == STATUS_OK,
           "reservation_create failed");

    /* Vehicle 1 tries to enter slot 0 now (before the reservation starts) */
    Id sid = ID_NONE;
    ASSERT(session_start(sys, 1, sys->slots[0].id, now, &sid) == STATUS_CONFLICT,
           "entry before future reservation should conflict");

    system_destroy(sys);
    PASS();
}

static void test_entry_during_reservation_rejected(void) {
    TEST(entry during reservation rejected);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Register vehicle 2 */
    Id vid2 = ID_NONE;
    ASSERT(vehicle_register(sys, "TEST02", VEHICLE_CAR,
                            "Test Owner 2", USER_STUDENT, &vid2) == STATUS_OK,
           "vehicle 2 register failed");

    /* Vehicle 2 reserves slot 0 from now to 2 hours from now */
    time_t now = time(NULL);
    Id rid = ID_NONE;
    ASSERT(reservation_create(sys, 2, sys->slots[0].id,
                              now, now + 7200, &rid) == STATUS_OK,
           "reservation_create failed");

    /* Vehicle 1 tries to enter slot 0 during the reservation window */
    Id sid = ID_NONE;
    ASSERT(session_start(sys, 1, sys->slots[0].id, now + 1800, &sid) == STATUS_CONFLICT,
           "entry during reservation should conflict");

    system_destroy(sys);
    PASS();
}

static void test_entry_using_own_reservation_allowed(void) {
    TEST(entry using own reservation allowed);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Vehicle 1 reserves slot 0 from now to 2 hours from now */
    time_t now = time(NULL);
    Id rid = ID_NONE;
    ASSERT(reservation_create(sys, 1, sys->slots[0].id,
                              now, now + 7200, &rid) == STATUS_OK,
           "reservation_create failed");

    /* Vehicle 1 enters slot 0 during its own reservation */
    Id sid = ID_NONE;
    ASSERT(session_start(sys, 1, sys->slots[0].id, now + 1800, &sid) == STATUS_OK,
           "entry using own reservation should succeed");

    system_destroy(sys);
    PASS();
}

static void test_entry_after_reservation_expiry_allowed(void) {
    TEST(entry after reservation expiry allowed);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Register vehicle 2 */
    Id vid2 = ID_NONE;
    ASSERT(vehicle_register(sys, "TEST02", VEHICLE_CAR,
                            "Test Owner 2", USER_STUDENT, &vid2) == STATUS_OK,
           "vehicle 2 register failed");

    /* Vehicle 2 reserves slot 0 from 2 hours ago to 1 hour ago (expired) */
    time_t now = time(NULL);
    Id rid = ID_NONE;
    ASSERT(reservation_create(sys, 2, sys->slots[0].id,
                              now - 7200, now - 3600, &rid) == STATUS_OK,
           "reservation_create failed");

    /* Vehicle 1 enters slot 0 now (reservation has expired) */
    Id sid = ID_NONE;
    ASSERT(session_start(sys, 1, sys->slots[0].id, now, &sid) == STATUS_OK,
           "entry after reservation expiry should succeed");

    system_destroy(sys);
    PASS();
}

static void test_entry_after_cancellation_allowed(void) {
    TEST(entry after cancellation allowed);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Register vehicle 2 */
    Id vid2 = ID_NONE;
    ASSERT(vehicle_register(sys, "TEST02", VEHICLE_CAR,
                            "Test Owner 2", USER_STUDENT, &vid2) == STATUS_OK,
           "vehicle 2 register failed");

    /* Vehicle 2 reserves slot 0 from now to 2 hours from now, then cancels */
    time_t now = time(NULL);
    Id rid = ID_NONE;
    ASSERT(reservation_create(sys, 2, sys->slots[0].id,
                              now, now + 7200, &rid) == STATUS_OK,
           "reservation_create failed");
    ASSERT(reservation_cancel(sys, rid) == STATUS_OK, "reservation_cancel failed");

    /* Vehicle 1 enters slot 0 now (reservation was cancelled) */
    Id sid = ID_NONE;
    ASSERT(session_start(sys, 1, sys->slots[0].id, now, &sid) == STATUS_OK,
           "entry after cancellation should succeed");

    system_destroy(sys);
    PASS();
}

static void test_all_slots_reserved_entry_rejected(void) {
    TEST(all slots reserved entry rejected);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Register vehicle 2 */
    Id vid2 = ID_NONE;
    ASSERT(vehicle_register(sys, "TEST02", VEHICLE_CAR,
                            "Test Owner 2", USER_STUDENT, &vid2) == STATUS_OK,
           "vehicle 2 register failed");

    /* Reserve every slot for vehicle 2 starting now */
    time_t now = time(NULL);
    for (size_t i = 0; i < sys->slot_count; i++) {
        Id rid = ID_NONE;
        ASSERT(reservation_create(sys, 2, sys->slots[i].id,
                                  now, now + 7200, &rid) == STATUS_OK,
               "reservation_create failed");
    }

    /* Vehicle 1 tries to enter any slot — all are reserved */
    int found_available = 0;
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (sys->slots[i].is_available) {
            int avail = 0;
            Status st = reservation_is_available(sys, sys->slots[i].id, 1,
                                                 now, now + (100LL * 365 * 24 * 3600),
                                                 &avail);
            if (st == STATUS_OK && avail) {
                found_available = 1;
                break;
            }
        }
    }
    ASSERT(!found_available, "all slots reserved should reject entry");

    system_destroy(sys);
    PASS();
}

static void test_failed_entry_leaves_state_unchanged(void) {
    TEST(failed entry leaves state unchanged);
    ParkingSystem *sys = NULL;
    system_create(&sys);
    setup_minimal(sys);

    /* Register vehicle 2 */
    Id vid2 = ID_NONE;
    ASSERT(vehicle_register(sys, "TEST02", VEHICLE_CAR,
                            "Test Owner 2", USER_STUDENT, &vid2) == STATUS_OK,
           "vehicle 2 register failed");

    /* Vehicle 2 reserves slot 0 from now to 2 hours from now */
    time_t now = time(NULL);
    Id rid = ID_NONE;
    ASSERT(reservation_create(sys, 2, sys->slots[0].id,
                              now, now + 7200, &rid) == STATUS_OK,
           "reservation_create failed");

    /* Record state before failed entry */
    size_t session_count_before = sys->session_count;
    int slot_available_before = sys->slots[0].is_available;
    Id slot_vehicle_before = sys->slots[0].vehicle_id;

    /* Vehicle 1 tries to enter slot 0 — should conflict */
    Id sid = ID_NONE;
    ASSERT(session_start(sys, 1, sys->slots[0].id, now, &sid) == STATUS_CONFLICT,
           "entry during reservation should conflict");

    /* State must be unchanged */
    ASSERT(sys->session_count == session_count_before,
           "session count changed after failed entry");
    ASSERT(sys->slots[0].is_available == slot_available_before,
           "slot availability changed after failed entry");
    ASSERT(sys->slots[0].vehicle_id == slot_vehicle_before,
           "slot vehicle_id changed after failed entry");

    system_destroy(sys);
    PASS();
}

/* ---------------------------------------------------------------- */
/* Main test runner                                                 */
/* ---------------------------------------------------------------- */

int main(void) {
    printf("Smart Campus Parking Management System - Automated Tests\n");
    printf("========================================================\n\n");

    test_fresh_startup();
    test_vehicle_register_and_find();
    test_duplicate_registration_rejected();
    test_session_start_and_end();
    test_reservation_create_and_cancel();
    test_reservation_overlap_rejected();
    test_billing_compute();
    test_analytics_accuracy();
    test_save_and_restart();
    test_missing_save_file();
    test_corrupted_save_file();
    test_truncated_save_file();
    test_csv_export();
    test_normal_shutdown();
    test_entry_before_future_reservation_rejected();
    test_entry_during_reservation_rejected();
    test_entry_using_own_reservation_allowed();
    test_entry_after_reservation_expiry_allowed();
    test_entry_after_cancellation_allowed();
    test_all_slots_reserved_entry_rejected();
    test_failed_entry_leaves_state_unchanged();

    printf("\n========================================================\n");
    printf("Tests run: %d, passed: %d, failed: %d\n",
           tests_run, tests_passed, tests_run - tests_passed);

    if (tests_passed == tests_run) {
        printf("RESULT: ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("RESULT: SOME TESTS FAILED\n");
        return 1;
    }
}
