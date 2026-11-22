#include "parking.h"
#include "registration.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST_PATH "tests/parking_test.dat"

/* Header layout: magic (4), version (4), next_id (8), vehicle_count (8),
 * slot_count (8), session_count (8). The first vehicle record follows. */
#define VEHICLE_COUNT_OFFSET 16
#define FIRST_VEHICLE_CATEGORY_OFFSET 48
#define TRUNCATED_HEADER_SIZE 20

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(condition, name)                                   \
    do {                                                         \
        tests_run++;                                             \
        if (condition) {                                         \
            tests_passed++;                                      \
            printf("PASS: %s\n", name);                          \
        } else {                                                 \
            printf("FAIL: %s\n", name);                          \
        }                                                        \
    } while (0)

static void remove_test_file(void) {
    remove(TEST_PATH);
    remove(TEST_PATH ".tmp");
    remove(TEST_PATH ".bak");
    remove(TEST_PATH ".bak.1");
    remove(TEST_PATH ".bak.2");
    remove(TEST_PATH ".bak.3");
}

static ParkingSystem *make_system(void) {
    ParkingSystem *sys = parking_create(DEFAULT_SLOTS);
    if (sys == NULL) {
        fprintf(stderr, "FATAL: parking_create failed\n");
        exit(1);
    }
    return sys;
}

static Id add_vehicle(ParkingSystem *sys, const char *registration,
                      UserType user_type, const char *institution_id) {
    Id id = ID_NONE;
    ParkingStatus status = parking_register(
        sys, registration, "Test Owner", institution_id, user_type,
        VEHICLE_CAR, &id);
    if (status != PARKING_OK) {
        fprintf(stderr, "FATAL: could not register %s (%d)\n",
                registration, (int)status);
        exit(1);
    }
    return id;
}

