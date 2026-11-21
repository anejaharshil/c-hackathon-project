#include "analytics.h"
#include "billing.h"
#include "system.h"

size_t analytics_active_sessions(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    size_t n = 0;
    for (size_t i = 0; i < sys->session_count; i++) {
        if (sys->sessions[i].active) {
            n++;
        }
    }
    return n;
}

size_t analytics_occupied_slots(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    size_t n = 0;
    for (size_t i = 0; i < sys->slot_count; i++) {
        if (!sys->slots[i].is_available) {
            n++;
        }
    }
    return n;
}

long analytics_total_revenue(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    long total = 0;
    for (size_t i = 0; i < sys->session_count; i++) {
        const ParkingSession *s = &sys->sessions[i];
        if (!s->active && s->end_time != 0) {
            long cost = 0;
            if (billing_compute(sys, s->id, &cost) == STATUS_OK) {
                total += cost;
            }
        }
    }
    return total;
}

size_t analytics_completed_sessions(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    size_t n = 0;
    for (size_t i = 0; i < sys->session_count; i++) {
        if (!sys->sessions[i].active) {
            n++;
        }
    }
    return n;
}
