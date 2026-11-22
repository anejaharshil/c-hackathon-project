// Header Files
#include "Header Files/parking.h"
#include "Header Files/registration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Global Variables
// (none)

// Macros
#define TEXT_SIZE 32
#define INPUT_TEXT 128
#define BACKUP_PATH_SIZE 512

// Function Declarations
static int my_isspace(char c);
static void clear_screen(void);
static int read_line(const char *prompt, char *buffer, size_t size);
static void trim(char *text);
static int read_int(const char *prompt, int minimum, int maximum, int *out);
static void pause_screen(void);
static void print_header(const ParkingSystem *sys);
static void print_menu(void);
static void format_time(time_t value, char *buffer, size_t size);
static void format_duration(long seconds, char *buffer, size_t size);
static const char *status_message(ParkingStatus status);
static ParkingSystem *load_system(void);
static void register_vehicle(ParkingSystem *sys);
static void vehicle_entry(ParkingSystem *sys);
static void print_receipt(const Receipt *receipt);
static void vehicle_exit(ParkingSystem *sys);
static void parking_status(ParkingSystem *sys);
static void search_vehicle(ParkingSystem *sys);
static void statistics(const ParkingSystem *sys);
static ParkingStatus save_state(ParkingSystem *sys);

// main function
int main(void) {
    ParkingSystem *sys = load_system();
    if (sys == NULL) {
        return 1;
    }

    int running = 1;
    while (running) {
        clear_screen();
        print_header(sys);
        print_menu();

        int choice = 0;
        if (!read_int("\nEnter choice: ", 0, 6, &choice)) {
            break;
        }

        switch (choice) {
            case 1:
                register_vehicle(sys);
                break;
            case 2:
                vehicle_entry(sys);
                break;
            case 3:
                vehicle_exit(sys);
                break;
            case 4:
                parking_status(sys);
                break;
            case 5:
                search_vehicle(sys);
                break;
            case 6:
                statistics(sys);
                break;
            case 0:
                running = 0;
                break;
            default:
                break;
        }
    }

    /* Save only on exit — all changes live in memory during the session. */
    ParkingStatus saved = save_state(sys);
    clear_screen();
    if (saved == PARKING_OK) {
        puts("Data saved. Goodbye.");
    } else {
        puts("Warning: data could not be saved. Your changes may be lost.");
        pause_screen();
    }
    parking_destroy(sys);
    return 0;
}

// Functions
static int my_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\v' || c == '\f' || c == '\r';
}

static void clear_screen(void) {
    /* ANSI escape sequence works on modern terminals including Windows 10+,
     * avoiding a shell spawn that system("cls") would require. */
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

static int read_line(const char *prompt, char *buffer, size_t size) {
    if (prompt != NULL) {
        fputs(prompt, stdout);
        fflush(stdout);
    }
    if (fgets(buffer, (int)size, stdin) == NULL) {
        return 0;
    }

    size_t length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else if (length == size - 1) {
        int character;
        while ((character = getchar()) != '\n' && character != EOF) {
        }
    }
    return 1;
}

static void trim(char *text) {
    size_t start = 0;
    while (text[start] != '\0' && my_isspace(text[start])) {
        start++;
    }
    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }

    size_t length = strlen(text);
    while (length > 0 && my_isspace(text[length - 1])) {
        text[--length] = '\0';
    }
}

static int read_int(const char *prompt, int minimum, int maximum, int *out) {
    char buffer[TEXT_SIZE];
    for (;;) {
        if (!read_line(prompt, buffer, sizeof buffer)) {
            return 0;
        }
        trim(buffer);

        char *end = NULL;
        long value = strtol(buffer, &end, 10);
        if (end != buffer && *end == '\0' &&
            value >= minimum && value <= maximum) {
            *out = (int)value;
            return 1;
        }
        puts("Invalid input. Enter a number in the given range.");
    }
}

static void pause_screen(void) {
    char buffer[INPUT_TEXT];
    fputs("\nPress Enter to continue...", stdout);
    fflush(stdout);
    (void)read_line(NULL, buffer, sizeof buffer);
}