static void test_registration(void) {
    ParkingSystem *sys = make_system();
    Id id = ID_NONE;

    CHECK(parking_register(sys, "UP16AA0001", "Alice", "590000001",
                           USER_STUDENT, VEHICLE_CAR, &id) == PARKING_OK,
          "student registration succeeds");
    CHECK(id != ID_NONE, "registration returns a stable ID");
    CHECK(parking_register(sys, "UP16AA0002", "Bob", "123456789",
                           USER_STAFF, VEHICLE_CAR, NULL) == PARKING_OK,
          "staff registration succeeds");
    CHECK(parking_register(sys, "UP16AA0003", "Carol", NULL,
                           USER_VISITOR, VEHICLE_CAR, NULL) == PARKING_OK,
          "visitor registration succeeds without an institutional ID");

    Vehicle *student = parking_find_vehicle(sys, "UP16AA0001");
    CHECK(student != NULL && student->user_type == USER_STUDENT &&
              strcmp(student->institution_id, "590000001") == 0,
          "student record stores category and ID");

    int zeroed = 1;
    for (size_t i = strlen(student->registration); i < MAX_REG_LEN; i++) {
        if (student->registration[i] != '\0') {
            zeroed = 0;
        }
    }
    for (size_t i = strlen(student->owner); i < MAX_OWNER_LEN; i++) {
        if (student->owner[i] != '\0') {
            zeroed = 0;
        }
    }
    for (size_t i = strlen(student->institution_id); i < MAX_ID_LEN; i++) {
        if (student->institution_id[i] != '\0') {
            zeroed = 0;
        }
    }
    CHECK(zeroed, "unused string bytes are zeroed");

    CHECK(parking_register(sys, "UP16AA0001", "Duplicate", "590000002",
                           USER_STUDENT, VEHICLE_CAR, NULL) == PARKING_DUPLICATE,
          "duplicate registration is rejected");
    CHECK(parking_register(sys, "UP16AA0004", "Duplicate", "590000001",
                           USER_STUDENT, VEHICLE_CAR, NULL) == PARKING_DUPLICATE,
          "duplicate student ID is rejected");
    CHECK(parking_register(sys, "UP16AA0005", "Different", "590000001",
                           USER_STAFF, VEHICLE_CAR, NULL) == PARKING_OK,
          "same institutional ID may exist in another category");
    CHECK(parking_register(sys, "", "Empty", "590000003",
                           USER_STUDENT, VEHICLE_CAR, NULL) == PARKING_INVALID_REGISTRATION,
          "empty registration is rejected");
    CHECK(parking_register(sys, "UP16AA0006", "Bad", "590000004",
                           (UserType)99, VEHICLE_CAR, NULL) == PARKING_INVALID,
          "invalid user category is rejected");
    CHECK(parking_register(sys, "UP16AA0007", "Bad", "590000005",
                           USER_STUDENT, (VehicleType)99, NULL) == PARKING_INVALID,
          "invalid vehicle type is rejected");
    CHECK(parking_register(sys, "up16aa0070", "Lower", "590000070",
                           USER_STUDENT, VEHICLE_CAR, NULL) == PARKING_OK,
          "lowercase registration is accepted");
    CHECK(parking_find_vehicle(sys, "up16aa0070") != NULL,
          "lowercase lookup finds an uppercase stored registration");
    CHECK(parking_register(sys, "UP16AA0070", "Duplicate", "590000071",
                           USER_STUDENT, VEHICLE_CAR, NULL) == PARKING_DUPLICATE,
          "normalized duplicate registration is rejected");
    CHECK(parking_register(sys, "UR78HJ7834", "Bad", "590000072",
                           USER_STUDENT, VEHICLE_CAR, NULL) ==
          PARKING_INVALID_REGISTRATION,
          "invalid state code is rejected at registration");
    CHECK(parking_register(sys, "UP99AB1234", "Bad", "590000073",
                           USER_STUDENT, VEHICLE_CAR, NULL) ==
          PARKING_INVALID_REGISTRATION,
          "invalid RTO district is rejected at registration");
    CHECK(parking_register(sys, "UP16AA0074", "Bad", "590100000",
                           USER_STUDENT, VEHICLE_CAR, NULL) ==
          PARKING_INVALID_ID,
          "SAP ID outside the allowed range is rejected");
    CHECK(parking_register(sys, "UP16AA0075", "Bad", "12345678",
                           USER_STAFF, VEHICLE_CAR, NULL) ==
          PARKING_INVALID_ID,
          "eight-digit teacher ID is rejected");

    parking_destroy(sys);
}

static void test_billing(void) {
    time_t base = 1700000000;

    CHECK(parking_compute_charge(USER_VISITOR, base, base) == 0,
          "zero-duration visit is free");
    CHECK(parking_compute_charge(USER_VISITOR, base, base + 1) == 20,
          "one second rounds up to one hour");
    CHECK(parking_compute_charge(USER_VISITOR, base, base + 1800) == 20,
          "thirty minutes costs Rs. 20");
    CHECK(parking_compute_charge(USER_VISITOR, base, base + 3600) == 20,
          "one hour costs Rs. 20");
    CHECK(parking_compute_charge(USER_VISITOR, base, base + 3601) == 40,
          "one hour one second rounds to two hours");
    CHECK(parking_compute_charge(USER_VISITOR, base, base + 5400) == 40,
          "ninety minutes costs Rs. 40");
    CHECK(parking_compute_charge(USER_VISITOR, base, base + 90000) == 500,
          "visitor billing has no 24-hour cap");
    CHECK(parking_compute_charge(USER_STUDENT, base, base + 99999) == 0,
          "student parking is always free");
    CHECK(parking_compute_charge(USER_STAFF, base, base + 99999) == 0,
          "staff parking is always free");
}

