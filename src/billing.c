#include "billing.h"
#include "config.h"
#include "system.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Billing policy (documented assumption, using existing config only):
 *
 * - Chargeable hours = ceiling(duration_seconds / 3600).
 * - Any positive stay charges at least 1 hour.
 * - Chargeable hours are capped at CONFIG_MAX_SESSION_HOURS.
 * - Cost = chargeable_hours * CONFIG_DEFAULT_HOURLY_RATE (minor units).
 *
 * All arithmetic is integer; no floating-point money is used.
 */

/* Return the chargeable hours for a duration given in seconds. */
static long chargeable_hours(long long seconds) {
    if (seconds <= 0) {
        return 0;
    }
    long hours = (long)((seconds + 3599LL) / 3600LL);
    if (hours < 1) {
        hours = 1;
    }
    if (hours > CONFIG_MAX_SESSION_HOURS) {
        hours = CONFIG_MAX_SESSION_HOURS;
    }
    return hours;
}

Status billing_compute(const ParkingSystem *sys, Id session_id, long *out_cost) {
    if (sys == NULL || out_cost == NULL) {
        return STATUS_INVALID_ARGUMENT;
    }

    size_t index = 0;
    if (session_find_by_id(sys, session_id, &index) != STATUS_OK) {
        return STATUS_NOT_FOUND;
    }

    const ParkingSession *s = session_get(sys, index);
    if (s == NULL || s->active || s->end_time == 0) {
        /* Only completed, non-active sessions are billable. */
        return STATUS_NOT_FOUND;
    }

    long long seconds = (long long)difftime(s->end_time, s->start_time);
    if (seconds < 0) {
        /* Negative duration: invalid data, reject. */
        return STATUS_INVALID_ARGUMENT;
    }

    long hours = chargeable_hours(seconds);
    long cost = hours * (long)CONFIG_DEFAULT_HOURLY_RATE;
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
        int n = snprintf(buf, bufsz, "session %ld: no billable session",
                         (long)session_id);
        if (n < 0) {
            n = 0;
        }
        if ((size_t)n >= bufsz) {
            n = (int)bufsz - 1;
        }
        buf[n] = '\0';
        return (int)strlen(buf);
    }

    size_t index = 0;
    session_find_by_id(sys, session_id, &index);
    const ParkingSession *s = session_get(sys, index);

    const Vehicle *v = NULL;
    const ParkingSlot *sl = NULL;
    size_t idx = 0;
    if (s != NULL &&
        vehicle_find_by_id(sys, s->vehicle_id, &idx) == STATUS_OK) {
        v = vehicle_get(sys, idx);
    }
    idx = 0;
    if (s != NULL &&
        slot_find_by_id(sys, s->slot_id, &idx) == STATUS_OK) {
        sl = slot_get(sys, idx);
    }

    char entry[32];
    char exit_[32];
    entry[0] = '\0';
    exit_[0] = '\0';
    if (s != NULL) {
        struct tm *tm = localtime(&s->start_time);
        if (tm != NULL) {
            strftime(entry, sizeof(entry), "%Y-%m-%d %H:%M", tm);
        }
        tm = localtime(&s->end_time);
        if (tm != NULL) {
            strftime(exit_, sizeof(exit_), "%Y-%m-%d %H:%M", tm);
        }
    }

    long seconds = (long)((s != NULL) ? difftime(s->end_time, s->start_time) : 0);
    long hours = chargeable_hours(seconds);
    const char *reg = (v != NULL) ? v->registration : "-";
    const char *owner = (v != NULL) ? v->owner : "-";
    long slot_id = (sl != NULL) ? (long)sl->id : (long)session_id;

    int n = snprintf(buf, bufsz,
                     "session %ld: reg=%s owner=%s slot=%ld entry=%s exit=%s "
                     "duration=%ldh rate=%d/hr total=%ld",
                     (long)session_id, reg, owner, slot_id, entry, exit_,
                     hours, CONFIG_DEFAULT_HOURLY_RATE, cost);
    if (n < 0) {
        n = 0;
    }
    if ((size_t)n >= bufsz) {
        n = (int)bufsz - 1;
    }
    buf[n] = '\0';
    return (int)strlen(buf);
}
