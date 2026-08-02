#ifndef __BENCH_HTIF__
#define __BENCH_HTIF__
/* bench_htif.h - HTIF (tohost/fromhost) console output (benchmark only,
 *                additive only)
 *
 * -- Why HTIF and not the UART ------------------------------------
 *   UART0 at 115200 baud @ 500MHz pbus costs about 43,000 cycles per character.
 *   verilator runs a few thousand cycles/s, so one character takes tens of
 *   seconds of real time.
 *   Printing the CoreMark report (700+ characters) over the UART would take
 *   hours longer than the benchmark itself, and those UART cycles would mix
 *   into mcycle and contaminate the measurement.
 *
 *   HTIF, by contrast, is the path where fesvr (testchip_tsi_t) polls and
 *   services tohost/fromhost. link.ld already provides tohost/fromhost and
 *   exit() in rtdev_shim.c already writes tohost, so this path is already alive
 *   - neither the harness nor the simulator needs touching.
 *
 * -- What must NEVER be used on rv32: the bcd putchar path --------
 *   The "convenient" HTIF path is a single write to tohost:
 *       tohost = (device<<56) | (cmd<<48) | payload    // device 1 = console
 *   This breaks on rv32. tohost is 64-bit but RV32 has no 64-bit store, so GCC
 *   splits it into two sw instructions. If fesvr polls tohost in between, it
 *   sees only the low word while the high word (device/cmd) is still unwritten.
 *   It then mistakes that for a syscall request with device=0, cmd=0 and
 *   performs a shutdown.
 *
 *   We really did hit this trap. The moment Dhrystone printed '-' (0x2D):
 *       *** FAILED *** (tohost = 45)
 *       Assertion failed in TOP.TestDriver.testHarness: exit code = 45
 *   45 = 0x2D = '-'. fesvr read only the low word before the high word had
 *   been written.
 *   Because it is timing dependent, short outputs (a smoke test) pass by luck -
 *   which makes it even more dangerous.
 *
 * -- Hence the syscall path is used instead (riscv-tests standard) -
 *   tohost receives the ADDRESS of magic_mem. Under ilp32 an address is 32-bit,
 *   so the high word is always 0 and stays 0 -> the split store has no
 *   intermediate state. The race cannot occur at all.
 *
 *       magic_mem[0..3] = { syscall_num, arg0, arg1, arg2 }
 *       fence
 *       tohost = (uintptr_t)magic_mem      // only the low word goes 0 -> non-0
 *       while (fromhost == 0) ;            // wait for the fesvr reply
 *       fromhost = 0
 *
 *   As a bonus, sys_write(fd, buf, len) can be sent in one go, making it one
 *   round trip per buffer rather than per character. Far faster than bcd
 *   putchar.
 */
#include <stdint.h>
#include <stddef.h>

extern volatile uint64_t tohost;
extern volatile uint64_t fromhost;

/* riscv-fesvr proxy kernel syscall number (the newlib/linux common write) */
#define HTIF_SYS_WRITE 64

/* 64-byte aligned so it stays safe even if fesvr reads a whole cache line. */
static volatile uint64_t bench_magic_mem[8] __attribute__((aligned(64)));

/* IMPORTANT: arguments must be passed as unsigned.
 * Casting a pointer through signed long into uint64_t sign-extends it, turning
 * 0x8000ee48 into 0xffffffff8000ee48, and the host cannot read that address.
 *   Access exception occurred while host was accessing memory on behalf
 *   of target: Memory address 0xffffffff8000ee48 is invalid
 * rv32 addresses live in the 0x8000_0000 range with the MSB set, so this bug is
 * hit every single time. */
static inline uint64_t bench_htif_syscall(uint64_t which, uint64_t a0,
                                          uint64_t a1, uint64_t a2)
{
    bench_magic_mem[0] = which;
    bench_magic_mem[1] = a0;
    bench_magic_mem[2] = a1;
    bench_magic_mem[3] = a2;
    __sync_synchronize();                       /* fence: make the arguments visible first */

    /* ilp32 -> the upper 32 bits of this value are 0. The tohost high word only
     * ever goes from 0 to 0, so the single low-word store is itself the atomic
     * request. */
    tohost = (uint64_t)(uintptr_t)bench_magic_mem;

    while (fromhost == 0) { }                   /* wait for the fesvr reply */
    fromhost = 0;
    __sync_synchronize();

    return bench_magic_mem[0];                  /* return value */
}

/* Emit one buffer in a single round trip (fd 1 = stdout) */
static inline void htif_write(const char *buf, unsigned int len)
{
    if (buf && len)
        (void)bench_htif_syscall(HTIF_SYS_WRITE, 1u,
                                 (uint64_t)(uintptr_t)buf, (uint64_t)len);
}

static inline void htif_putc(char c)
{
    htif_write(&c, 1);
}

static inline void htif_puts(const char *s)
{
    unsigned int n = 0;
    if (!s) return;
    while (s[n]) n++;
    htif_write(s, n);
}

#endif /* __BENCH_HTIF__ */
