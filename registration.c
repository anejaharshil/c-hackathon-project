// Header Files
#include "registration.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// Global Variables
typedef struct {
    const char *state;
    const char *districts;
} RtoState;

static const RtoState RTO_STATES[] = {
    { "AN", "01 02" },
    { "AP", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40" },
    { "AR", "01 02" },
    { "DL", "01 02 03 04 05 06 07 08 09 10" },
    { "DN", "01 02" },
    { "DD", "01 02" },
    { "GA", "01 02" },
    { "GJ", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22" },
    { "HR", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15" },
    { "HP", "01 02 03 04 05 06 07 08 09 10 11" },
    { "JK", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17" },
    { "JH", "01 02 03 04 05 06 07 08 09 10 11 12" },
    { "KA", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20" },
    { "KL", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30" },
    { "MP", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20" },
    { "MH", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20" },
    { "MN", "01 02" },
    { "ML", "01 02" },
    { "MZ", "01 02" },
    { "NL", "01 02" },
    { "OR", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31" },
    { "PY", "01 02 03 04 05 06 07 08 09 10 11 12" },
    { "PN", "01 02" },
    { "RJ", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34" },
    { "SK", "01 02 03 04 05 06 07 08 09 10 11 12 13 14" },
    { "TN", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36" },
    { "TS", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20" },
    { "TG", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20" },
    { "UP", "11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96" },
    { "UK", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20" },
    { "WB", "01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96" },
};

// Macros
#define REGISTRATION_LENGTH 10
#define SAP_MIN 590000000L
#define SAP_MAX 590099999L

// Function Declarations
static const char *districts_for(const char *state);
static int district_is_listed(const char *districts, const char *code);

// Functions
/* definitions */

static const char *districts_for(const char *state) {
    for (int i = 0; i < (int)(sizeof RTO_STATES / sizeof RTO_STATES[0]); i++) {
        if (strncmp(RTO_STATES[i].state, state, 2) == 0) {
            return RTO_STATES[i].districts;
        }
    }
    return NULL;
}

static int district_is_listed(const char *districts, const char *code) {
    if (districts == NULL || code == NULL) {
        return 0;
    }
    const char *p = districts;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (strncmp(p, code, 2) == 0 && (p[2] == ' ' || p[2] == '\0')) {
            return 1;
        }
        while (*p && *p != ' ') p++;
    }
    return 0;
}

int registration_is_valid(const char *text) {
    if (text == NULL) {
        return 0;
    }
    size_t len = strlen(text);
    if (len != REGISTRATION_LENGTH) {
        return 0;
    }
    // Check pattern: [A-Z]{2}[0-9]{2}[A-Z]{2}[0-9]{4}
    for (int i = 0; i < 2; i++) {
        if (!isupper((unsigned char)text[i])) {
            return 0;
        }
    }
    for (int i = 2; i < 4; i++) {
        if (!isdigit((unsigned char)text[i])) {
            return 0;
        }
    }
    for (int i = 4; i < 6; i++) {
        if (!isupper((unsigned char)text[i])) {
            return 0;
        }
    }
    for (int i = 6; i < 10; i++) {
        if (!isdigit((unsigned char)text[i])) {
            return 0;
        }
    }
    // Find state entry and check district
    const char *districts = districts_for(&text[0]);
    if (districts == NULL) {
        return 0;
    }
    return district_is_listed(districts, &text[2]);
}

int registration_normalize(char *out, size_t size, const char *text) {
    if (out == NULL || text == NULL || size < 11) {
        return 0;
    }
    // Trim leading whitespace
    const char *src = text;
    while (*src && isspace((unsigned char)*src)) {
        src++;
    }
    // Trim trailing whitespace: find end
    size_t raw_len = strlen(src);
    const char *end = src + raw_len;
    while (end > src && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    // Uppercase and copy to a temporary buffer, validating at the same time
    char normalized[11]; // exactly 10 chars + NUL
    size_t i;
    for (i = 0; i < 10 && src + i < end; i++) {
        char c = (unsigned char)src[i];
        // Check if it's a letter or digit; after trimming, we expect exactly 10 chars
        normalized[i] = toupper(c);
    }
    normalized[10] = '\0';
    // But we need to validate the format. Let's check length matches exactly.
    // After trimming, if the length is not 10, it's invalid.
    if (i != 10) {
        out[0] = '\0';
        return 0;
    }
    // Now validate using registration_is_valid
    if (!registration_is_valid(normalized)) {
        out[0] = '\0';
        return 0;
    }
    // NUL-terminate out up to size-1
    size_t copy_len = strlen(normalized) + 1; // +1 for NUL
    if (copy_len > size) {
        copy_len = size;
    }
    memcpy(out, normalized, copy_len);
    return 1;
}

int sap_id_is_valid(const char *text) {
    if (text == NULL) {
        return 0;
    }
    size_t len = strlen(text);
    if (len != 9) {
        return 0;
    }
    for (int i = 0; i < 9; i++) {
        if (!isdigit((unsigned char)text[i])) {
            return 0;
        }
    }
    // Check range: 590000000 - 590099999
    long val = atol(text);
    return val >= SAP_MIN && val <= SAP_MAX;
}

int teacher_id_is_valid(const char *text) {
    if (text == NULL) {
        return 0;
    }
    size_t len = strlen(text);
    if (len != 9) {
        return 0;
    }
    for (int i = 0; i < 9; i++) {
        if (!isdigit((unsigned char)text[i])) {
            return 0;
        }
    }
    return 1;
}