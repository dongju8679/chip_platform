#ifndef __PRINTF__
#define __PRINTF__

/* printf.h - shim that forwards to the standard stdio (v304-newlib)
 *
 * -- Today (USE_NEWLIB_STDIO=1, the default) -------------------------
 *   This header is a thin shim over <stdio.h>. printf/vprintf/puts/putchar are
 *   provided by newlib (rv32-multilib, rv32imac/ilp32).
 *   The output path is unchanged:
 *     printf() [newlib] -> _write(1, buf, len) [Platform/Common/Src/syscalls.c]
 *                       -> uart_write/uart_putc [Platform/Common/Src/uart.c]
 *                       -> SiFive UART0 txdata
 *   The v301 in-house implementation was designed from the start to funnel into
 *   the same lower interface as newlib (_write), so syscalls.c did not change
 *   by a single line.
 *
 * -- Why the file was kept -------------------------------------------
 *   Six files include this header (port_glue.c, log.h, FREERTOS_preempt.c,
 *   UART_printf.c, CA_measure.c, printf.c). To leave all of them untouched, the
 *   header stays and simply delegates its contents to <stdio.h>.
 *
 * -- Reverting (USE_NEWLIB_STDIO=0) ----------------------------------
 *   If you need to go back to a toolchain without newlib (e.g. the chipyard
 *   conda riscv64-unknown-elf-gcc, whose multilib list is just "." so there is
 *   no libc.a for rv32imac/ilp32), pass make ... USE_NEWLIB_STDIO=0. That makes
 *   Makefile.VERIF link Platform/Common/Src/printf.c (the 271-line in-house
 *   implementation) again, and this header uses the #else declarations below.
 *   Background: docs/newlib_transition.md
 */

#if !defined(USE_NEWLIB_STDIO) || USE_NEWLIB_STDIO

#include <stdio.h>
#include <stdarg.h>

#else  /* USE_NEWLIB_STDIO == 0 : the v301 in-house implementation */

/* Supported conversions: %c %s %d %i %u %x %X %o %p %% ,
 *            field width / zero padding / left alignment (%-8d, %08x),
 *            length modifiers l / ll */
#include <stdarg.h>

int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int puts(const char *s);
int putchar(int c);

#endif /* USE_NEWLIB_STDIO */

/* The syscall (Platform/Common/Src/syscalls.c). Its signature matches the
 * newlib contract, so both implementations funnel into this one function. */
int _write(int fd, const char *buf, int len);

#endif /* __PRINTF__ */
