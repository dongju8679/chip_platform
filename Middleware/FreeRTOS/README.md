# Middleware/FreeRTOS - the RTOS layer

Why it sits here: the RTOS is the layer applications (App) run on top of, so it
belongs in Middleware between the application and the foundation, not under App.
Status: **working**. Populated with FreeRTOS-Kernel **V11.1.0** plus the official
      RISC-V port, and 2-task preemptive scheduling has been verified on chipyard
      RV32RocketConfig.
Detailed record: **`chip_docs/verif/docs/FreeRTOS_port.md`**

## Structure
```
Middleware/FreeRTOS/
|-- kernel/        FreeRTOS V11.1.0 upstream (do not modify)
|   |-- tasks.c list.c queue.c
|   |-- portable/MemMang/heap_4.c
|   `-- include/   FreeRTOS headers
|-- port/RISC-V/   the official RISC-V port (upstream) + port_glue.c (written by us)
`-- config/        FreeRTOSConfig.h (written by us)
```

## Build
```bash
make -f Implementation/Makefile.VERIF src_dir=. F=FREERTOS SF=preempt \
     TRACK=baremetal USE_FREERTOS=1 OPT=-O2 EXTRA_CFLAGS=-DPRELOAD
```
- This directory is only pulled into the build when `USE_FREERTOS=1`. With the
  default of 0, the compile command lines of existing cases do not change by a
  single character.
- `TRACK=baremetal` is required (it reuses the crt.S boot and the uart/printf
  stdout path).
- Keep `OPT=-O2`. `-Os` turns 64-bit shifts into libgcc calls, and this
  toolchain has no rv32 libgcc (`__ashldi3`/`__lshrdi3` undefined).

Verification case: `verif/ip/S5740/FREERTOS/preempt/` (`run.sh` builds, runs and judges)

## Minimal configuration (what is actually used)
tasks.c + list.c + heap_4.c + port.c + portASM.S + port_glue.c + FreeRTOSConfig.h
-> text 15KB + bss 12KB = about 27KB / DTIM 60KB (about 35KB of headroom)

## Platform integration points (the 3 porting elements) - how each was solved
- **Timer tick**: handled entirely by the official port.c. Given only
  `configMTIME_BASE_ADDRESS`(0x0200BFF8) / `configMTIMECMP_BASE_ADDRESS`(0x02004000)
  in FreeRTOSConfig.h, `vPortSetupTimerInterrupt()` and `csrs mie,0x880` happen
  automatically.
  -> no separate timer porting code was needed.
  NOTE: `configCPU_CLOCK_HZ` must be given the **mtime frequency (500kHz)**, not
    the CPU frequency (500MHz) (chip_docs/verif/docs/FreeRTOS_port.md 3.1 - the
    biggest pitfall).
- **Context switch**: the official portASM.S saves and restores on the task stack.
- **Trap entry**: `vPortInstallFreeRTOSTrapHandler()` in `port_glue.c` swaps
  `mtvec` from `_trap_vector_entry` to `freertos_risc_v_trap_handler` just before
  the scheduler starts.
  **mtvec.S was not modified** - `mtvec.S` assumes "return into the context that
  was entered" and knows nothing about `pxCurrentTCB`, so preemption cannot be
  wedged into it.
  The existing IP verification path keeps using `_trap_vector_entry`.

## App integration point (not done yet)
- Replace the for(;;) in App/Common/Main/app_main.c with vTaskStartScheduler()
- For now the scheduler is only started inside the verification case
  `verif/ip/S5740/FREERTOS/preempt`.

## Caveats
- The official files under `kernel/` and `port/RISC-V/` (port.c, portASM.S,
  portmacro.h, portContext.h) are **upstream and unmodified. Do not edit them.**
  Customization belongs in `port_glue.c` or `config/FreeRTOSConfig.h`.
- The UART costs about 43,400 cycles per character (16-46 s measured in
  simulation). Keep task output down to one character at a time.
