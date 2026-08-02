#ifndef __HTIF__
#define __HTIF__

/* htif.h - HTIF (tohost/fromhost) console output (header-only, shared HAL)
 *
 * -- Why it is needed: UART is far too slow for verification -----------
 *   UART0 = 115200 baud @ 500MHz pbus -> roughly 43,000 cycles per character.
 *   verilator runs a few thousand cycles/s, so a single character costs tens of
 *   seconds of wall-clock time.
 *   Measured: the UART_printf case took 4,677 s (78 min) for ~200 characters.
 *   Spending minutes just to print one PASS/FAIL marker line is not viable.
 *
 *   HTIF is the path where fesvr (testchip_tsi_t) polls and services
 *   tohost/fromhost. link.ld already provides tohost/fromhost and exit() in
 *   rtdev_shim.c already writes tohost, so this path is already alive.
 *   Neither the harness nor the simulator needs touching. One round trip per
 *   buffer.
 *
 * -- On provenance / duplication -------------------------------------
 *   This was copied verbatim from Middleware/benchmark/common/bench_htif.h
 *   (only the symbol names changed: the bench_ prefix became htif_).
 *   NOTE: bench_htif.h was deliberately not deleted, nor changed to include
 *     this file. Doing so would rename the static objects and thus change the
 *     already-PASSing CoreMark/Dhrystone images. Re-running the benchmarks
 *     costs more than carrying one duplicated header.
 *     Merging them is a follow-up task (see "next steps" in
 *     chip_docs/verif/docs/baremetal.md).
 *
 * -- What must NEVER be used on rv32: the bcd putchar path ------------
 *   The "convenient" HTIF path is a single write to tohost:
 *       tohost = (device<<56) | (cmd<<48) | payload    // device 1 = console
 *   On rv32 this breaks. tohost is 64-bit but RV32 has no 64-bit store, so GCC
 *   splits it into two sw instructions. If fesvr polls in between, it sees only
 *   the low word while the high word is still unwritten. It then mistakes that
 *   for a syscall request with device=0, cmd=0 and performs a shutdown.
 *   (Observed case: the moment Dhrystone printed '-' (0x2D),
 *      *** FAILED *** (tohost = 45)   <- 45 = 0x2D = '-')
 *
 * -- Hence the syscall path is used instead (riscv-tests standard) ----
 *   tohost receives the *address* of magic_mem. Under ilp32 an address is
 *   32-bit, so the high word is always 0 and stays 0 -> the split store has no
 *   intermediate state. The race cannot occur at all.
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 *   Already verified. CoreMark/Dhrystone print their reports through this path
 *   and PASS (build/logs/BENCH_*.log); the new EXCEPTION/PMP cases also emit
 *   their markers through it.
 *
 * -- Constraints -----------------------------------------------------
 *   - Do not use it on the +verif=<TEST> co-sim backend (verif_tsi_t).
 *     That side uses tohost under a different protocol. This is for
 *     self-checking cases only.
 *   - exit() also uses tohost. Output followed by exit is fine.
 */

#include <stdint.h>
#include <stddef.h>

extern volatile uint64_t tohost;
extern volatile uint64_t fromhost;

/* riscv-fesvr proxy kernel syscall number (the newlib/linux common write) */
#define HTIF_SYS_WRITE 64

/* 64-byte aligned so it stays safe even if fesvr reads a whole cache line. */
static volatile uint64_t htif_magic_mem[8] __attribute__((aligned(64)));

/* IMPORTANT: arguments must be passed as unsigned.
 * Casting a pointer through signed long into uint64_t sign-extends it, turning
 * 0x8000ee48 into 0xffffffff8000ee48, and the host cannot read that address.
 * rv32 addresses live in the 0x8000_0000 range with the MSB set, so this bug is
 * hit every single time. */
static inline uint64_t htif_syscall(uint64_t which, uint64_t a0,
                                    uint64_t a1, uint64_t a2)
{
    htif_magic_mem[0] = which;
    htif_magic_mem[1] = a0;
    htif_magic_mem[2] = a1;
    htif_magic_mem[3] = a2;
    __sync_synchronize();                       /* fence: make the arguments visible first */

    tohost = (uint64_t)(uintptr_t)htif_magic_mem;

    while (fromhost == 0) { }                   /* wait for the fesvr reply */
    fromhost = 0;
    __sync_synchronize();

    return htif_magic_mem[0];
}

/* Emit one buffer in a single round trip (fd 1 = stdout) */
static inline void htif_write(const char *buf, unsigned int len)
{
    if (buf && len)
        (void)htif_syscall(HTIF_SYS_WRITE, 1u,
                           (uint64_t)(uintptr_t)buf, (uint64_t)len);
}

static inline void htif_puts(const char *s)
{
    unsigned int n = 0;
    if (!s) return;
    while (s[n]) n++;
    htif_write(s, n);
}

/* Append a 32-bit value as 8 hex digits (for shipping results without printf) */
static inline void htif_puthex(uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    char b[8];
    int i;
    for (i = 7; i >= 0; i--) { b[i] = hex[v & 0xFu]; v >>= 4; }
    htif_write(b, 8);
}

#endif /* __HTIF__ */
