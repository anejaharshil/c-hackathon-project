#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

/* Shared configuration values.
 *
 * These are declared extern here and defined once in config.c. Headers must
 * never define non-static global variables, to avoid multiple-definition
 * errors when included in more than one translation unit.
 */

extern const char *CONFIG_DATA_PATH;
extern const char *CONFIG_REPORTS_PATH;
extern int CONFIG_DEFAULT_HOURLY_RATE;
extern int CONFIG_MAX_SESSION_HOURS;
extern int CONFIG_SLOT_CAPACITY;

/* Initialize configuration (load defaults / optional env overrides). */
Status config_init(void);

/* Return a human-readable summary of current configuration. */
const char *config_summary(void);

#endif /* CONFIG_H */