static void print_header(const ParkingSystem *sys) {
    printf("SMART CAMPUS PARKING SYSTEM\n");
    printf("Teacher: %zu reg / %zu occ / %zu  |  Student: %zu reg / %zu occ / %zu  |  Visitor: %zu reg / %zu occ / %zu\n",
           parking_section_registered(sys, SECTION_TEACHER),
           parking_section_occupied(sys, SECTION_TEACHER),
           parking_section_total(sys, SECTION_TEACHER),
           parking_section_registered(sys, SECTION_STUDENT),
           parking_section_occupied(sys, SECTION_STUDENT),
           parking_section_total(sys, SECTION_STUDENT),
           parking_section_registered(sys, SECTION_VISITOR),
           parking_section_occupied(sys, SECTION_VISITOR),
           parking_section_total(sys, SECTION_VISITOR));
    printf("Total registered: %zu  |  Total occupied: %zu / %zu\n\n",
           sys->vehicle_count,
           parking_occupied_slots(sys), sys->slot_count);
}

static void print_menu(void) {
    printf("========================================\n");
    printf("1. Register Vehicle\n");
    printf("2. Vehicle Entry\n");
    printf("3. Vehicle Exit\n");
    printf("4. Parking Status\n");
    printf("5. Search Vehicle\n");
    printf("6. Statistics\n");
    printf("0. Exit\n");
    printf("========================================\n");
}

static void format_time(time_t value, char *buffer, size_t size) {
    struct tm *local = localtime(&value);
    if (local == NULL ||
        strftime(buffer, size, "%Y-%m-%d %H:%M:%S", local) == 0) {
        snprintf(buffer, size, "unknown");
    }
}

static void format_duration(long seconds, char *buffer, size_t size) {
    if (seconds < 0) {
        seconds = 0;
    }
    long hours = seconds / HOUR_SECONDS;
    long minutes = (seconds % HOUR_SECONDS) / 60;
    long remainder = seconds % 60;
    snprintf(buffer, size, "%ldh %02ldm %02lds", hours, minutes, remainder);
}

static const char *status_message(ParkingStatus status) {
    switch (status) {
        case PARKING_NOT_FOUND:
            return "No matching vehicle was found.";
        case PARKING_DUPLICATE:
            return "That registration or institutional ID is already in use.";
        case PARKING_ALREADY_PARKED:
            return "That vehicle is already parked.";
        case PARKING_NOT_PARKED:
            return "That vehicle is not currently parked.";
        case PARKING_FULL:
            return "The section is full.";
        case PARKING_LIMIT:
            return "The record limit has been reached.";
        case PARKING_NO_MEMORY:
            return "Not enough memory to complete the operation.";
        case PARKING_INVALID:
            return "Invalid input. Please check the values and try again.";
        case PARKING_INVALID_REGISTRATION:
            return "Invalid registration. Use the format UP16AB1234 with a valid state and RTO code.";
        case PARKING_INVALID_ID:
            return "Invalid ID. Students need a 9-digit SAP ID (590000000-590099999); teachers need a 9-digit ID.";
        case PARKING_INVALID_TIME:
            return "Exit time is before entry time. Please correct the timestamp.";
        default:
            return "The operation could not be completed.";
    }
}

static ParkingStatus save_state(ParkingSystem *sys) {
    return storage_save(sys, DATA_PATH);
}

static ParkingSystem *load_system(void) {
    ParkingSystem *sys = parking_create(DEFAULT_SLOTS);
    if (sys == NULL) {
        fprintf(stderr, "Could not allocate the parking system.\n");
        return NULL;
    }

    ParkingStatus status = storage_load(sys, DATA_PATH);
    if (status == PARKING_OK || status == PARKING_NOT_FOUND) {
        return sys;
    }
    if (status == PARKING_NO_MEMORY) {
        fprintf(stderr, "Not enough memory to load saved data.\n");
        parking_destroy(sys);
        return NULL;
    }

    clear_screen();
    printf("The saved data at %s could not be read.\n", DATA_PATH);
    if (status == PARKING_INVALID) {
        char backup[BACKUP_PATH_SIZE];
        if (storage_preserve(DATA_PATH, backup, sizeof backup) == PARKING_OK) {
            printf("The original file was preserved as:\n  %s\n", backup);
            puts("The application will start with an empty database.");
        } else {
            puts("The original file could not be preserved.");
            puts("Saving is disabled for this session to protect it.");
        }
    } else {
        puts("Saving is disabled for this session.");
    }
    pause_screen();
    return sys;
}