static void test_entry_and_exit(void) {
    ParkingSystem *sys = make_system();
    add_vehicle(sys, "UP16AA0001", USER_STUDENT, "590000001");
    add_vehicle(sys, "UP16AA0002", USER_STAFF, "123456789");
    add_vehicle(sys, "UP16AA0003", USER_VISITOR, NULL);
    time_t base = 1700000000;
    Id slot_id = ID_NONE;
    Receipt receipt;

    CHECK(parking_enter(sys, "UP16AA0001", base, &slot_id) == PARKING_OK &&
              slot_id != ID_NONE,
          "automatic slot allocation succeeds");
    CHECK(parking_occupied_slots(sys) == 1,
          "occupancy updates after entry");
    CHECK(parking_active_sessions(sys) == 1,
          "active session count updates after entry");
    CHECK(parking_enter(sys, "UP16AA0001", base, NULL) == PARKING_ALREADY_PARKED,
          "duplicate entry is rejected");
    CHECK(parking_enter(sys, "UP16ZZ9999", base, NULL) == PARKING_NOT_FOUND,
          "entry for an unregistered vehicle is rejected");

    CHECK(parking_exit(sys, "UP16AA0001", base + 1800, &receipt) == PARKING_OK,
          "student exit succeeds");
    CHECK(receipt.charge == 0, "student receipt shows Rs. 0");
    CHECK(receipt.duration_seconds == 1800, "exit duration is recorded");
    CHECK(parking_occupied_slots(sys) == 0, "slot is released after exit");
    CHECK(parking_active_sessions(sys) == 0,
          "active session count clears after exit");
    Vehicle *student = parking_find_vehicle(sys, "UP16AA0001");
    CHECK(parking_sessions_for(sys, student->id) == 1,
          "completed session remains in history");
    CHECK(parking_exit(sys, "UP16ZZ9999", base, &receipt) == PARKING_NOT_FOUND,
          "exit for an unregistered vehicle is rejected");
    CHECK(parking_exit(sys, "UP16AA0001", base + 1800, &receipt) ==
              PARKING_NOT_PARKED,
          "exit without an active session is rejected");

    CHECK(parking_enter(sys, "UP16AA0002", base, NULL) == PARKING_OK,
          "staff entry succeeds");
    CHECK(parking_exit(sys, "UP16AA0002", base + 4000, &receipt) == PARKING_OK &&
              receipt.charge == 0,
          "staff receipt shows Rs. 0");

    CHECK(parking_enter(sys, "UP16AA0003", base, NULL) == PARKING_OK,
          "visitor entry succeeds");
    CHECK(parking_exit(sys, "UP16AA0003", base + 1800, &receipt) == PARKING_OK &&
              receipt.charge == 20,
          "visitor pays Rs. 20 after thirty minutes");
    CHECK(parking_enter(sys, "UP16AA0003", base, NULL) == PARKING_OK,
          "visitor may re-enter after exit");
    CHECK(parking_exit(sys, "UP16AA0003", base + 5400, &receipt) == PARKING_OK &&
              receipt.charge == 40,
          "visitor pays Rs. 40 after ninety minutes");
    CHECK(parking_total_revenue(sys) == 60,
          "revenue totals completed visitor sessions");

    parking_destroy(sys);
}

