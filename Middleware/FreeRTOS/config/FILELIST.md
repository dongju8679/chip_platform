# config/ - FreeRTOS configuration (written by us)

Status: **populated**. Rationale in chapter 3 of `chip_docs/verif/docs/FreeRTOS_port.md`.

## Files
  [x] FreeRTOSConfig.h   configuration tailored to TinyRocket/RV32RocketConfig

## Actual settings
  configUSE_PREEMPTION            1          preemptive scheduling
  configUSE_TIME_SLICING          1          round-robin among equal priorities (required by the preemption demo)
  configCPU_CLOCK_HZ              500000     NOT the CPU clock - see the caveat below
  configTICK_RATE_HZ              1000       tick frequency -> mtimecmp += 500
  configMTIME_BASE_ADDRESS        0x0200BFF8 CLINT mtime
  configMTIMECMP_BASE_ADDRESS     0x02004000 CLINT mtimecmp
  configMAX_PRIORITIES            5
  configMINIMAL_STACK_SIZE        128 words  (idle)
  configISR_STACK_SIZE_WORDS      256 words  dedicated trap stack (avoids editing link.ld)
  configTOTAL_HEAP_SIZE           10 KB      heap_4
  configSUPPORT_DYNAMIC_ALLOCATION 1

## The biggest pitfall: configCPU_CLOCK_HZ is not "the CPU clock"
The official RISC-V port.c derives the mtimecmp increment as
    uxTimerIncrementsForOneTick = configCPU_CLOCK_HZ / configTICK_RATE_HZ
so this value is used as the **mtime counter frequency**.
On this DUT the CPU runs at 500MHz while the CLINT mtime runs at 500kHz.
Putting 500000000 here would give mtimecmp += 500,000, making one tick take
1 second in mtime terms, and the scheduler would appear frozen.
-> configCPU_CLOCK_HZ = 500000 (the mtime frequency). If the real core frequency
  is needed, use the separately defined configCORE_CLOCK_HZ (500000000UL).

## Minimization (anything unused is 0)
  configUSE_MUTEXES / RECURSIVE_MUTEXES / COUNTING_SEMAPHORES  0
  configUSE_TIMERS / QUEUE_SETS / EVENT_GROUPS / STREAM_BUFFERS 0
  configUSE_IDLE_HOOK / TICK_HOOK                               0
  INCLUDE_vTaskDelete                                           0

## Conversely, deliberately enabled
  configCHECK_FOR_STACK_OVERFLOW  2   -> the hook prints "!STACK"
  configUSE_MALLOC_FAILED_HOOK    1   -> the hook prints "!HEAP"
  configASSERT                    defined -> prints "!ASSERT" and halts
  Reason: in a 60K DTIM environment the most common failures are stack overflow
        and heap exhaustion; with these off, the symptom is only "halts with no
        output", which makes the cause hard to find.

## Hooks for the control experiment
  CFG_USE_PREEMPTION / CFG_USE_TIME_SLICING can be overridden to 0 with -D.
  They are used to falsify whether the preemption demo really passes because of
  preemption (chip_docs/verif/docs/FreeRTOS_port.md 6.4 - disabling either one
  alone leaves the demo incomplete).

## Memory map
  - The heap is heap_4's ucHeap (in .bss), so no link.ld change was needed.
  - The ISR stack is likewise allocated in .bss via configISR_STACK_SIZE_WORDS,
    so there is no need to create a __freertos_irq_stack_top linker symbol.
  - Measured: image end 0x80006920, __stack_top 0x8000F000 -> about 35KB of headroom.
