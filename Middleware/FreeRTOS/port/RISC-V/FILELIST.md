# port/RISC-V/ - the porting layer

Base: the official FreeRTOS RISC-V port (GCC) + TinyRocket customization
Role: connects FreeRTOS to Platform (our hardware foundation)
Status: **populated**. Detailed rationale in `chip_docs/verif/docs/FreeRTOS_port.md`.

## Files
  [x] port.c        FreeRTOS **upstream, unmodified** (do not edit)
  [x] portASM.S     FreeRTOS **upstream, unmodified** - trap entry/return, context save/restore
  [x] portmacro.h   FreeRTOS **upstream, unmodified**
  [x] portContext.h FreeRTOS **upstream, unmodified** (included by portASM.S)
  [x] chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions/
        freertos_risc_v_chip_specific_extensions.h   upstream, unmodified
        (portasmHAS_MTIME=1, portasmHAS_SIFIVE_CLINT=1, no extra registers)
  [x] port_glue.c   the only file we wrote - see below

## What port_glue.c does (= the entirety of the actual porting work)
  1. Trap vector switch  vPortInstallFreeRTOSTrapHandler()
                      mtvec: _trap_vector_entry -> freertos_risc_v_trap_handler
                      (mtvec.S knows nothing about pxCurrentTCB, so preemption
                       is impossible there. Rather than modifying mtvec.S, only
                       the vector is swapped just before the scheduler starts
                       -> the existing IP verification path stays intact)
                      To revert: vPortRestoreBaremetalTrapHandler()
  2. freestanding fill-ins  memset/memcpy/memcmp/strlen
                      (-nostdlib, plus this toolchain lacks rv32 newlib/libgcc.
                       util.c's __wrap_memset is not picked up without --wrap
                       and returns failure outside the ITIM/DTIM range, making
                       it unsuitable for the kernel)
  3. FreeRTOS hooks   vAssertCalled / MallocFailed / StackOverflow
                      -> print a short marker (!ASSERT/!HEAP/!STACK) and halt
                      (the UART costs about 16 s per character, so even error
                       messages must be short)

## Platform integration (the 3 porting elements) - how each was solved
  1. Timer tick   -> **handled entirely by the official port.c.** Given only
                   configMTIME_BASE_ADDRESS    = 0x0200BFF8
                   configMTIMECMP_BASE_ADDRESS = 0x02004000
                   in FreeRTOSConfig.h, vPortSetupTimerInterrupt() and
                   `csrs mie,0x880` happen automatically -> no separate timer
                   porting code was needed.
                   mtimecmp increment = configCPU_CLOCK_HZ / configTICK_RATE_HZ = 500
                   NOTE: configCPU_CLOCK_HZ here is not the CPU (500MHz) but the
                     mtime frequency (500kHz) - see chip_docs/verif/docs/FreeRTOS_port.md 3.1
  2. Context switch -> **handled by the official portASM.S** (saves x1,x5~x31
                   + mepc + mstatus + xCriticalNesting on the task stack, then
                   switches sp to xISRStackTop)
  3. Trap entry   -> the mtvec swap in port_glue.c (item 1 above)

## Caveats
  - port.c / portASM.S / portmacro.h / portContext.h are **upstream. Do not edit.**
    Customization belongs in port_glue.c or config/FreeRTOSConfig.h.
  - The CLINT addresses match CLINT_TIME_LO / CLINT_TIMECMP_LO in
    Platform/Chipset/S5740/Inc/S5740.h (the same as the existing timer verification).
  - To use PLIC external interrupts alongside this, implement
    freertos_risc_v_application_interrupt_handler, which portASM.S leaves open as
    a weak symbol, and forward to the existing pISR_BASE.ext_int_handler[] dispatch.
