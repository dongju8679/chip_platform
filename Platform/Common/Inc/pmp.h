#ifndef __PMP__
#define __PMP__

/* pmp.h - Physical Memory Protection configuration API (RV32, M-mode)
 *
 * -- Why this API ----------------------------------------------------
 * PMP looks like something you can "just write the CSRs for", but in practice
 * it is full of landmines. This API keeps them behind the header.
 *
 *  (1) CSR numbers must be compile-time constants.
 *      csrw pmpaddr0, x works, but csrw pmpaddr[i], x does not.
 *      Indexed access therefore needs a 16-way switch. There is no reason to
 *      copy that into every test when the driver can write it once.
 *
 *  (2) The address encoding is not intuitive.
 *      pmpaddr is the physical address shifted right by 2 (PMP_SHIFT=2), and
 *      NAPOT layers a "size-1 bit pattern" on top of that.
 *      napot encoding: addr>>2 | ((size/2 - 1) >> 2)
 *      The size must be at least 8 bytes and a power of two.
 *      RV32RocketConfig has pmpgranularity=4, so NA4 (4 bytes) also works.
 *
 *  (3) IMPORTANT: in M-mode the check only applies if the L(lock) bit is set.
 *      An entry with L=0 has no effect on M-mode accesses (only S/U mode is
 *      checked). This is 99% of all "I configured PMP, why is there no trap?"
 *      This configuration (RV32RocketConfig) uses M-mode only, so verifying
 *      PMP requires locking.
 *
 *  (4) IMPORTANT: once locked, an entry cannot be released before reset.
 *      Writes to the corresponding pmpcfg byte and to pmpaddr are ignored
 *      entirely. pmp_lock_region() is therefore irreversible.
 *      Before using it in a test, confirm that "this region is not used again
 *      until the test ends". Locking the code or stack region kills you on the
 *      spot.
 *
 * -- DUT specification (RV32RocketConfig) ----------------------------
 *   dts: riscv,pmpregions = <8>, riscv,pmpgranularity = <4>
 *   -> 8 entries, minimum granularity 4 bytes (NA4 usable).
 *   Only pmpcfg0..1 are valid (on RV32 one cfg register covers 4 entries).
 *
 * -- Is this verifiable on chipyard? ---------------------------------
 *   Yes. verif/ip/S5740/PMP/violation locks a region through this API and
 *   performs a violating access, confirming that mcause 5 (load access fault) /
 *   7 (store access fault) actually fire.
 *
 * -- Usage -----------------------------------------------------------
 *   exceptions_init();
 *   exception_expect(CAUSE_STORE_ACCESS);
 *   pmp_lock_region(0, 0x80020000, 4096, 0);      // perm 0 = all access denied
 *   *(volatile int *)0x80020000 = 1;              // -> store access fault
 *   if (!exception_matched()) fail();
 */

#include "type.h"

/* Permission bits. Same values as PMP_R/W/X in encoding.h, but there are two
 * copies of encoding.h (see the trap.h comment), so the names are pinned here. */
#define PMP_PERM_NONE   0x00u
#define PMP_PERM_R      0x01u
#define PMP_PERM_W      0x02u
#define PMP_PERM_X      0x04u
#define PMP_PERM_RW     (PMP_PERM_R | PMP_PERM_W)
#define PMP_PERM_RX     (PMP_PERM_R | PMP_PERM_X)
#define PMP_PERM_RWX    (PMP_PERM_R | PMP_PERM_W | PMP_PERM_X)

/* Address matching mode (pmpcfg[4:3]) */
#define PMP_MODE_OFF    0x00u
#define PMP_MODE_TOR    0x08u   /* [previous entry addr, this entry addr) */
#define PMP_MODE_NA4    0x10u   /* fixed 4 bytes */
#define PMP_MODE_NAPOT  0x18u   /* 2^k bytes (k>=3) */

#define PMP_LOCK        0x80u

#define PMP_NUM_REGIONS 8       /* RV32RocketConfig: dts riscv,pmpregions */

/* Configure one NAPOT region (without locking).
 *   idx   : 0..PMP_NUM_REGIONS-1
 *   base  : starting physical address. Must be aligned to size.
 *   size  : 4 (NA4) or a power of two >= 8 (NAPOT)
 *   perm  : combination of PMP_PERM_x
 * Returns: 0 = success, negative = argument error.
 *
 * NOTE: it does not lock, so M-mode accesses are unaffected.
 *   Use it to inspect the configured values, or to lock later.
 */
int pmp_set_region(unsigned idx, xlen_t base, xlen_t size, unsigned perm);

/* Same as pmp_set_region() but also sets the L(lock) bit.
 * -> From this point on, M-mode accesses are checked too.
 * -> Irreversible before reset. Read caveat (4) above. */
int pmp_lock_region(unsigned idx, xlen_t base, xlen_t size, unsigned perm);

/* Turn an entry back OFF (no effect on a locked entry).
 * Returns: 0 = success, negative = argument error. */
int pmp_clear_region(unsigned idx);

/* Read back the current configuration. For debugging / self-checking.
 * cfg  : the corresponding pmpcfg byte (perm | mode | lock)
 * addr : raw pmpaddr register value (already shifted right by 2)
 * Returns: 0 = success, negative = argument error. */
int pmp_get_region(unsigned idx, unsigned *cfg, xlen_t *addr);

/* Encode base/size into a NAPOT pmpaddr value. Returns 0 on invalid arguments.
 * pmp_set_region() uses it internally, but it is exposed so a test can compute
 * the expected value itself and compare. */
xlen_t pmp_napot_encode(xlen_t base, xlen_t size);

#endif /* __PMP__ */
