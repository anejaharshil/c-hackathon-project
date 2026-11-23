// Header Files
#include "Header Files/map.h"
#include "Header Files/registration.h"

// Header Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Macros
#define MAP_COLS 160
#define MAP_ROWS 45

/* Cell attribute (colour) values. */
#define CELL_NORMAL 0   /* default terminal colour */
#define CELL_AVAILABLE 1  /* green - available parking bay */
#define CELL_OCCUPIED 2   /* red - occupied parking bay */

/* Cell: a single character cell with an associated attribute. */
typedef struct {
    char ch;
    int attr;
} Cell;

// Global Variables
static Cell grid[MAP_ROWS][MAP_COLS + 1];

// Function Declarations
static void grid_clear(void);
static void draw_horizontal_line(int r, int c0, int c1, char ch);
static void draw_vertical_line(int c, int r0, int r1, char ch);
static void draw_rectangle(int r0, int r1, int c0, int c1, char ch);
static void draw_text(int r, int c, const char *text);
static void draw_building(int r0, int r1, int c0, int c1,
                          const char *name, const char *sub);
static void draw_road_h(int r, int c0, int c1);
static void draw_road_v(int c, int r0, int r1);
static void draw_parking_bay(int r, int c, char type_char, int slot_id,
                             int available);
static void render_grid(void);
static void draw_buildings(void);
static void draw_roads(void);
static void draw_landscaping(void);
static void draw_slot_bays(const ParkingSystem *sys);

// Slot placement: one entry per real ParkingSlot id (1..45). Each entry gives
// the map row/col of the bay top-left and the vehicle-type display letter.
// Slots are allocated round-robin by section then type (5 bays per pool):
//   Teacher  : Car 1-5, Moto 6-10, Bike 11-15   (East / Staff lot)
//   Student  : Car 16-20, Moto 21-25, Bike 26-30 (West / Student lot)
//   Visitor  : Car 31-35, Moto 36-40, Bike 41-45 (South / Visitor lot)
typedef struct {
    int row;
    int col;
    char type_char; /* 'C' car, 'M' motorcycle, 'B' bicycle */
} SlotPlacement;

static const SlotPlacement SLOT_PLACEMENTS[MAX_SLOTS] = {
    /* Teacher / Staff cars: IDs 1-5 (East lot car section) */
    {32, 74, 'C'}, {32, 78, 'C'}, {32, 82, 'C'}, {32, 86, 'C'}, {32, 90, 'C'},
    /* Teacher / Staff motorcycles: IDs 6-10 */
    {34, 114, 'M'}, {34, 117, 'M'}, {34, 120, 'M'}, {34, 123, 'M'}, {34, 126, 'M'},
    /* Teacher / Staff bicycles: IDs 11-15 */
    {36, 114, 'B'}, {36, 117, 'B'}, {36, 120, 'B'}, {36, 123, 'B'}, {36, 126, 'B'},
    /* Student cars: IDs 16-20 (West lot car section) */
    {32, 40, 'C'}, {32, 44, 'C'}, {32, 48, 'C'}, {32, 52, 'C'}, {32, 56, 'C'},
    /* Student motorcycles: IDs 21-25 */
    {34, 40, 'M'}, {34, 44, 'M'}, {34, 48, 'M'}, {34, 52, 'M'}, {34, 56, 'M'},
    /* Student bicycles: IDs 26-30 */
    {36, 40, 'B'}, {36, 44, 'B'}, {36, 48, 'B'}, {36, 52, 'B'}, {36, 56, 'B'},
    /* Visitor cars: IDs 31-35 (South lot car section) */
    {41, 60, 'C'}, {41, 64, 'C'}, {41, 68, 'C'}, {41, 72, 'C'}, {41, 76, 'C'},
    /* Visitor motorcycles: IDs 36-40 */
    {42, 60, 'M'}, {42, 63, 'M'}, {42, 66, 'M'}, {42, 69, 'M'}, {42, 72, 'M'},
    /* Visitor bicycles: IDs 41-45 */
    {43, 60, 'B'}, {43, 63, 'B'}, {43, 66, 'B'}, {43, 69, 'B'}, {43, 72, 'B'},
};

// Functions
void view_campus_map(const ParkingSystem *sys) {
    if (sys == NULL) {
        return;
    }
    grid_clear();
    draw_buildings();
    draw_roads();
    draw_landscaping();
    draw_slot_bays(sys);
    render_grid();
}

static void grid_clear(void) {
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            grid[r][c].ch = ' ';
            grid[r][c].attr = CELL_NORMAL;
        }
    }
}

static void draw_horizontal_line(int r, int c0, int c1, char ch) {
    if (r < 0 || r >= MAP_ROWS) {
        return;
    }
    if (c0 < 0) c0 = 0;
    if (c1 >= MAP_COLS) c1 = MAP_COLS - 1;
    for (int c = c0; c <= c1; c++) {
        grid[r][c].ch = ch;
    }
}

static void draw_vertical_line(int c, int r0, int r1, char ch) {
    if (c < 0 || c >= MAP_COLS) return;
    if (r0 < 0) r0 = 0;
    if (r1 >= MAP_ROWS) r1 = MAP_ROWS - 1;
    for (int r = r0; r <= r1; r++) {
        grid[r][c].ch = ch;
    }
}

static void draw_rectangle(int r0, int r1, int c0, int c1, char ch) {
    draw_horizontal_line(r0, c0, c1, ch);
    draw_horizontal_line(r1, c0, c1, ch);
    draw_vertical_line(c0, r0, r1, ch);
    draw_vertical_line(c1, r0, r1, ch);
}

