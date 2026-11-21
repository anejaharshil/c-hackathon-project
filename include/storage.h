#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"
#include "system.h"

struct ParkingSystem; /* defined in system.h */

/* Persist the ParkingSystem to a binary .dat file. Returns STATUS_IO_ERROR on
 * failure. Raw pointers are never persisted; only stable IDs and record data. */
Status storage_save(const struct ParkingSystem *sys, const char *path);

/* Load a ParkingSystem from a binary .dat file, replacing current contents.
 * Returns STATUS_IO_ERROR on failure. */
Status storage_load(struct ParkingSystem *sys, const char *path);

#endif /* STORAGE_H */
