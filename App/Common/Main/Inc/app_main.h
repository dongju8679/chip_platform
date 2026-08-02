/* app_main.h - the shared App main loop (chip-independent product entry point)
 *   NOTE: unlike the verification firmware, this runs forever with no exit (it
 *   is the product).
 */
#ifndef _APP_MAIN_H_
#define _APP_MAIN_H_

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

/* One iteration of the main loop (implemented per chip - events/tasks).
 * No-op by default. */
int app_loop_once(void);

/* The product main - initialize, then loop forever. main in start.S calls
 * this. */
int app_main(void);

#endif /* _APP_MAIN_H_ */
