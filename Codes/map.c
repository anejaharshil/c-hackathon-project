// Header Files
#include "Header Files/map.h"
#include "Header Files/registration.h"

// Header Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global Variables
// (none)

// Macros
#define MAP_COLS 160
#define MAP_ROWS 52
#define GRID_COLS 160
#define GRID_ROWS 52

/* Campus lot slot pools, keyed to ParkingSlot section/type. Slots are
 * allocated round-robin by section then type, so section s/type t occupies
 * pool index s*3+t with 5 bays each. The display labels A/B/C map to
 * Student/Teacher/Visitor sections. */
#define LOT_STUDENT 0
#define LOT_STAFF 1
#define LOT_VISITOR 2

/* Display slot label prefixes per lot/section. */
#define LOT_STUDENT_NAME "Student"
#define LOT_STAFF_NAME "Staff"
#define LOT_VISITOR_NAME "Visitor"

// Function Declarations
static void grid_clear(char grid[GRID_ROWS][GRID_COLS + 1]);
static void grid_rect(char grid[GRID_ROWS][GRID_COLS + 1], int r0, int r1,
                      int c0, int c1, char ch);
static void grid_hline(char grid[GRID_ROWS][GRID_COLS + 1], int r,
                       int c0, int c1, char ch);
static void grid_vline(char grid[GRID_ROWS][GRID_COLS + 1], int c,
                       int r0, int r1, char ch);
static void grid_text(char grid[GRID_ROWS][GRID_COLS + 1], int r, int c,
                      const char *text);
static void draw_buildings(char grid[GRID_ROWS][GRID_COLS + 1]);
static void draw_roads(char grid[GRID_ROWS][GRID_COLS + 1]);
static void draw_landscaping(char grid[GRID_ROWS][GRID_COLS + 1]);
static void draw_slot_bays(char grid[GRID_ROWS][GRID_COLS + 1],
                           const ParkingSystem *sys);
static void render_grid(char grid[GRID_ROWS][GRID_COLS + 1]);

/* Slot placement: one entry per real ParkingSlot id (1..45). Each entry gives
 * the map row/col of the bay top-left and the vehicle-type display letter.
 * Slots are allocated round-robin by section then type (5 per pool):
 *   Teacher  : Car 1-5, Moto 6-10, Bike 11-15  (East lot)
 *   Student  : Car 16-20, Moto 21-25, Bike 26-30 (West lot)
 *   Visitor  : Car 31-35, Moto 36-40, Bike 41-45 (South lot)
 */
typedef struct {
    int row;
    int col;
    char type_char; /* 'C' car, 'M' motorcycle, 'B' bicycle */
} SlotPlacement;

static const SlotPlacement SLOT_PLACEMENTS[MAX_SLOTS] = {
    /* Teacher / Staff cars: IDs 1-5 (East lot car section) */
    {31, 114, 'C'}, {31, 118, 'C'}, {31, 122, 'C'}, {33, 114, 'C'}, {33, 118, 'C'},
    /* Teacher / Staff motorcycles: IDs 6-10 */
    {35, 114, 'M'}, {35, 117, 'M'}, {35, 120, 'M'}, {35, 123, 'M'}, {35, 126, 'M'},
    /* Teacher / Staff bicycles: IDs 11-15 */
    {37, 114, 'B'}, {37, 116, 'B'}, {37, 118, 'B'}, {37, 120, 'B'}, {37, 122, 'B'},
    /* Student cars: IDs 16-20 (West lot car section) */
    {31, 10, 'C'}, {31, 14, 'C'}, {31, 18, 'C'}, {33, 10, 'C'}, {33, 14, 'C'},
    /* Student motorcycles: IDs 21-25 */
    {35, 10, 'M'}, {35, 13, 'M'}, {35, 16, 'M'}, {35, 19, 'M'}, {35, 22, 'M'},
    /* Student bicycles: IDs 26-30 */
    {37, 10, 'B'}, {37, 12, 'B'}, {37, 14, 'B'}, {37, 16, 'B'}, {37, 18, 'B'},
    /* Visitor cars: IDs 31-35 (South lot car section) */
    {44, 60, 'C'}, {44, 64, 'C'}, {44, 68, 'C'}, {45, 60, 'C'}, {45, 64, 'C'},
    /* Visitor motorcycles: IDs 36-40 */
    {46, 60, 'M'}, {46, 63, 'M'}, {46, 66, 'M'}, {46, 69, 'M'}, {46, 72, 'M'},
    /* Visitor bicycles: IDs 41-45 */
    {47, 60, 'B'}, {47, 62, 'B'}, {47, 64, 'B'}, {47, 66, 'B'}, {47, 68, 'B'},
};