static void draw_text(int r, int c, const char *text) {
    if (r < 0 || r >= MAP_ROWS || c < 0 || c >= MAP_COLS) return;
    for (int i = 0; text[i] != '\0' && c + i < MAP_COLS; i++) {
        grid[r][c + i].ch = text[i];
    }
}

static void draw_building(int r0, int r1, int c0, int c1,
                          const char *name, const char *sub) {
    draw_rectangle(r0, r1, c0, c1, '#');
    int name_c = c0 + ((c1 - c0 + 1) - (int)strlen(name)) / 2;
    if (name_c < c0 + 1) name_c = c0 + 1;
    draw_text(r0 + 1, name_c, name);
    if (sub != NULL) {
        int sub_c = c0 + ((c1 - c0 + 1) - (int)strlen(sub)) / 2;
        if (sub_c < c0 + 1) sub_c = c0 + 1;
        draw_text(r0 + 3, sub_c, sub);
    }
}

static void draw_road_h(int r, int c0, int c1) {
    draw_horizontal_line(r, c0, c1, '=');
}

static void draw_road_v(int c, int r0, int r1) {
    draw_vertical_line(c, r0, r1, '=');
}

static void draw_parking_bay(int r, int c, char type_char, int slot_id,
                             int available) {
    if (r < 0 || r >= MAP_ROWS || c < 0 || c + 3 >= MAP_COLS) return;
    int attr = available ? CELL_AVAILABLE : CELL_OCCUPIED;
    char disp_char = available ? type_char : '#';
    char bay[5];
    snprintf(bay, sizeof bay, "%c%02d", disp_char, slot_id);
    for (int i = 0; bay[i] != '\0' && c + i < MAP_COLS; i++) {
        grid[r][c + i].ch = bay[i];
        grid[r][c + i].attr = attr;
    }
}

static void render_grid(void) {
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            int attr = grid[r][c].attr;
            if (attr == CELL_AVAILABLE) fputs("\033[32m", stdout);
            else if (attr == CELL_OCCUPIED) fputs("\033[31m", stdout);
            fputc(grid[r][c].ch, stdout);
            if (attr != CELL_NORMAL) fputs("\033[0m", stdout);
        }
        fputc('\n', stdout);
    }
}

static void draw_buildings(void) {
    /* North campus: Academic Complex (left), Central Library (center),
     * Research Centre (right). Hollow rectangles. */
    draw_building(3, 9, 6, 32, "Academic Complex", "Main Lecture Wing");
    draw_building(3, 9, 54, 85, "Central Library", "Reading Hall");
    draw_building(3, 9, 108, 140, "Research Centre", "Labs Wing");

    /* Central campus: Great Hall. */
    draw_building(16, 24, 62, 98, "Great Hall", "Quadrangle Building");

    /* West campus: Student Centre. */
    draw_building(30, 38, 6, 32, "Student Centre", "Cafeteria & Lounge");

    /* East campus: Administration Building. */
    draw_building(30, 38, 112, 140, "Administration", "Registrar's Office");

    /* South campus: Security Gate / Main Entrance. */
    draw_building(44, 48, 58, 87, "Main Entrance", "Security Gate");
}

static void draw_roads(void) {
    /* North-south internal boulevard (center). */
    draw_road_v(70, 11, 42);

    /* East-west campus spine connecting sections (row 14). */
    draw_road_h(14, 35, 125);

    /* West campus internal road. */
    draw_road_h(29, 36, 54);
    draw_road_v(20, 11, 29);

    /* East campus internal road. */
    draw_road_h(29, 106, 124);
    draw_road_v(140, 11, 29);

    /* South entrance boulevard connecting to central spine. */
    draw_road_h(43, 55, 85);
    draw_road_v(55, 39, 43);
    draw_road_v(85, 39, 43);

    /* Access roads from each lot to the central boulevard. */
    draw_road_v(50, 12, 29);   /* Student lot access */
    draw_road_v(90, 12, 29);   /* Staff lot access */
    draw_road_v(70, 44, 43);   /* Visitor lot access */
}

static void draw_landscaping(void) {
    /* North campus gardens between buildings. */
    draw_rectangle(5, 7, 36, 54, '.');
    draw_rectangle(5, 7, 86, 110, '.');

    /* Central quadrangle. */
    draw_rectangle(25, 28, 64, 96, '.');
    draw_text(26, 80, "Central Quadrangle");

    /* West campus green. */
    draw_rectangle(31, 37, 36, 54, '.');

    /* East campus courtyard. */
    draw_rectangle(31, 37, 112, 130, '.');

    /* South entrance lawn. */
    draw_rectangle(41, 42, 50, 90, '.');
}

static void draw_slot_bays(const ParkingSystem *sys) {
    if (sys == NULL) return;
    for (size_t i = 0; i < sys->slot_count; i++) {
        const ParkingSlot *slot = &sys->slots[i];
        if (slot->id < 1 || slot->id > MAX_SLOTS) continue;
        const SlotPlacement *p = &SLOT_PLACEMENTS[(size_t)slot->id - 1];
        if (p->row < 0 || p->row >= MAP_ROWS || p->col < 0 ||
            p->col + 3 >= MAP_COLS) continue;
        draw_parking_bay(p->row, p->col, p->type_char, (int)slot->id,
                         slot->available);
    }
}
