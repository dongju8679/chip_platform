/* timer.c - CLINT soft timer queue (shared HAL)
 *
 * Provenance: promoted verbatim from
 *       verif/ip/S5740/CLINT/timer_interrupt/Src/timer.c. The body is
 *       byte-identical to the rt-dev original (only this comment block was
 *       added). See Platform/Common/Inc/timer.h for the API description and
 *       design rationale.
 *
 * Override: if the test folder (verif/.../Src) contains a timer.c of the same
 *       name, Makefile.VERIF drops this file from the link and uses the test's
 *       copy.
 *       (See the "rt-dev copy-paste policy" in
 *        chip_docs/verif/docs/baremetal.md)
 */
#include "interrupt.h"
#include "timer.h"

unsigned int TIMER_COUNT;
TIMER pTIMER_QUEUE[TIMER_QUEUE_MAX];

void init_timeint_handler(void)
{
    TIMECMP = -1;    
    pISR_BASE.time_int_handler = execute_timer;
    SET_INTERRUPT_ENABLE(MIP_MTIP);
}

int register_timer(int (*handler)(), int delay_ns, int repeat_ns)
{
    if (TIMER_COUNT >= TIMER_QUEUE_MAX)
        return TIMER_COUNT;
    volatile uint64_t exe_time = TIME;

    exe_time += (uint64_t)delay_ns * CLOCK_TIME / 1024;
    if (exe_time < TIMECMP) {
        TIMECMP = exe_time;
    }
    unsigned int orig_mstatus = read_csr(mstatus);
    GLOBAL_INTERRUPT_DISABLE();
    int idx = 0;
    for (idx = 0; idx < TIMER_QUEUE_MAX; idx++) {
        if (pTIMER_QUEUE[idx].handler == 0)
            break;
    }
    pTIMER_QUEUE[idx].exe_time = exe_time;
    pTIMER_QUEUE[idx].handler = handler;
    pTIMER_QUEUE[idx].repeat_ns = repeat_ns;

    TIMER_COUNT++;
    write_csr(mstatus, orig_mstatus);
    return idx;
}

int execute_timer()
{
    volatile uint64_t time = TIME;
    volatile uint64_t min_time = -1;

    for (int i  = 0; i < TIMER_QUEUE_MAX; i++) {
        if (pTIMER_QUEUE[i].handler == 0)
            continue;
        if (pTIMER_QUEUE[i].exe_time < time)
        {
            pTIMER_QUEUE[i].handler(0);
            if (pTIMER_QUEUE[i].repeat_ns > 0){
                pTIMER_QUEUE[i].exe_time = time + (pTIMER_QUEUE[i].repeat_ns * CLOCK_TIME / 1024);

                if (pTIMER_QUEUE[i].exe_time > min_time)
                    min_time = pTIMER_QUEUE[i].exe_time;
            }
            else {
                unsigned int orig_mstatus = read_csr(mstatus);
                GLOBAL_INTERRUPT_DISABLE();
                pTIMER_QUEUE[i].handler = 0;
                TIMER_COUNT--;
                write_csr(mstatus, orig_mstatus);
            }
        }
        else if(pTIMER_QUEUE[i].exe_time < min_time)
        {
            min_time = pTIMER_QUEUE[i].exe_time;
        }
    }
    TIMECMP = min_time;
    return 0;
}

int clear_timer(int idx)
{
    if (idx == -1)
    {
        __wrap_memset((void*)pTIMER_QUEUE, 0, sizeof(TIMER)*TIMER_QUEUE_MAX);
        TIMER_COUNT = 0;
        TIMECMP = -1;
        return 0;
    }

    if (pTIMER_QUEUE[idx].handler == 0)
        return 0;

    pTIMER_QUEUE[idx].handler = 0;
    volatile uint64_t min_time = -1;

    for (int i  = 0; i < TIMER_QUEUE_MAX; i++) {
        if (pTIMER_QUEUE[i].handler == 0)
            continue;
        if (pTIMER_QUEUE[i].exe_time < min_time)
        {
            min_time = pTIMER_QUEUE[i].exe_time;
        }
    }
    TIMECMP = min_time;
    TIMER_COUNT--;
    return 0;
}