static void test_capacity(void) {
    ParkingSystem *sys = make_system();

    /* Each section has 5 car slots (45 total / 9 pools). */
    size_t student_car_total = parking_section_type_total(
        sys, SECTION_STUDENT, VEHICLE_CAR);
    CHECK(student_car_total == 5, "student section has 5 car slots");
    size_t teacher_car_total = parking_section_type_total(
        sys, SECTION_TEACHER, VEHICLE_CAR);
    CHECK(teacher_car_total == 5, "teacher section has 5 car slots");
    size_t visitor_car_total = parking_section_type_total(
        sys, SECTION_VISITOR, VEHICLE_CAR);
    CHECK(visitor_car_total == 5, "visitor section has 5 car slots");

    /* Fill all teacher car slots */
    for (int i = 0; i < 5; i++) {
        char reg[16];
        snprintf(reg, sizeof reg, "UP16TC%04d", i);
        char sap[16];
        snprintf(sap, sizeof sap, "59000020%01d", i);
        CHECK(parking_register(sys, reg, "Teacher", sap, USER_STAFF,
                               VEHICLE_CAR, NULL) == PARKING_OK,
              "teacher car registration succeeds");
        CHECK(parking_enter(sys, reg, 1, NULL) == PARKING_OK,
              "teacher car entry succeeds");
    }
    /* Sixth teacher car entry should be full */
    CHECK(parking_register(sys, "UP16TC9999", "Extra", "590000299",
                           USER_STAFF, VEHICLE_CAR, NULL) == PARKING_OK,
          "extra teacher car registration succeeds");
    CHECK(parking_enter(sys, "UP16TC9999", 1, NULL) == PARKING_FULL,
          "teacher car section is full");

    parking_destroy(sys);
}

static void test_growth(void) {
    ParkingSystem *sys = make_system();
    char registration[32];
    char institution[32];
    int all_ok = 1;

    for (int i = 0; i < 200; i++) {
        snprintf(registration, sizeof registration, "UP16AA%04d", i);
        snprintf(institution, sizeof institution, "590000%03d", i);
        if (parking_register(sys, registration, "Owner", institution,
                             USER_STUDENT, VEHICLE_CAR,
                             NULL) != PARKING_OK) {
            all_ok = 0;
        }
    }

    CHECK(all_ok, "two hundred registrations succeed");
    CHECK(sys->vehicle_count == 200,
          "vehicle array grows dynamically with realloc");
    CHECK(parking_find_vehicle(sys, "UP16AA0199") != NULL,
          "records added after growth remain searchable");

    parking_destroy(sys);
}

static void test_persistence(void) {
    remove_test_file();
    ParkingSystem *sys = make_system();
    Id student_id = add_vehicle(sys, "UP16AA0001", USER_STUDENT, "590000001");
    add_vehicle(sys, "UP16AA0003", USER_VISITOR, NULL);

    CHECK(parking_enter(sys, "UP16AA0001", 1700000000, NULL) == PARKING_OK,
          "active session created before save");
    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK, "save succeeds");

    ParkingSystem *loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_OK, "load succeeds");
    CHECK(loaded->vehicle_count == 2, "vehicles are restored");
    CHECK(loaded->slot_count == DEFAULT_SLOTS, "slots are restored");
    CHECK(loaded->session_count == 1 && loaded->sessions[0].active,
          "active session is restored");

    /* Find the occupied student car slot and verify it is not available */
    Id student_slot = loaded->sessions[0].slot_id;
    int slot_found = 0;
    int slot_occupied = 0;
    for (size_t i = 0; i < loaded->slot_count; i++) {
        if (loaded->slots[i].id == student_slot) {
            slot_found = 1;
            slot_occupied = !loaded->slots[i].available;
            break;
        }
    }
    CHECK(slot_found, "student slot exists after load");
    CHECK(slot_occupied, "student car slot occupancy is restored");

    Vehicle *student = parking_find_vehicle(loaded, "UP16AA0001");
    CHECK(student != NULL && student->id == student_id,
          "vehicle IDs are stable across a restart");
    CHECK(student != NULL && strcmp(student->owner, "Test Owner") == 0 &&
              strcmp(student->institution_id, "590000001") == 0 &&
              student->vehicle_type == VEHICLE_CAR &&
              student->user_type == USER_STUDENT,
          "vehicle fields survive a restart");
    CHECK(loaded->sessions[0].start_time == 1700000000,
          "session start time survives a restart");
    CHECK(parking_active_sessions(loaded) == 1,
          "active session count survives a restart");

    Receipt receipt;
    CHECK(parking_exit(loaded, "UP16AA0001", 1700001800, &receipt) == PARKING_OK &&
              receipt.charge == 0,
          "a restored active session can exit");
    /* The student's slot should now be available again */
    slot_found = 0;
    for (size_t i = 0; i < loaded->slot_count; i++) {
        if (loaded->slots[i].id == student_slot) {
            slot_found = 1;
            CHECK(loaded->slots[i].available, "student slot is released on exit");
            break;
        }
    }
    CHECK(slot_found, "student slot still exists after exit");
    /* Visitor enters a visitor car slot */
    Id reused = ID_NONE;
    CHECK(parking_enter(loaded, "UP16AA0003", 1700001800, &reused) == PARKING_OK,
          "a restored system allows visitor entry");
    CHECK(parking_exit(loaded, "UP16AA0003", 1700003600, &receipt) == PARKING_OK &&
              receipt.charge == 20,
          "a restored system bills a visitor");

    Id next_id = add_vehicle(loaded, "UP16AA0020", USER_VISITOR, NULL);
    CHECK(next_id > student_id, "ID counter continues after load");

    parking_destroy(loaded);
    parking_destroy(sys);
    remove_test_file();
}

