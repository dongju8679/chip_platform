/* app_intr.c - the shared App interrupt skeleton (chip independent)
 *   A simple handler table plus the integration point with the BSP hal.
 *   Extended or overridden per chip.
 */
#include "app_intr.h"

#ifndef APP_IRQ_MAX
#define APP_IRQ_MAX 64          /* adjust per chip via -D */
#endif

/* Generic handler table (chip independent) */
static struct {
    app_irq_handler_t fn;
    void *ctx;
} g_irq_table[APP_IRQ_MAX];

int app_irq_register(int irq_id, app_irq_handler_t handler, void *ctx)
{
    if (irq_id < 0 || irq_id >= APP_IRQ_MAX) return -1;
    g_irq_table[irq_id].fn  = handler;
    g_irq_table[irq_id].ctx = ctx;
    return 0;
}

int app_irq_unregister(int irq_id)
{
    if (irq_id < 0 || irq_id >= APP_IRQ_MAX) return -1;
    g_irq_table[irq_id].fn  = 0;
    g_irq_table[irq_id].ctx = 0;
    return 0;
}

/* Shared dispatcher - called by the trap handler (BSP) with the irq_id */
void app_irq_dispatch(int irq_id)
{
    if (irq_id < 0 || irq_id >= APP_IRQ_MAX) return;
    if (g_irq_table[irq_id].fn)
        g_irq_table[irq_id].fn(irq_id, g_irq_table[irq_id].ctx);
}

/* enable/disable are wired to the BSP (hal) PLIC/CLIC control - WEAK per chip */
WEAK void app_irq_enable(int irq_id)  { (void)irq_id; }
WEAK void app_irq_disable(int irq_id) { (void)irq_id; }

/* Register all user interrupts at once - implemented per chip. No-op by default. */
WEAK int app_intr_register_all(void) { return 0; }
