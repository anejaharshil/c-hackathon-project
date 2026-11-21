#include "analytics.h"
#include "billing.h"
#include "system.h"

#include <stdio.h>
#include <time.h>

static const char *zone_name(ZoneType z);

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

size_t analytics_total_capacity(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0;
    }
    return sys->slot_count;
}

int analytics_occupancy_percentage(const ParkingSystem *sys) {
    if (sys == NULL || sys->slot_count == 0) {
        return 0;
    }
    size_t occupied = analytics_occupied_slots(sys);
    return (int)((occupied * 100) / sys->slot_count);
}

static const char *zone_name(ZoneType z) {
    switch (z) {
        case ZONE_A:        return "Zone A";
        case ZONE_B:        return "Zone B";
        case ZONE_C:        return "Zone C";
        case ZONE_ACCESSIBLE: return "Zone Accessible";
        case ZONE_VISITOR:  return "Zone Visitor";
        default:            return "Unknown";
    }
}

void analytics_zone_utilization(const ParkingSystem *sys) {
    if (sys == NULL) {
        return;
    }
    ZoneType zones[5] = { ZONE_A, ZONE_B, ZONE_C, ZONE_ACCESSIBLE, ZONE_VISITOR };
    for (int i = 0; i < 5; i++) {
        size_t ztotal = 0, zocc = 0;
        for (size_t j = 0; j < sys->slot_count; j++) {
            const ParkingSlot *s = &sys->slots[j];
            if (s->zone != zones[i]) continue;
            ztotal++;
            if (!s->is_available) zocc++;
        }
        if (ztotal > 0) {
            int pct = (int)((zocc * 100) / ztotal);
            printf("  %s: %zu/%zu occupied (%d%%)\n",
                   zone_name(zones[i]), zocc, ztotal, pct);
        }
    }
}

double analytics_average_duration(const ParkingSystem *sys) {
    if (sys == NULL) {
        return 0.0;
    }
    long long total_seconds = 0;
    size_t count = 0;
    for (size_t i = 0; i < sys->session_count; i++) {
        const ParkingSession *s = &sys->sessions[i];
        if (!s->active && s->end_time != 0 && s->end_time >= s->start_time) {
            total_seconds += (long long)(s->end_time - s->start_time);
            count++;
        }
    }
    if (count == 0) {
        return 0.0;
    }
    return (double)total_seconds / (double)count;
}

void analytics_vehicle_category_counts(const ParkingSystem *sys) {
    if (sys == NULL) {
        return;
    }
    size_t cars = 0, motorcycles = 0, bicycles = 0;
    for (size_t i = 0; i < sys->vehicle_count; i++) {
        const Vehicle *v = &sys->vehicles[i];
        switch (v->type) {
            case VEHICLE_CAR:       cars++; break;
            case VEHICLE_MOTORCYCLE: motorcycles++; break;
            case VEHICLE_BICYCLE:   bicycles++; break;
            default: break;
        }
    }
    printf("  Car:       %zu\n", cars);
    printf("  Motorcycle:%zu\n", motorcycles);
    printf("  Bicycle:   %zu\n", bicycles);
}
