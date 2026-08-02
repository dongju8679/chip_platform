#ifndef __FREERTOS_PREEMPT__
#define __FREERTOS_PREEMPT__

/* Number of preemptive switches to observe. 1 switch = 1 tick = 1 ms
 * (500k CPU cycles). verilator measures at about 2,700 cycles/s, so one tick
 * takes about 3 minutes.
 * -> 10 switches is about 30 minutes. Raising it grows simulation time
 *    linearly. */
#ifndef PREEMPT_SWITCH_TARGET
#define PREEMPT_SWITCH_TARGET 10
#endif

/* Task stack (in words). 128 words = 512 B. Traps use a dedicated ISR stack
 * (configISR_STACK_SIZE_WORDS), so the task stack needs no ISR headroom. */
#ifndef PREEMPT_TASK_STACK_WORDS
#define PREEMPT_TASK_STACK_WORDS 192
#endif

#define PREEMPT_PASS_MARKER "RTOSOK"

#endif /* __FREERTOS_PREEMPT__ */
