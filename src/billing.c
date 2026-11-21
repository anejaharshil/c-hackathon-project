#include "billing.h"
#include "config.h"
#include "system.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

Status billing_compute(const ParkingSystem *sys, Id session_id, long *out_cost) {
    if (sys == NULL || out_cost == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }
    size_t index = 0;
    if (session_find_by_id(sys, session_id, &index) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }
    const ParkingSession *s = session_get(sys, index);
    if (s == NULL || !s->active || s->end_time == 0) {
        return STATUS_NOT_FOUND;
    }

    double hours = difftime(s->end_time, s->start_time) / 3600.0;
    if (hours < 0.0) {
        hours = 0.0;
    }
    long cost = (long)((hours * CONFIG_DEFAULT_HOURLY_RATE) + 0.5);
    *out_cost = cost;
    return STATUS_OK;
}

int billing_format(const ParkingSystem *sys, Id session_id, char *buf, size_t bufsz) {
    if (buf == NULL || bufsz == 0) {
        return 0;
    }
    buf[0] = '\0';
    if (sys == NULL) {
        return 0;
    }
    long cost = 0;
    if (billing_compute(sys, session_id, &cost) != STATUS_OK) {
        int n = snprintf(buf, bufsz, "session %lld: no billable session", (long long)session_id);
        if (n < 0 || (size_t)n >= bufsz) {
            buf[bufsz - 1] = '\0';
        }
        return (int)strlen(buf);
    }
    int n = snprintf(buf, bufsz, "session %lld: cost %ld minor units",
                     (long long)session_id, cost);
    if (n < 0 || (size_t)n >= bufsz) {
        buf[bufsz - 1] = '\0';
    }
    return (int)strlen(buf);
}
