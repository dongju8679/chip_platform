#ifndef __TIMER__
#define __TIMER__

/* timer.h - soft timer queue on top of CLINT mtime/mtimecmp (shared HAL)
 *
 * -- Provenance ------------------------------------------------------
 * Originally lived in verif/ip/S5740/CLINT/timer_interrupt/Inc/timer.h.
 * Because each test folder carried its own driver, any other test that wanted
 * a timer had to copy the file again. It was therefore promoted to
 * Platform/Common. The contents are the rt-dev original (not a single
 * declaration was changed).
 *
 * -- Why this API ----------------------------------------------------
 * The rt-dev timer multiplexes "one hardware timer (mtimecmp) through a
 * software queue". There is only one mtimecmp per hart, so registering several
 * delayed callbacks means loading only the earliest expiry into mtimecmp,
 * keeping the rest in the queue, and rearming from the interrupt.
 * execute_timer() performs that rearming.
 * Keeping this structure intact is what makes copy-pasted rt-dev tests behave
 * identically.
 *
 * -- Dependencies ----------------------------------------------------
 *   - CLINT_TIME_LO / CLINT_TIMECMP_LO   : Platform/Chipset/S5740/Inc/S5740.h
 *   - pISR_BASE.time_int_handler         : Platform/Common/Inc/interrupt.h
 *   - __wrap_memset()                    : Platform/Common/Src/util.c
 *   So this header must be used after including interrupt.h, or with
 *   -include S5740.h in effect (Makefile.VERIF already adds it for
 *   TRACK=baremetal).
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 * Yes. RV32RocketConfig has a CLINT (0x02000000) and mtime/mtimecmp really
 * run. verif/ip/S5740/CLINT/timer_interrupt PASSes with this driver.
 *
 * -- CLOCK_TIME caveat -----------------------------------------------
 * The delay argument of register_timer() is documented as "ns", but the actual
 * conversion is
 *   exe_time += delay * CLOCK_TIME / 1024
 * CLOCK_TIME=335 is a constant derived from the rt-dev silicon RTC and differs
 * from the chipyard timebase (500 kHz). In other words, absolute times do not
 * match. Existing tests only check "does the interrupt fire", so the value was
 * left as is. Override it with -DCLOCK_TIME=... if absolute time is needed.
 */

#define TIMER_QUEUE_MAX (10)

#define TIME (*(volatile uint64_t *)CLINT_TIME_LO)
#define TIMECMP (*(volatile uint64_t *)CLINT_TIMECMP_LO)
#ifndef CLOCK_TIME
#define CLOCK_TIME (335)
#endif

typedef struct _TIMER{
    int (*handler)();
    int repeat_ns;
    uint64_t exe_time;
}TIMER;

extern TIMER pTIMER_QUEUE[TIMER_QUEUE_MAX];

/* Push mtimecmp to -1, register execute_timer as the timer interrupt handler,
 * and enable mie.MTIE. The caller must enable global interrupts (mstatus.MIE)
 * separately. */
void init_timeint_handler(void);

/* Register one callback in the queue. repeat_ns>0 repeats periodically, 0 is
 * one-shot. Returns the queue index (which can be passed to clear_timer), or
 * TIMER_COUNT if the queue is full. */
int register_timer(int (*)(), int, int);

/* Timer interrupt handler. Invokes every expired callback and rearms mtimecmp. */
int execute_timer();

/* Cancel one idx. idx==-1 clears the entire queue. */
int clear_timer(int);

#endif