static void test_revenue_persistence(void) {
    remove_test_file();
    ParkingSystem *sys = make_system();
    add_vehicle(sys, "UP16AA0003", USER_VISITOR, NULL);
    Receipt receipt;

    CHECK(parking_enter(sys, "UP16AA0003", 1700000000, NULL) == PARKING_OK,
          "visitor entry before revenue save");
    CHECK(parking_exit(sys, "UP16AA0003", 1700005400, &receipt) == PARKING_OK,
          "visitor exit before revenue save");
    CHECK(parking_total_revenue(sys) == 40, "revenue computed before save");
    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK, "save revenue succeeds");

    ParkingSystem *loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_OK, "load revenue succeeds");
    CHECK(parking_total_revenue(loaded) == 40,
          "completed session revenue is restored");

    parking_destroy(loaded);
    parking_destroy(sys);
    remove_test_file();
}

static void test_corruption(void) {
    remove_test_file();
    FILE *file = fopen(TEST_PATH, "wb");
    if (file != NULL) {
        fputs("not a parking file", file);
        fclose(file);
    }

    ParkingSystem *sys = make_system();
    add_vehicle(sys, "UP16AA0030", USER_STUDENT, "590000030");

    CHECK(storage_load(sys, TEST_PATH) == PARKING_INVALID,
          "corrupt file is rejected");
    CHECK(sys->vehicle_count == 1,
          "a failed load leaves the current system intact");
    CHECK(storage_load(sys, "tests/does_not_exist.dat") == PARKING_NOT_FOUND,
          "a missing file is reported separately");

    file = fopen(TEST_PATH, "rb");
    char buffer[32] = {0};
    size_t count = 0;
    if (file != NULL) {
        count = fread(buffer, 1, sizeof buffer - 1, file);
        fclose(file);
    }
    CHECK(count > 0 && strcmp(buffer, "not a parking file") == 0,
          "loading does not overwrite the corrupt file");

    char backup[128];
    CHECK(storage_preserve(TEST_PATH, backup, sizeof backup) == PARKING_OK,
          "corrupt file can be preserved");
    CHECK(strcmp(backup, TEST_PATH ".bak") == 0,
          "preservation reports the backup path");
    remove(backup);
    remove_test_file();
    parking_destroy(sys);
}

static void test_old_format(void) {
    remove_test_file();
    FILE *file = fopen(TEST_PATH, "wb");
    if (file != NULL) {
        unsigned int magic = 0x504B4453u;
        unsigned int version = 1;
        fwrite(&magic, sizeof magic, 1, file);
        fwrite(&version, sizeof version, 1, file);
        fclose(file);
    }

    ParkingSystem *sys = make_system();
    CHECK(storage_load(sys, TEST_PATH) == PARKING_INVALID,
          "version 1 save file is rejected as incompatible");

    char backup[128];
    CHECK(storage_preserve(TEST_PATH, backup, sizeof backup) == PARKING_OK,
          "version 1 file can be preserved");
    CHECK(strcmp(backup, TEST_PATH ".bak") == 0,
          "version 1 backup path is reported");
    remove(backup);

    parking_destroy(sys);
    remove_test_file();
}

