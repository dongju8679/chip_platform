#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* FreeRTOSConfig.h - TinyRocket / chipyard RV32RocketConfig (v301)
 *
 * Target: rv32imac_zicsr_zifencei / ilp32, DTIM 60K @ 0x80000000
 * Kernel: FreeRTOS-Kernel V11.1.0 (Middleware/FreeRTOS/kernel, unmodified)
 * Port:   the official GCC/RISC-V port (Middleware/FreeRTOS/port/RISC-V,
 *         unmodified)
 *
 * IMPORTANT NOTE ABOUT configCPU_CLOCK_HZ
 * The official RISC-V port (port.c) computes the tick period like this:
 *
 *     uxTimerIncrementsForOneTick = configCPU_CLOCK_HZ / configTICK_RATE_HZ
 *     mtimecmp += uxTimerIncrementsForOneTick
 *
 * In other words configCPU_CLOCK_HZ is interpreted not as "the CPU clock" but
 * as the frequency at which the mtime counter advances. On this DUT the two
 * differ:
 *     CPU (core)           = 500 MHz
 *     CLINT mtime (RTC)    = 500 kHz   (DTS timebase-frequency)
 * Putting 500000000 here would give
 *     500000000 / 1000 = 500,000 -> mtimecmp += 500000 -> one tick takes
 *     1 second (in mtime terms)
 * and the scheduler would appear to be effectively frozen.
 *
 * So configCPU_CLOCK_HZ is set to 500000 (the mtime frequency), which gives
 *     500000 / 1000 = 500 -> mtimecmp += 500 -> a 1 kHz tick (as required)
 * Use configCORE_CLOCK_HZ below if the real core frequency is needed.
 */

/* -- Clocks ---------------------------------------------- */
#define configCORE_CLOCK_HZ                     500000000UL /* for reference: the real core clock */
#define configMTIME_FREQ_HZ                     500000UL    /* CLINT mtime(RTC) frequency */

/* The value port.c uses for the tick computation = the mtime frequency (see the comment above) */
#define configCPU_CLOCK_HZ                      configMTIME_FREQ_HZ

/* The required specification is 1000 Hz. However verilator measures at about
 * 2,700 cycles/s, so one 1 kHz tick is 500,000 CPU cycles = about 3 minutes of
 * wall time. Raise it (e.g. -DFREERTOS_TICK_RATE_HZ=50000) only for a quick
 * check of the port itself; final verification uses the default of 1000. */
#ifndef FREERTOS_TICK_RATE_HZ
#define FREERTOS_TICK_RATE_HZ                   1000
#endif
#define configTICK_RATE_HZ                      ( ( TickType_t ) FREERTOS_TICK_RATE_HZ )
/* -> uxTimerIncrementsForOneTick = 500000/1000 = 500 (at the default) */

/* -- CLINT (chipyard rocket-chip) -------------------------- */
#define configMTIME_BASE_ADDRESS                ( 0x0200BFF8UL )
#define configMTIMECMP_BASE_ADDRESS             ( 0x02004000UL )

/* -- Scheduling ------------------------------------------ */
/* Left overridable to 0 via -D purely for the control experiment.
 * (To falsify whether the preemption demo really passes because of preemption -
 *  docs 6.4) */
#ifndef CFG_USE_PREEMPTION
#define CFG_USE_PREEMPTION                      1
#endif
#ifndef CFG_USE_TIME_SLICING
#define CFG_USE_TIME_SLICING                    1
#endif
#define configUSE_PREEMPTION                    CFG_USE_PREEMPTION
#define configUSE_TIME_SLICING                  CFG_USE_TIME_SLICING
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configMAX_PRIORITIES                    5
#define configIDLE_SHOULD_YIELD                 1

/* -- Memory (kernel + stacks + heap must all fit in the 60K DTIM) -- */
#define configMINIMAL_STACK_SIZE                ( ( configSTACK_DEPTH_TYPE ) 128 ) /* 128 word = 512B (idle) */
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 10 * 1024 ) )       /* heap_4 */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Switch to a dedicated ISR stack on trap entry (portASM.S uses xISRStackTop).
 * Defining this macro makes port.c allocate the stack in .bss, so link.ld needs
 * no changes. */
#define configISR_STACK_SIZE_WORDS              256 /* 1KB */

/* -- Feature minimization (to save DTIM) ------------------ */
#define configUSE_MUTEXES                       0
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_TIMERS                        0
#define configUSE_QUEUE_SETS                    0
#define configUSE_EVENT_GROUPS                  0
#define configUSE_STREAM_BUFFERS                0
#define configUSE_CO_ROUTINES                   0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_TRACE_FACILITY                0
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configUSE_NEWLIB_REENTRANT              0
#define configQUEUE_REGISTRY_SIZE               0
#define configRECORD_STACK_HIGH_ADDRESS         0

/* -- Types ------------------------------------------------ */
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configSTACK_DEPTH_TYPE                  uint32_t
#define configMAX_TASK_NAME_LEN                 8

/* -- assert: disable interrupts and halt (the PC pins the location in simulation) -- */
#define configASSERT_DEFINED                    1
extern void vAssertCalled( const char * pcFile, unsigned long ulLine );
#define configASSERT( x )                                       \
    if( ( x ) == 0 )                                            \
    {                                                           \
        vAssertCalled( __FILE__, __LINE__ );                    \
    }

/* -- Which APIs to include -------------------------------- */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xSemaphoreGetMutexHolder        0

#endif /* FREERTOS_CONFIG_H */
