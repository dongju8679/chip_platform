/* app_intr.h - the shared App interrupt interface (chip independent)
 *   The chip-specific interrupt controller (PLIC/CLIC etc.) is abstracted by
 *   the BSP (hal).
 *   This header provides only the generic form of "handler registration".
 */
#ifndef _APP_INTR_H_
#define _APP_INTR_H_

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

/* Interrupt handler type (generic) */
typedef void (*app_irq_handler_t)(int irq_id, void *ctx);

/* Handler register/unregister - implemented on top of the BSP (hal), or
 * overridden per chip via WEAK */
int  app_irq_register(int irq_id, app_irq_handler_t handler, void *ctx);
int  app_irq_unregister(int irq_id);
void app_irq_enable(int irq_id);
void app_irq_disable(int irq_id);

/* Hook to register all user interrupts at once (implemented per chip) */
int app_intr_register_all(void);

#endif /* _APP_INTR_H_ */