static void test_boundaries(void) {
    CHECK(parking_create(0) == NULL, "zero-slot system is rejected");
    CHECK(parking_create(MAX_SLOTS + 1) == NULL,
          "oversized system is rejected");

    ParkingSystem *sys = make_system();
    char long_text[MAX_REG_LEN + 4];
    memset(long_text, 'A', sizeof long_text - 1);
    long_text[sizeof long_text - 1] = '\0';

    CHECK(parking_register(sys, long_text, "Owner", "590000001", USER_STUDENT,
                           VEHICLE_CAR, NULL) == PARKING_INVALID_REGISTRATION,
          "overlong registration is rejected");
    CHECK(parking_register(sys, "UP16AA0040", NULL, "590000001", USER_STUDENT,
                           VEHICLE_CAR, NULL) == PARKING_INVALID,
          "missing owner is rejected");
    CHECK(parking_register(sys, "UP16AA0041", "Owner", NULL, USER_STUDENT,
                           VEHICLE_CAR, NULL) == PARKING_INVALID,
          "missing student ID is rejected");

    add_vehicle(sys, "UP16AA0042", USER_STUDENT, "590000042");
    add_vehicle(sys, "UP16AA0043", USER_STUDENT, "590000043");
    Id slot = ID_NONE;
    Receipt receipt;
    CHECK(parking_enter(sys, "UP16AA0042", 1, &slot) == PARKING_OK,
          "entry before slot reuse");
    CHECK(parking_exit(sys, "UP16AA0042", 2, &receipt) == PARKING_OK,
          "exit before slot reuse");
    Id reused = ID_NONE;
    CHECK(parking_enter(sys, "UP16AA0043", 3, &reused) == PARKING_OK &&
              reused == slot,
          "released slot is reused");
    CHECK(parking_exit(sys, "UP16ZZ9999", 4, &receipt) == PARKING_NOT_FOUND,
          "exit for an unknown vehicle is rejected");

    sys->next_id = LLONG_MAX;
    CHECK(parking_register(sys, "UP16AA0044", "Owner", "590000044", USER_STUDENT,
                           VEHICLE_CAR, NULL) == PARKING_LIMIT,
          "ID exhaustion is rejected");
    parking_destroy(sys);
}

static void test_registration_validation(void) {
    CHECK(registration_is_valid("UP16FU5674"),
          "UP16 Noida registration is valid");
    CHECK(registration_is_valid("UK07CC1234"),
          "UK07 Uttarakhand registration is valid");
    CHECK(registration_is_valid("DL01AB1234"),
          "DL01 Delhi registration is valid");
    CHECK(registration_is_valid("MH01AA0001"),
          "MH01 Mumbai registration is valid");
    CHECK(registration_is_valid("KA01AB1234"),
          "KA01 Bengaluru registration is valid");
    CHECK(registration_is_valid("TN01AB1234"),
          "TN01 Chennai registration is valid");
    CHECK(registration_is_valid("AP39AB1234"),
          "AP39 Andhra Pradesh registration is valid");
    CHECK(registration_is_valid("TS09AB1234"),
          "TS09 Telangana registration is valid");
    CHECK(registration_is_valid("AN01AB1234"),
          "AN01 Andaman registration is valid");
    CHECK(!registration_is_valid("UR78HJ7834"),
          "unknown state code is rejected");
    CHECK(!registration_is_valid("UP99AB1234"),
          "unknown RTO district is rejected");
    CHECK(!registration_is_valid("UP16FU567"),
          "nine-character registration is rejected");
    CHECK(!registration_is_valid("UP16FU56745"),
          "eleven-character registration is rejected");
    CHECK(!registration_is_valid("UP16F15674"),
          "digit in the series is rejected");
    CHECK(!registration_is_valid("UP16FU56A4"),
          "letter in the number is rejected");
    CHECK(!registration_is_valid("UP16 FU5674"),
          "space inside the registration is rejected");
    CHECK(!registration_is_valid(""),
          "empty registration is rejected");
    CHECK(!registration_is_valid(NULL),
          "null registration is rejected");

    char normalized[16];
    CHECK(registration_normalize(normalized, sizeof normalized,
                                 "up16fu5674") &&
              strcmp(normalized, "UP16FU5674") == 0,
          "lowercase registration is normalized to uppercase");
    CHECK(registration_normalize(normalized, sizeof normalized,
                                 " Up16Fu5674 ") &&
              strcmp(normalized, "UP16FU5674") == 0,
          "surrounding spaces are trimmed during normalization");
    CHECK(!registration_normalize(normalized, sizeof normalized,
                                  "UR78HJ7834"),
          "invalid registration is not normalized");
    CHECK(!registration_normalize(normalized, sizeof normalized,
                                  "UP16FU56745"),
          "eleven-character input is not normalized");
}

