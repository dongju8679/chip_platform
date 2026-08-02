/* app_init.h - the shared App initialization interface (chip independent)
 *   Chip-specific implementations override the WEAK functions. common provides
 *   only empty default implementations.
 */
#ifndef _APP_INIT_H_
#define _APP_INIT_H_

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

/* User (chip) initialization hooks - implemented per chip. The common default
 * is a no-op. */
int app_init_early(void);    /* minimal init such as clocks and power */
int app_init_periph(void);   /* peripheral initialization */
int app_init_user(void);     /* user feature initialization */

/* The combined initialization called from the system entry point */
int app_init(void);

/* Enter low power */
int app_sleep(void);

#endif /* _APP_INIT_H_ */
