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

#endif /* ANALYTICS_H */