static void register_vehicle(ParkingSystem *sys) {
    clear_screen();
    print_header(sys);
    puts("REGISTER VEHICLE\n");

    int category = 0;
    if (!read_int("Category (1 Student, 2 Teacher, 3 Visitor): ", 1, 3,
                  &category)) {
        return;
    }
    UserType user_type = (UserType)(category - 1);

    char owner[INPUT_TEXT];
    if (!read_line("Owner name: ", owner, sizeof owner)) {
        return;
    }
    trim(owner);

    /* Vehicle type is asked before registration because a bicycle may not
     * carry a registration number. */
    int kind = 0;
    if (!read_int("Vehicle type (1 Car, 2 Motorcycle, 3 Bicycle): ", 1, 3,
                  &kind)) {
        return;
    }
    VehicleType vehicle_type = (VehicleType)(kind - 1);

    char registration[INPUT_TEXT] = "";
    if (vehicle_type != VEHICLE_BICYCLE) {
        for (;;) {
            if (!read_line("Vehicle registration (e.g. UP16AB1234): ",
                           registration, sizeof registration)) {
                return;
            }
            trim(registration);
            char normalized[16];
            if (registration_normalize(normalized, sizeof normalized,
                                       registration)) {
                for (size_t i = 0; normalized[i] != '\0'; i++) {
                    registration[i] = normalized[i];
                }
                break;
            }
            puts("Invalid registration. Use the format UP16AB1234 with a "
                 "valid state and RTO code.");
            puts("Please retype the registration number.");
        }
    }

    char institution[INPUT_TEXT] = "-";
    if (user_type != USER_VISITOR) {
        const char *prompt = user_type == USER_STUDENT
                                 ? "SAP ID (590000000-590099999): "
                                 : "Teacher ID (9 digits): ";
        const char *bad_msg = user_type == USER_STUDENT
                                  ? "Invalid SAP ID. Enter a 9-digit value "
                                    "between 590000000 and 590099999."
                                  : "Invalid Teacher ID. Enter a 9-digit number.";
        for (;;) {
            if (!read_line(prompt, institution, sizeof institution)) {
                return;
            }
            trim(institution);
            int id_ok = user_type == USER_STUDENT
                            ? sap_id_is_valid(institution)
                            : teacher_id_is_valid(institution);
            if (id_ok) {
                break;
            }
            puts(bad_msg);
            puts("Please retype the ID.");
        }
    }

    Id id = ID_NONE;
    ParkingStatus status = parking_register(sys, registration, owner,
                                            institution, user_type,
                                            vehicle_type, &id);
    if (status == PARKING_OK) {
        printf("\nVehicle registered successfully (ID %lld).\n",
               (long long)id);
    } else {
        printf("\n%s\n", status_message(status));
    }
    pause_screen();
}

static void vehicle_entry(ParkingSystem *sys) {
    clear_screen();
    print_header(sys);
    puts("VEHICLE ENTRY\n");

    char registration[INPUT_TEXT];
    if (!read_line("Vehicle registration (leave blank for bicycle): ", registration,
                   sizeof registration)) {
        return;
    }
    trim(registration);

    Id slot_id = ID_NONE;
    ParkingStatus status;

    if (registration[0] == '\0') {
        /* Bicycle entry: look up by owner name since bicycles have no
         * registration number. */
        char owner[INPUT_TEXT];
        if (!read_line("Owner name: ", owner, sizeof owner)) {
            return;
        }
        trim(owner);

        Id matches[16];
        size_t match_count = parking_find_vehicles_by_owner_and_type(
            sys, owner, VEHICLE_BICYCLE, matches,
            sizeof matches / sizeof matches[0]);

        if (match_count == 0) {
            printf("\nNo bicycle registered for owner '%s'.\n", owner);
            pause_screen();
            return;
        }

        Id vehicle_id = ID_NONE;
        if (match_count == 1) {
            vehicle_id = matches[0];
        } else {
            /* Multiple bicycles for this owner - ask which one. */
            printf("\nMultiple bicycles registered for %s:\n", owner);
            for (size_t i = 0; i < match_count; i++) {
                Vehicle *v = parking_vehicle_by_id(sys, matches[i]);
                printf("  %zu. ID %lld\n", i + 1, (long long)v->id);
            }
            int choice = 0;
            if (!read_int("Select bicycle: ", 1, (int)match_count, &choice)) {
                return;
            }
            vehicle_id = matches[choice - 1];
        }

        status = parking_enter_by_id(sys, vehicle_id, time(NULL), &slot_id);
    } else {
        status = parking_enter(sys, registration, time(NULL), &slot_id);
    }

    if (status == PARKING_OK) {
        printf("\nEntry recorded. Assigned slot %lld.\n", (long long)slot_id);
    } else {
        printf("\n%s\n", status_message(status));
    }
    pause_screen();
}

