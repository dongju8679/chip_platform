/* app_main.c - the shared App main (chip-independent product entry point)
 *
 *   The decisive difference from the verification firmware (VERIF/):
 *     verification: test(); exit(CODE_SUCCESS);   <- it ends (reporting to the simulator)
 *     product:      app_init(); while(1) {...}    <- it never ends (real operation)
 *
 *   Wire the BSP (start.S) _start -> main so that it calls app_main().
 */
#include "app_main.h"
#include "app_init.h"
#include "app_intr.h"

/* One main-loop iteration - per-chip event handling / task execution.
 * No-op (idle) by default. */
WEAK int app_loop_once(void) { return 0; }

/* Low-power idle hook (per-chip WFI etc.). No-op by default. */
WEAK void app_idle(void) { }

int app_main(void)
{
    /* 1) initialization (early -> periph -> user) */
    app_init();

    /* 2) interrupt registration (per-chip app_intr_register_all) */
    app_intr_register_all();

    /* 3) the product main loop - runs forever (this is the essence of the product) */
    for (;;) {
        app_loop_once();   /* handle events/tasks (per chip) */
        app_idle();        /* wait in low power when there is nothing to do (WFI etc.) */
    }

    /* unreachable (the product never terminates) */
    return 0;
}
