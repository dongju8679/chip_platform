# kernel/ - the FreeRTOS upstream sources

Source: github.com/FreeRTOS/FreeRTOS-Kernel (official) - **V11.1.0**
Rule: upstream, unmodified. Do not edit. (Porting and configuration live in port/ and config/)
Status: **populated**. Details in `chip_docs/verif/docs/FreeRTOS_port.md`.

Version note: the request was for 10.6.x, but the V11.1.0 already present in this
repository was reused. Across the scope used here (tasks/list/heap_4 + the official
RISC-V port), neither the API nor the port structure changed between 10.6 and 11.1.
If it ever has to be rolled back, only this directory needs replacing - port/,
config/ and the test code can stay as they are.
(chip_docs/verif/docs/FreeRTOS_port.md 2.1)

## What goes into the build (RTOS_SRC in Makefile.VERIF)
  [x] tasks.c          task management (the core of the scheduler)
  [x] list.c           the internal data structure (a dependency of tasks)
  [x] portable/MemMang/heap_4.c   dynamic allocation (configTOTAL_HEAP_SIZE = 10KB)
        -> heap_4 was chosen over heap_1. The size difference is small, and
          freeing becomes necessary once queues/semaphores are added later.

## Present but not built (add to RTOS_SRC when needed)
  [x] queue.c            queues/semaphores/mutexes - unnecessary for the 2-task
                         preemption demo. To use them, enable them together with
                         configUSE_MUTEXES=1 and friends.
  [ ] timers.c           software timers (excluded because configUSE_TIMERS=0)
  [ ] event_groups.c / stream_buffer.c   (unused)

## Headers (include/)
  [x] FreeRTOS.h task.h list.h queue.h projdefs.h portable.h atomic.h ... all copied
      (FreeRTOSConfig.h is not here - it lives in config/ and is found via -I)

## Measured (RV32RocketConfig, rv32imac/ilp32, -O2)
  text 15,078 + data 20 + bss 11,816 = 26,914 B  /  DTIM 60KB
  Of the bss, 10KB is heap_4's ucHeap and 1KB is the ISR stack.