static void print_receipt(const Receipt *receipt) {
    char entry[TEXT_SIZE];
    char exit_time[TEXT_SIZE];
    char duration[TEXT_SIZE];
    format_time(receipt->entry_time, entry, sizeof entry);
    format_time(receipt->exit_time, exit_time, sizeof exit_time);
    format_duration(receipt->duration_seconds, duration, sizeof duration);

    puts("\nPARKING RECEIPT");
    printf("Vehicle:   %s\n", receipt->registration);
    printf("Owner:     %s\n", receipt->owner);
    printf("Category:  %s\n", parking_user_type_name(receipt->user_type));
    printf("Slot:      %lld\n", (long long)receipt->slot_id);
    printf("Entry:     %s\n", entry);
    printf("Exit:      %s\n", exit_time);
    printf("Duration:  %s\n", duration);
    printf("Charge:    Rs. %ld\n", receipt->charge);
}

static void vehicle_exit(ParkingSystem *sys) {
    clear_screen();
    print_header(sys);
    puts("VEHICLE EXIT\n");

    char registration[INPUT_TEXT];
    if (!read_line("Vehicle registration (leave blank for bicycle): ",
                   registration, sizeof registration)) {
        return;
    }
    trim(registration);

    ParkingStatus status;

    if (registration[0] == '\0') {
        /* Bicycle exit: look up by owner name since bicycles have no
         * registration number. */
        char owner[INPUT_TEXT];
        if (!read_line("Owner name: ", owner, sizeof owner)) {
            return;
        }
        trim(owner);

        Id matches[16];
        size_t match_count = parking_find_vehicles_by_owner_and_type(
            sys, owner, VEHICLE_BICYCLE, matches,
            sizeof matches / sizeof matches[0]);

        if (match_count == 0) {
            printf("\nNo bicycle registered for owner '%s'.\n", owner);
            pause_screen();
            return;
        }

        Id vehicle_id = ID_NONE;
        if (match_count == 1) {
            vehicle_id = matches[0];
        } else {
            /* Multiple bicycles for this owner - ask which one. */
            printf("\nMultiple bicycles registered for %s:\n", owner);
            for (size_t i = 0; i < match_count; i++) {
                Vehicle *v = parking_vehicle_by_id(sys, matches[i]);
                printf("  %zu. ID %lld\n", i + 1, (long long)v->id);
            }
            int choice = 0;
            if (!read_int("Select bicycle: ", 1, (int)match_count,
                          &choice)) {
                return;
            }
            vehicle_id = matches[choice - 1];
        }

        Receipt receipt;
        status = parking_exit_by_id(sys, vehicle_id, time(NULL),
                                    &receipt);
        if (status == PARKING_OK) {
            print_receipt(&receipt);
        } else {
            printf("\n%s\n", status_message(status));
        }
        pause_screen();
        return;
    } else {
        Receipt receipt;
        status = parking_exit(sys, registration, time(NULL), &receipt);
        if (status == PARKING_OK) {
            print_receipt(&receipt);
        } else {
            printf("\n%s\n", status_message(status));
        }
        pause_screen();
        return;
    }

    if (status == PARKING_OK) {
        printf("\nExit recorded.\n");
    } else {
        printf("\n%s\n", status_message(status));
    }
    pause_screen();
}