static void test_id_validation(void) {
    CHECK(sap_id_is_valid("590000000"), "lowest SAP ID is valid");
    CHECK(sap_id_is_valid("590099999"), "highest SAP ID is valid");
    CHECK(!sap_id_is_valid("590100000"), "SAP ID above range is rejected");
    CHECK(!sap_id_is_valid("589999999"), "SAP ID below range is rejected");
    CHECK(!sap_id_is_valid("59000000"), "eight-digit SAP ID is rejected");
    CHECK(!sap_id_is_valid("5900000000"), "ten-digit SAP ID is rejected");
    CHECK(!sap_id_is_valid("59000000A"), "non-numeric SAP ID is rejected");
    CHECK(!sap_id_is_valid(""), "empty SAP ID is rejected");
    CHECK(!sap_id_is_valid(NULL), "null SAP ID is rejected");

    CHECK(teacher_id_is_valid("123456789"), "nine-digit teacher ID is valid");
    CHECK(teacher_id_is_valid("000000000"),
          "teacher ID with leading zeros is valid");
    CHECK(!teacher_id_is_valid("12345678"), "eight-digit teacher ID is rejected");
    CHECK(!teacher_id_is_valid("1234567890"),
          "ten-digit teacher ID is rejected");
    CHECK(!teacher_id_is_valid("12345678A"),
          "non-numeric teacher ID is rejected");
    CHECK(!teacher_id_is_valid(""), "empty teacher ID is rejected");
    CHECK(!teacher_id_is_valid(NULL), "null teacher ID is rejected");
}

