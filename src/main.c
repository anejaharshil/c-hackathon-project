#include "config.h"
#include "system.h"

#include <stdio.h>

/* Sprint 0 stub entry point (Developer 3 owns main).
 * Demonstrates the skeleton links and configuration is accessible.
 * Application features are intentionally not implemented yet. */
int main(void) {
    if (config_init() != STATUS_OK) {
        return 1;
    }
    puts(config_summary());
    return 0;
}
