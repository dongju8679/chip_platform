/* app_init.c - shared App initialization (chip-independent skeleton)
 *   The WEAK hooks are no-ops unless a chip-specific implementation exists, in
 *   which case that one wins.
 */
#include "app_init.h"

/* WEAK hooks to be overridden per chip (no-op by default) */
WEAK int app_init_early(void)  { return 0; }
WEAK int app_init_periph(void) { return 0; }
WEAK int app_init_user(void)   { return 0; }
WEAK int app_sleep_enter(void) { return 0; }

/* Combined initialization - order is guaranteed (early -> periph -> user) */
int app_init(void)
{
    int rc;
    if ((rc = app_init_early())  != 0) return rc;
    if ((rc = app_init_periph()) != 0) return rc;
    if ((rc = app_init_user())   != 0) return rc;
    return 0;
}

/* Enter low power (calls the chip-specific app_sleep_enter) */
int app_sleep(void)
{
    return app_sleep_enter();
}