// Functions
void view_campus_map(const ParkingSystem *sys) {
    if (sys == NULL) {
        return;
    }
    char grid[GRID_ROWS][GRID_COLS + 1];
    grid_clear(grid);
    draw_buildings(grid);
    draw_roads(grid);
    draw_landscaping(grid);
    draw_slot_bays(grid, sys);
    render_grid(grid);
}

static void grid_clear(char grid[GRID_ROWS][GRID_COLS + 1]) {
    for (int r = 0; r < GRID_ROWS; r++) {
        memset(grid[r], ' ', GRID_COLS);
        grid[r][GRID_COLS] = '\0';
    }
}

static void grid_rect(char grid[GRID_ROWS][GRID_COLS + 1], int r0, int r1,
                      int c0, int c1, char ch) {
    for (int r = r0; r <= r1 && r < GRID_ROWS; r++) {
        for (int c = c0; c <= c1 && c < GRID_COLS; c++) {
            if (r >= 0 && c >= 0) {
                grid[r][c] = ch;
            }
        }
    }
}

static void grid_hline(char grid[GRID_ROWS][GRID_COLS + 1], int r,
                       int c0, int c1, char ch) {
    if (r < 0 || r >= GRID_ROWS) {
        return;
    }
    for (int c = c0; c <= c1 && c < GRID_COLS; c++) {
        if (c >= 0) {
            grid[r][c] = ch;
        }
    }
}

static void grid_vline(char grid[GRID_ROWS][GRID_COLS + 1], int c,
                       int r0, int r1, char ch) {
    if (c < 0 || c >= GRID_COLS) {
        return;
    }
    for (int r = r0; r <= r1 && r < GRID_ROWS; r++) {
        if (r >= 0) {
            grid[r][c] = ch;
        }
    }
}

static void grid_text(char grid[GRID_ROWS][GRID_COLS + 1], int r, int c,
                      const char *text) {
    if (r < 0 || r >= GRID_ROWS || c < 0 || c >= GRID_COLS) {
        return;
    }
    for (int i = 0; text[i] != '\0' && c + i < GRID_COLS; i++) {
        grid[r][c + i] = text[i];
    }
}

static void draw_buildings(char grid[GRID_ROWS][GRID_COLS + 1]) {
    /* North campus: Academic Complex (left), Central Library (center),
     * Research Centre (right). Buildings are hollow rectangles so slot bays
     * placed inside them remain visible. */
    grid_hline(grid, 3, 5, 34, '#');
    grid_hline(grid, 9, 5, 34, '#');
    grid_vline(grid, 5, 3, 9, '#');
    grid_vline(grid, 34, 3, 9, '#');
    grid_text(grid, 3, 12, "Academic Complex");
    grid_text(grid, 8, 12, "Main Lecture Wing");

    grid_hline(grid, 3, 58, 87, '#');
    grid_hline(grid, 9, 58, 87, '#');
    grid_vline(grid, 58, 3, 9, '#');
    grid_vline(grid, 87, 3, 9, '#');
    grid_text(grid, 3, 66, "Central Library");
    grid_text(grid, 8, 66, "Reading Hall");

    grid_hline(grid, 3, 111, 140, '#');
    grid_hline(grid, 9, 111, 140, '#');
    grid_vline(grid, 111, 3, 9, '#');
    grid_vline(grid, 140, 3, 9, '#');
    grid_text(grid, 3, 118, "Research Centre");
    grid_text(grid, 8, 118, "Labs Wing");

    /* Central campus: Great Hall (center). */
    grid_hline(grid, 16, 62, 98, '#');
    grid_hline(grid, 24, 62, 98, '#');
    grid_vline(grid, 62, 16, 24, '#');
    grid_vline(grid, 98, 16, 24, '#');
    grid_text(grid, 16, 74, "Great Hall");
    grid_text(grid, 21, 74, "Quadrangle Building");

    /* West campus: Student Centre (left). */
    grid_hline(grid, 30, 5, 34, '#');
    grid_hline(grid, 38, 5, 34, '#');
    grid_vline(grid, 5, 30, 38, '#');
    grid_vline(grid, 34, 30, 38, '#');
    grid_text(grid, 30, 16, "Student Centre");
    grid_text(grid, 35, 16, "Cafeteria & Lounge");

    /* East campus: Administration Building (right). */
    grid_hline(grid, 30, 111, 140, '#');
    grid_hline(grid, 38, 111, 140, '#');
    grid_vline(grid, 111, 30, 38, '#');
    grid_vline(grid, 140, 30, 38, '#');
    grid_text(grid, 30, 122, "Administration");
    grid_text(grid, 35, 122, "Registrar's Office");

    /* South campus: Security Gate (center-left). */
    grid_hline(grid, 44, 58, 87, '#');
    grid_hline(grid, 48, 58, 87, '#');
    grid_vline(grid, 58, 44, 48, '#');
    grid_vline(grid, 87, 44, 48, '#');
    grid_text(grid, 44, 70, "Main Entrance");
    grid_text(grid, 47, 70, "Security Gate");
}

