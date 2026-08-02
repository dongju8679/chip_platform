#ifndef __LOG__
#define __LOG__

/* log.h - logging macros (printf-based, header-only)
 *
 * -- Why there is only a header and no log.c -------------------------
 * Everything is a macro. Levels are cut at compile time, so a disabled log
 * generates no code at all. In this environment (-nostdlib, no --gc-sections)
 * unused code otherwise stays in the image (see the opt-in comment in
 * Makefile.VERIF); with macros nothing is left behind to begin with.
 *
 * -- Output path -----------------------------------------------------
 *   LOG_x() -> printf()            Platform/Common/Src/printf.c
 *           -> _write(1, ...)      Platform/Common/Src/syscalls.c
 *           -> uart_write()        Platform/Common/Src/uart.c
 *           -> SiFive UART0 @0x10020000
 * That is exactly the path the UART_printf case has already verified.
 *
 * -- IMPORTANT: logging is expensive on RTL --------------------------
 * 115200 baud @ 500MHz pbus = roughly 43,000 cycles per character.
 * verilator runs a few thousand cycles/s, so one character takes tens of
 * seconds (measured wall-time for the UART_printf case: 4677 s = 78 min).
 * The default level is therefore LOG_LEVEL_NONE. Enabling it must be an
 * explicit choice by the test via -DLOG_LEVEL=...
 * Never enable it in cycle-measuring tests (CA/BENCH) - it contaminates the
 * measurement.
 *
 * -- Usage -----------------------------------------------------------
 *   build: EXTRA_CFLAGS="-DLOG_LEVEL=LOG_LEVEL_INFO"
 *   code:  LOG_INFO("pmp cfg=%02x addr=%08x", cfg, addr);
 *   The prefix is added automatically:  [I] pmp cfg=...
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 * The output path itself is already proven by the UART_printf case (PASS).
 * All this header adds is the level cut, so no separate case was created.
 * To check it, build with EXTRA_CFLAGS="-DLOG_LEVEL=LOG_LEVEL_WARN" and confirm
 * LOG_INFO disappears from the image (compare sizes).
 *
 * -- Caution before exit ---------------------------------------------
 * The moment exit() writes tohost, the host terminates the simulation and any
 * characters left in the TX FIFO are truncated. A test with logging enabled
 * must call uart_flush() before finishing (see the uart.h comment).
 */

#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_NONE   /* OFF by default - see the "expensive" note above */
#endif

#if LOG_LEVEL > LOG_LEVEL_NONE
#include "printf.h"
#define LOG_PRINT(tag, fmt, ...) printf("[" tag "] " fmt "\n", ##__VA_ARGS__)
#else
/* At level NONE, printf.h is not even included.
 * It expands to (void)0, so the arguments are not evaluated either - do not
 * pass arguments with side effects. */
#define LOG_PRINT(tag, fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) LOG_PRINT("E", fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(fmt, ...)  LOG_PRINT("W", fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(fmt, ...)  LOG_PRINT("I", fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) LOG_PRINT("D", fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#endif /* __LOG__ */