static void test_persistence_failures(void) {
    remove_test_file();
    ParkingSystem *sys = make_system();
    add_vehicle(sys, "UP16AA0050", USER_STUDENT, "590000050");

    CHECK(storage_save(sys, "tests/no_such_directory/file.dat") == PARKING_ERROR,
          "save to a missing directory fails");
    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK, "baseline save succeeds");

    FILE *temporary = fopen(TEST_PATH ".tmp", "rb");
    CHECK(temporary == NULL,
          "temporary file is removed after a successful save");
    if (temporary != NULL) {
        fclose(temporary);
    }

    FILE *file = fopen(TEST_PATH, "ab");
    if (file != NULL) {
        fputs("x", file);
        fclose(file);
    }
    ParkingSystem *loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_INVALID,
          "trailing bytes are rejected");
    parking_destroy(loaded);

    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK,
          "save before truncation test");
    file = fopen(TEST_PATH, "rb");
    unsigned char prefix[TRUNCATED_HEADER_SIZE];
    size_t prefix_size = 0;
    if (file != NULL) {
        prefix_size = fread(prefix, 1, sizeof prefix, file);
        fclose(file);
    }
    file = fopen(TEST_PATH, "wb");
    if (file != NULL) {
        fwrite(prefix, 1, prefix_size, file);
        fclose(file);
    }
    loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_INVALID,
          "truncated file is rejected");
    parking_destroy(loaded);

    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK,
          "save before invalid category test");
    file = fopen(TEST_PATH, "r+b");
    if (file != NULL) {
        int32_t bad_type = 99;
        fseek(file, FIRST_VEHICLE_CATEGORY_OFFSET, SEEK_SET);
        fwrite(&bad_type, sizeof bad_type, 1, file);
        fclose(file);
    }
    loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_INVALID,
          "invalid user category in a file is rejected");
    parking_destroy(loaded);

    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK,
          "save before invalid count test");
    file = fopen(TEST_PATH, "r+b");
    if (file != NULL) {
        uint64_t bad_count = UINT64_MAX;
        fseek(file, VEHICLE_COUNT_OFFSET, SEEK_SET);
        fwrite(&bad_count, sizeof bad_count, 1, file);
        fclose(file);
    }
    loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_INVALID,
          "invalid record count in a file is rejected");
    parking_destroy(loaded);

    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK,
          "save before backup fallback test");
    file = fopen(TEST_PATH ".bak", "wb");
    if (file != NULL) {
        fputs("occupied", file);
        fclose(file);
    }
    char backup[128];
    CHECK(storage_preserve(TEST_PATH, backup, sizeof backup) == PARKING_OK,
          "an existing backup name is skipped");
    CHECK(strcmp(backup, TEST_PATH ".bak.1") == 0,
          "the fallback backup path is reported");
    remove(backup);
    remove(TEST_PATH ".bak");

    parking_destroy(sys);
    remove_test_file();
}

static void test_inconsistent_state(void) {
    remove_test_file();
    ParkingSystem *sys = make_system();
    add_vehicle(sys, "UP16AA0060", USER_STUDENT, "590000060");
    add_vehicle(sys, "UP16AA0061", USER_STUDENT, "590000061");
    CHECK(parking_enter(sys, "UP16AA0060", 1700000000, NULL) == PARKING_OK,
          "entry for consistency tests");

    sys->slots[0].vehicle_id = 9999;
    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK,
          "inconsistent slot state is saved for testing");
    ParkingSystem *loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_INVALID,
          "occupied slot with a missing vehicle is rejected");
    parking_destroy(loaded);

    sys->slots[0].vehicle_id = sys->vehicles[0].id;
    sys->sessions[0].slot_id = 9999;
    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK,
          "inconsistent session state is saved for testing");
    loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_INVALID,
          "active session with a missing slot is rejected");
    parking_destroy(loaded);

    sys->sessions[0].slot_id = sys->slots[0].id;
    sys->vehicles[1].id = sys->vehicles[0].id;
    CHECK(storage_save(sys, TEST_PATH) == PARKING_OK,
          "duplicate IDs are saved for testing");
    loaded = make_system();
    CHECK(storage_load(loaded, TEST_PATH) == PARKING_INVALID,
          "duplicate vehicle IDs are rejected");
    parking_destroy(loaded);

    parking_destroy(sys);
    remove_test_file();
}

int main(void) {
    remove_test_file();

    test_registration_validation();
    test_id_validation();
    test_registration();
    test_billing();
    test_entry_and_exit();
    test_capacity();
    test_growth();
    test_persistence();
    test_revenue_persistence();
    test_corruption();
    test_old_format();
    test_boundaries();
    test_persistence_failures();
    test_inconsistent_state();

    remove_test_file();
    printf("\nTests run: %d, passed: %d, failed: %d\n",
           tests_run, tests_passed, tests_run - tests_passed);
    return tests_passed == tests_run ? 0 : 1;
}
