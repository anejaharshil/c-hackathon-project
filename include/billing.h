#ifndef BILLING_H
#define BILLING_H

#include "common.h"
#include "session.h"

struct ParkingSystem; /* forward declaration; defined in system.h */

/* Compute the bill for a completed session using the configured hourly rate.
 * Returns the cost in currency minor units (e.g. cents). Sets out_cost.
 * Returns STATUS_NOT_FOUND if the session is not complete. */
Status billing_compute(const struct ParkingSystem *sys, Id session_id,
                       long *out_cost);

/* Generate a text bill line for a session. Writes at most bufsz bytes. */
int billing_format(const struct ParkingSystem *sys, Id session_id,
                   char *buf, size_t bufsz);

#endif /* BILLING_H */
