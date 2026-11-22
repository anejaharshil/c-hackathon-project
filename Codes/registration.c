// Header Files
#include "Header Files/registration.h"
#include <string.h>
#include <stdlib.h>

// Global Variables
// (none)

// Macros
#define REGISTRATION_LENGTH 10
#define SAP_MIN 590000000L
#define SAP_MAX 590099999L

/* Known Indian state/union-territory codes. District codes are not validated
 * against a table because the RTO district list changes over time and a
 * locally maintained copy cannot be exhaustive. Any two-digit district code
 * is accepted for a known state. */
static const char *const STATE_CODES[] = {
    "AN", "AP", "AR", "AS", "BR", "CH", "DN", "DD", "DL", "GA",
    "GJ", "HR", "HP", "JK", "JH", "KA", "KL", "MP", "MH", "MN",
    "ML", "MZ", "NL", "OD", "PB", "PY", "RJ", "SK", "TN", "TG",
    "TS", "UP", "UK", "WB",
};
#define STATE_CODE_COUNT \
    (sizeof STATE_CODES / sizeof STATE_CODES[0])

// Function Declarations
static int state_is_known(const char *state);
static int my_isupper(char c);
static int my_isdigit(char c);
static char my_toupper(char c);
static int my_isspace(char c);

// Functions
static int state_is_known(const char *state) {
    for (size_t i = 0; i < STATE_CODE_COUNT; i++) {
        if (STATE_CODES[i][0] == state[0] && STATE_CODES[i][1] == state[1]) {
            return 1;
        }
    }
    return 0;
}

static int my_isupper(char c) {
    return c >= 'A' && c <= 'Z';
}

static int my_isdigit(char c) {
    return c >= '0' && c <= '9';
}

static char my_toupper(char c) {
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

static int my_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\v' || c == '\f' || c == '\r';
}

int registration_is_valid(const char *text) {
    if (text == NULL) {
        return 0;
    }
    size_t len = strlen(text);
    if (len != REGISTRATION_LENGTH) {
        return 0;
    }
    /* Check pattern: [A-Z]{2}[0-9]{2}[A-Z]{2}[0-9]{4} */
    for (int i = 0; i < 2; i++) {
        if (!my_isupper(text[i])) {
            return 0;
        }
    }
    for (int i = 2; i < 4; i++) {
        if (!my_isdigit(text[i])) {
            return 0;
        }
    }
    for (int i = 4; i < 6; i++) {
        if (!my_isupper(text[i])) {
            return 0;
        }
    }
    for (int i = 6; i < 10; i++) {
        if (!my_isdigit(text[i])) {
            return 0;
        }
    }
    /* Validate state code against known list. District code is accepted
     * on faith (best-effort; RTO district tables change over time). */
    return state_is_known(&text[0]);
}

int registration_normalize(char *out, size_t size, const char *text) {
    if (out == NULL || text == NULL || size < 11) {
        return 0;
    }
    /* Trim leading whitespace */
    const char *src = text;
    while (*src && my_isspace(*src)) {
        src++;
    }
    /* Trim trailing whitespace: find end */
    size_t raw_len = strlen(src);
    const char *end = src + raw_len;
    while (end > src && my_isspace(*(end - 1))) {
        end--;
    }
    /* Copy and uppercase into a temporary buffer */
    char normalized[11]; /* exactly 10 chars + NUL */
    size_t i;
    for (i = 0; i < 10 && src + i < end; i++) {
        normalized[i] = my_toupper(src[i]);
    }
    normalized[10] = '\0';
    /* After trimming, the length must be exactly 10 and nothing may remain */
    if (i != 10 || src + i < end) {
        out[0] = '\0';
        return 0;
    }
    /* Validate the normalized result */
    if (!registration_is_valid(normalized)) {
        out[0] = '\0';
        return 0;
    }
    /* Copy into out, NUL-terminated, bounded by size */
    size_t copy_len = strlen(normalized) + 1; /* +1 for NUL */
    if (copy_len > size) {
        copy_len = size;
    }
    for (size_t j = 0; j < copy_len; j++) {
        out[j] = normalized[j];
    }
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
        if (!my_isdigit(text[i])) {
            return 0;
        }
    }
    /* Check range: 590000000 - 590099999 */
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
        if (!my_isdigit(text[i])) {
            return 0;
        }
    }
    return 1;
}
