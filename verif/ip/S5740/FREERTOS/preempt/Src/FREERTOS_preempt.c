/* FREERTOS_preempt.c - FreeRTOS 2-task preemptive scheduling verification (v301)
 *
 * -- What this proves -------------------------------------------------
 * "Two tasks print alternately" is not by itself evidence of preemption.
 * Under cooperative scheduling the output alternates just the same if the tasks
 * call taskYIELD()/vTaskDelay() themselves.
 *
 * So the two tasks in this test NEVER call a yield API even once.
 *   vTaskDelay / taskYIELD / queues / semaphores / mutexes - none of them.
 *   Each task is simply an infinite loop incrementing its own counter.
 *
 * Between such tasks there is exactly one path by which control can move:
 *   CLINT mtimer interrupt -> freertos_risc_v_trap_handler
 *     -> xTaskIncrementTick() -> (equal priority + configUSE_TIME_SLICING)
 *     -> vTaskSwitchContext() -> mret into the other task's context
 * That is, the tick interrupt forcibly took the CPU away = preemption.
 *
 * How it is observed: on every loop each task checks "was I the one running
 * previously?".
 *   If not -> a context switch just happened -> print one character of its own
 *   number.
 * So every single character of "1212121212" on stdout is one preemptive switch.
 *
 * -- Expected output ---------------------------------------------------
 *   R                     marker printed just before entering the scheduler
 *   1212121212            10 preemptive switches (strictly alternating)
 *   RTOSOK c1=.. c2=..    neither counter is zero = both really ran
 *
 * -- Caution: the UART is extremely slow ------------------------------
 * 115200 baud @ pbus 500MHz = about 43,400 cycles per character (measured at
 * about 16 s). Output is therefore emitted one character at a time. Within one
 * slice (1 ms = 500,000 cycles) a single 43,400-cycle output completes easily,
 * so exactly one character appears per switch.
 */

#include "type.h"
#include "util.h"
#include "uart.h"
#include "printf.h"

#include "FreeRTOS.h"
#include "task.h"

#include "FREERTOS_preempt.h"

/* port_glue.c */
extern void vPortInstallFreeRTOSTrapHandler( void );

/* rtdev_shim.c */
extern void exit( unsigned int code );

/* -- Shared state used for observation ---------------------- */
static volatile unsigned long ulRunCount[ 2 ];  /* loop count of each task */
static volatile int           iLastRunner = -1; /* id of the previously running task */
static volatile int           iSwitchCount;     /* observed preemptive switches */
static volatile int           iDone;            /* termination flag */

/*-----------------------------------------------------------*/

/* The body shared by both tasks. It never calls a yield API. */
static void prvSpinTask( void * pvParameters )
{
    const int iMe = ( int ) ( portPOINTER_SIZE_TYPE ) pvParameters;
    int iSwitched;
    int iSeq;

    for( ; ; )
    {
        ulRunCount[ iMe ]++;

        /* The test-and-update that makes me the "previous runner" must be
         * atomic. (Being preempted between the test and the update could count
         * the same switch twice.) */
        iSwitched = 0;
        iSeq      = 0;

        taskENTER_CRITICAL();
        {
            if( ( iLastRunner != iMe ) && ( iDone == 0 ) )
            {
                /* The very first entry (iLastRunner == -1) is "the scheduler
                 * picked the first task", not a preemption. Still print it so
                 * you can see who was picked first, but do not count it as a
                 * preemptive switch. */
                iSwitched = 1;

                if( iLastRunner >= 0 )
                {
                    iSeq = ++iSwitchCount;
                }

                iLastRunner = iMe;
            }
        }
        taskEXIT_CRITICAL();

        if( iSwitched != 0 )
        {
            /* 1 character = 1 preemptive switch. ('0' + 1) = '1', ('0' + 2) = '2' */
            uart_putc( ( char ) ( '1' + iMe ) );

            if( iSeq >= PREEMPT_SWITCH_TARGET )
            {
                iDone = 1;
            }
        }

        if( iDone != 0 )
        {
            /* The verdict is made here, not outside the scheduler (in a
             * higher-priority reporter task). "Both really ran" only holds if
             * neither counter is zero. */
            if( iMe == 0 )
            {
                int iPass;

                taskDISABLE_INTERRUPTS();

                /* The 3 preemption criteria (identical to
                 * chip_docs/verif/docs/FreeRTOS_port.md 5.3).
                 * They are also carried in the exit code, so the verdict works
                 * even without a UART (e.g. on spike). */
                iPass = ( ulRunCount[ 0 ] != 0 ) &&
                        ( ulRunCount[ 1 ] != 0 ) &&
                        ( iSwitchCount >= PREEMPT_SWITCH_TARGET );

                printf( "\n" PREEMPT_PASS_MARKER " c1=%lu c2=%lu s=%d p=%d\n",
                        ulRunCount[ 0 ], ulRunCount[ 1 ], iSwitchCount, iPass );

#ifndef NO_UART_FLUSH
                /* On a backend without a real UART (spike) the ip.txwm poll
                 * never finishes, so it is excluded via -DNO_UART_FLUSH. It is
                 * mandatory on RTL (chip_docs/verif/docs/UART_printf.md 3.5 -
                 * without the flush the last line is truncated). */
                uart_flush();
#endif
                exit( iPass ? CODE_SUCCESS : 2u );
            }

            /* Task 1 steps aside so task 0 can do the cleanup. */
            taskYIELD();
        }
    }
}

/*-----------------------------------------------------------*/

int main( void )
{
    BaseType_t xResult;

    /* txen resets to 0 -> it must be enabled first (chip_docs/verif/docs/UART_printf.md 3.1) */
    uart_init();
    uart_putc( 'R' );

    /* The two tasks MUST have the same priority.
     * With different priorities the higher one monopolizes the CPU (neither
     * yields) and the lower one never runs - that would not demonstrate
     * preemption.
     * Only equal priority + configUSE_TIME_SLICING=1 gives round-robin on every
     * tick. */
    xResult = xTaskCreate( prvSpinTask, "T1", PREEMPT_TASK_STACK_WORDS,
                           ( void * ) 0, tskIDLE_PRIORITY + 1, NULL );

    if( xResult == pdPASS )
    {
        xResult = xTaskCreate( prvSpinTask, "T2", PREEMPT_TASK_STACK_WORDS,
                               ( void * ) 1, tskIDLE_PRIORITY + 1, NULL );
    }

    if( xResult != pdPASS )
    {
        /* heap_4 exhausted. Suspect configTOTAL_HEAP_SIZE. */
        uart_puts( "\n!CREATE\n" );
        uart_flush();
        exit( 1 );
    }

    /* Swap the trap vector from baremetal (_trap_vector_entry) to FreeRTOS.
     *   See the header comment of port_glue.c for why (mtvec.S knows nothing
     *   about pxCurrentTCB). */
    vPortInstallFreeRTOSTrapHandler();

    vTaskStartScheduler();

    /* Reaching here means the scheduler never even started (usually heap exhaustion). */
    uart_puts( "\n!SCHED\n" );
    uart_flush();
    exit( 1 );

    return CODE_FAIL;
}