static void draw_roads(char grid[GRID_ROWS][GRID_COLS + 1]) {
    /* North-south internal boulevard (center). */
    grid_vline(grid, 70, 11, 42, '=');
    grid_vline(grid, 71, 11, 42, '=');

    /* East-west campus spine connecting sections (row 14). */
    grid_hline(grid, 14, 35, 125, '=');

    /* West campus internal road. */
    grid_hline(grid, 29, 35, 55, '=');
    grid_vline(grid, 20, 11, 29, '=');

    /* East campus internal road. */
    grid_hline(grid, 29, 105, 125, '=');
    grid_vline(grid, 140, 11, 29, '=');

    /* South entrance boulevard connecting to central spine. */
    grid_hline(grid, 43, 55, 85, '=');
    grid_vline(grid, 55, 39, 43, '=');
    grid_vline(grid, 85, 39, 43, '=');

    /* Access roads from each lot to the central boulevard. */
    grid_vline(grid, 50, 12, 29, '=');   /* Student lot access */
    grid_vline(grid, 90, 12, 29, '=');   /* Staff lot access */
    grid_vline(grid, 70, 44, 43, '=');   /* Visitor lot access */
}

static void draw_landscaping(char grid[GRID_ROWS][GRID_COLS + 1]) {
    /* North campus gardens between buildings. */
    grid_rect(grid, 5, 7, 36, 56, '.');
    grid_rect(grid, 5, 7, 88, 110, '.');

    /* Central quadrangle. */
    grid_rect(grid, 25, 28, 64, 96, '.');
    grid_text(grid, 26, 80, "Central Quadrangle");

    /* West campus green. */
    grid_rect(grid, 31, 37, 36, 54, '.');

    /* East campus courtyard. */
    grid_rect(grid, 31, 37, 112, 130, '.');

    /* South entrance lawn. */
    grid_rect(grid, 45, 47, 60, 86, '.');
    grid_rect(grid, 41, 42, 50, 90, '.');
}

static void draw_slot_bays(char grid[GRID_ROWS][GRID_COLS + 1],
                           const ParkingSystem *sys) {
    if (sys == NULL) {
        return;
    }
    for (size_t i = 0; i < sys->slot_count; i++) {
        const ParkingSlot *slot = &sys->slots[i];
        if (slot->id < 1 || slot->id > MAX_SLOTS) {
            continue;
        }
        const SlotPlacement *p = &SLOT_PLACEMENTS[(size_t)slot->id - 1];
        if (p->row < 0 || p->row >= GRID_ROWS || p->col < 0 ||
            p->col + 3 >= GRID_COLS) {
            continue;
        }
        char bay_char = slot->available ? '.' : '#';
        char bay[5];
        snprintf(bay, sizeof bay, "%c%c%c", p->type_char, bay_char,
                 p->type_char);
        grid_text(grid, p->row, p->col, bay);
        /* Label the slot id below the bay. */
        char id_text[8];
        snprintf(id_text, sizeof id_text, "%lld", (long long)slot->id);
        int id_w = (int)strlen(id_text);
        int id_col = p->col + (3 - id_w) / 2;
        if (id_col < 0) {
            id_col = 0;
        }
        grid_text(grid, p->row + 1, id_col, id_text);
    }
}

static void render_grid(char grid[GRID_ROWS][GRID_COLS + 1]) {
    for (int r = 0; r < GRID_ROWS; r++) {
        fputs(grid[r], stdout);
        fputc('\n', stdout);
    }
}
