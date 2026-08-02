#ifndef __BENCH_STDIO__
#define __BENCH_STDIO__

/* bench_stdio.h - benchmark-only stdout (via HTIF), additive only
 *
 * Why Platform/Common/Src/printf.c cannot be used as is
 *   printf.c sends output down _write(1,...) -> syscalls.c -> uart_write().
 *   UART0 at 115200 baud @ 500MHz pbus costs about 43,000 cycles per character,
 *   and verilator runs a few thousand cycles/s, so one character takes tens of
 *   seconds of real time.
 *   The CoreMark report alone is over 700 characters, so over the UART printing
 *   would take far longer than the benchmark. On top of that, those UART cycles
 *   mix straight into mcycle and contaminate the measurement itself.
 *
 *   So printf.c is swapped for this file only in benchmark builds (USE_BENCH).
 *   The formatting logic is the same family as printf.c (integer only); only
 *   the sink is HTIF.
 *   Existing IP verification / FreeRTOS / UART_printf builds (USE_BENCH=0) are
 *   completely unaffected.
 *
 * No floating point support (no %f)
 *   rv32imac has no F extension, and this toolchain has no rv32 libgcc, so the
 *   soft-float helpers (__adddf3 etc.) cannot be linked. Hence integer only.
 *   -> CoreMark uses HAS_FLOAT=0 and Dhrystone is integer-only anyway, so
 *      neither is affected.
 */

#include <stdarg.h>

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int puts(const char *s);
int putchar(int c);

/* The name required by the CoreMark barebones port */
int ee_printf(const char *fmt, ...);
/* The name required by the riscv-tests dhrystone (really prints, regardless of DEBUG) */
int debug_printf(const char *fmt, ...);

#endif /* __BENCH_STDIO__ */
