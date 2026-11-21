#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Definitions of the extern configuration values declared in config.h. */
const char *CONFIG_DATA_PATH = "data/parking.dat";
const char *CONFIG_REPORTS_PATH = "reports/";
int CONFIG_DEFAULT_HOURLY_RATE = 2;
int CONFIG_MAX_SESSION_HOURS = 24;
int CONFIG_SLOT_CAPACITY = 50;

Status config_init(void) {
    const char *env = getenv("PARKING_DATA_PATH");
    if (env != NULL && env[0] != '\0') {
        CONFIG_DATA_PATH = env;
    }
    return STATUS_OK;
}

const char *config_summary(void) {
    static char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "data_path=%s hourly_rate=%d max_session_h=%d slot_capacity=%d",
        CONFIG_DATA_PATH, CONFIG_DEFAULT_HOURLY_RATE,
        CONFIG_MAX_SESSION_HOURS, CONFIG_SLOT_CAPACITY);
    if (n < 0 || (size_t)n >= sizeof(buf)) {
        buf[sizeof(buf) - 1] = '\0';
    }
    return buf;
}
