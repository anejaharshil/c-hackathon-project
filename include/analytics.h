#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "common.h"
#include "system.h"

struct ParkingSystem; /* defined in system.h */

/* Number of currently active sessions. */
size_t analytics_active_sessions(const struct ParkingSystem *sys);

/* Number of occupied slots. */
size_t analytics_occupied_slots(const struct ParkingSystem *sys);

/* Total billable minor-units across all completed sessions. */
long analytics_total_revenue(const struct ParkingSystem *sys);

/* Number of completed sessions. */
size_t analytics_completed_sessions(const struct ParkingSystem *sys);

/* Total number of parking slots. */
size_t analytics_total_capacity(const struct ParkingSystem *sys);

/* Occupancy percentage (0-100). Returns 0 if no slots. */
int analytics_occupancy_percentage(const struct ParkingSystem *sys);

/* Print zone-wise slot utilisation to stdout. */
void analytics_zone_utilization(const struct ParkingSystem *sys);

/* Average completed parking duration in seconds. Returns 0 if none. */
double analytics_average_duration(const struct ParkingSystem *sys);

/* Print vehicle category counts (car/motorcycle/bicycle) to stdout. */
void analytics_vehicle_category_counts(const struct ParkingSystem *sys);

#endif /* ANALYTICS_H */