static void parking_status(ParkingSystem *sys) {
    clear_screen();
    print_header(sys);
    puts("PARKING STATUS\n");

    const char *section_names[] = {"Teacher Parking", "Student Parking",
                                   "Visitor Parking"};
    for (int s = 0; s < SECTION_COUNT; s++) {
        printf("%s:\n", section_names[s]);
        const char *type_names[] = {"Car", "Motorcycle", "Bicycle"};
        for (int t = 0; t < VEHICLE_TYPE_COUNT; t++) {
            size_t occ = parking_section_type_occupied(
                sys, (ParkingSection)s, (VehicleType)t);
            size_t tot = parking_section_type_total(
                sys, (ParkingSection)s, (VehicleType)t);
            printf("  %-12s %zu/%zu\n", type_names[t], occ, tot);
        }
    }

    size_t occupied = parking_occupied_slots(sys);
    if (occupied == 0) {
        puts("\nNo vehicles are currently parked.");
    } else {
        printf("\n%-6s %-16s %-24s %s %s\n", "Slot", "Vehicle", "Owner",
               "Section", "Type");
        for (size_t i = 0; i < sys->slot_count; i++) {
            const ParkingSlot *slot = &sys->slots[i];
            if (slot->available) {
                continue;
            }
            Vehicle *vehicle = parking_vehicle_by_id(sys, slot->vehicle_id);
            if (vehicle == NULL) {
                printf("%-6lld %-16s %-24s %s %s\n",
                       (long long)slot->id, "(unknown)", "-", "-", "-");
            } else {
                printf("%-6lld %-16s %-24s %s %s\n",
                       (long long)slot->id, vehicle->registration,
                       vehicle->owner, parking_section_name(slot->section),
                       parking_vehicle_type_name(slot->type));
            }
        }
    }
    printf("\nFree slots: %zu\n", sys->slot_count - occupied);
    pause_screen();
}

static void search_vehicle(ParkingSystem *sys) {
    clear_screen();
    print_header(sys);
    puts("SEARCH VEHICLE\n");

    char registration[INPUT_TEXT];
    if (!read_line("Vehicle registration: ", registration,
                   sizeof registration)) {
        return;
    }
    trim(registration);

    Vehicle *vehicle = parking_find_vehicle(sys, registration);
    if (vehicle == NULL) {
        printf("\n%s\n", status_message(PARKING_NOT_FOUND));
        pause_screen();
        return;
    }

    printf("\nID:               %lld\n", (long long)vehicle->id);
    printf("Registration:     %s\n", vehicle->registration);
    printf("Owner:            %s\n", vehicle->owner);
    printf("Category:         %s\n",
           parking_user_type_name(vehicle->user_type));
    if (vehicle->user_type != USER_VISITOR) {
        printf("Institutional ID: %s\n", vehicle->institution_id);
    }
    printf("Vehicle type:     %s\n",
           parking_vehicle_type_name(vehicle->vehicle_type));

    ParkingSession *active = parking_active_session(sys, vehicle->id);
    if (active != NULL) {
        char entry[TEXT_SIZE];
        format_time(active->start_time, entry, sizeof entry);
        printf("Status:           Parked in slot %lld since %s\n",
               (long long)active->slot_id, entry);
    } else {
        puts("Status:           Not currently parked");
    }
    printf("Sessions on record: %zu\n",
           parking_sessions_for(sys, vehicle->id));
    pause_screen();
}

static void statistics(const ParkingSystem *sys) {
    clear_screen();
    print_header(sys);
    puts("STATISTICS\n");

    size_t active = parking_active_sessions(sys);
    size_t occupied = parking_occupied_slots(sys);
    size_t completed = sys->session_count - active;
    int occupancy = sys->slot_count == 0
                        ? 0
                        : (int)(occupied * 100 / sys->slot_count);

    printf("Registered vehicles: %zu\n", sys->vehicle_count);
    printf("Total slots:         %zu\n", sys->slot_count);
    printf("Occupied slots:      %zu\n", occupied);
    printf("Active sessions:     %zu\n", active);
    printf("Completed sessions:  %zu\n", completed);
    printf("Occupancy:           %d%%\n", occupancy);
    printf("Total revenue:       Rs. %ld\n", parking_total_revenue(sys));
    pause_screen();
}
