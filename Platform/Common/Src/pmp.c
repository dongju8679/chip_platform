/* pmp.c - Physical Memory Protection configuration (RV32, M-mode)
 *
 * See Platform/Common/Inc/pmp.h for the API rationale and caveats (especially
 * the lock bit).
 *
 * -- Implementation notes ---------------------------------------------
 * A CSR number is an immediate encoded into the instruction, so it cannot be
 * accessed by a runtime index. pmpaddr0..7 / pmpcfg0..1 are therefore expanded
 * into switch statements. It looks verbose, but there is no alternative, and it
 * only needs to exist once, in this file.
 *
 * RV32 pmpcfg layout:
 *   pmpcfg0 = [cfg3 cfg2 cfg1 cfg0]  (entries 0..3)
 *   pmpcfg1 = [cfg7 cfg6 cfg5 cfg4]  (entries 4..7)
 * So the cfg byte of entry i is byte (i%4) of pmpcfg(i/4).
 */
#include "pmp.h"
#include "encoding.h"

/* -- pmpaddr access --------------------------------------------------- */
static xlen_t pmpaddr_read(unsigned idx)
{
    switch (idx) {
    case 0: return read_csr(pmpaddr0);
    case 1: return read_csr(pmpaddr1);
    case 2: return read_csr(pmpaddr2);
    case 3: return read_csr(pmpaddr3);
    case 4: return read_csr(pmpaddr4);
    case 5: return read_csr(pmpaddr5);
    case 6: return read_csr(pmpaddr6);
    case 7: return read_csr(pmpaddr7);
    default: return 0;
    }
}

static void pmpaddr_write(unsigned idx, xlen_t v)
{
    switch (idx) {
    case 0: write_csr(pmpaddr0, v); break;
    case 1: write_csr(pmpaddr1, v); break;
    case 2: write_csr(pmpaddr2, v); break;
    case 3: write_csr(pmpaddr3, v); break;
    case 4: write_csr(pmpaddr4, v); break;
    case 5: write_csr(pmpaddr5, v); break;
    case 6: write_csr(pmpaddr6, v); break;
    case 7: write_csr(pmpaddr7, v); break;
    default: break;
    }
}

/* -- pmpcfg access (replaces only the byte of a single entry) ---------- */
static xlen_t pmpcfg_read_word(unsigned widx)
{
    switch (widx) {
    case 0: return read_csr(pmpcfg0);
    case 1: return read_csr(pmpcfg1);
    default: return 0;
    }
}

static void pmpcfg_write_word(unsigned widx, xlen_t v)
{
    switch (widx) {
    case 0: write_csr(pmpcfg0, v); break;
    case 1: write_csr(pmpcfg1, v); break;
    default: break;
    }
}

static unsigned pmpcfg_read_byte(unsigned idx)
{
    xlen_t w = pmpcfg_read_word(idx >> 2);
    return (unsigned)((w >> ((idx & 3u) * 8u)) & 0xFFu);
}

static void pmpcfg_write_byte(unsigned idx, unsigned cfg)
{
    unsigned shift = (idx & 3u) * 8u;
    xlen_t   w     = pmpcfg_read_word(idx >> 2);
    w &= ~((xlen_t)0xFFu << shift);
    w |=  ((xlen_t)(cfg & 0xFFu) << shift);
    pmpcfg_write_word(idx >> 2, w);
}

/* -- NAPOT encoding ---------------------------------------------------
 * size == 4        : NA4.   pmpaddr = base >> 2 (no low bit pattern)
 * size >= 8, 2^k   : NAPOT. pmpaddr = (base >> 2) | ((size >> 3) - 1)
 *   (Equivalent to shifting size/2 - 1 right by 2: (size/2-1)>>2 == size/8 - 1
 *    holds when size is a power of two.)
 * Returns 0 if the address is unaligned or the size is not a power of two.
 */
xlen_t pmp_napot_encode(xlen_t base, xlen_t size)
{
    if (size < 4u) return 0;
    if (size & (size - 1u)) return 0;        /* not a power of two */
    if (base & (size - 1u)) return 0;        /* not aligned to size */

    if (size == 4u)
        return base >> 2;                    /* NA4 */
    return (base >> 2) | ((size >> 3) - 1u); /* NAPOT */
}

static int pmp_program(unsigned idx, xlen_t base, xlen_t size,
                       unsigned perm, unsigned lock)
{
    xlen_t   addr;
    unsigned cfg;

    if (idx >= PMP_NUM_REGIONS) return -1;
    if (perm & ~(unsigned)PMP_PERM_RWX) return -2;

    addr = pmp_napot_encode(base, size);
    if (addr == 0 && !(base == 0 && size == 4u)) return -3;

    cfg = perm | ((size == 4u) ? PMP_MODE_NA4 : PMP_MODE_NAPOT);
    if (lock) cfg |= PMP_LOCK;

    /* Order matters: write addr first, cfg second.
     * Writing cfg first (especially together with lock) starts enforcing the
     * check against the old addr from that instant, and once locked, writes to
     * addr are ignored. */
    pmpaddr_write(idx, addr);
    pmpcfg_write_byte(idx, cfg);
    return 0;
}

int pmp_set_region(unsigned idx, xlen_t base, xlen_t size, unsigned perm)
{
    return pmp_program(idx, base, size, perm, 0);
}

int pmp_lock_region(unsigned idx, xlen_t base, xlen_t size, unsigned perm)
{
    return pmp_program(idx, base, size, perm, 1);
}

int pmp_clear_region(unsigned idx)
{
    if (idx >= PMP_NUM_REGIONS) return -1;
    /* For a locked entry this write is ignored (the hardware blocks it).
     * Use pmp_get_region() to detect the failure if you need to report it. */
    pmpcfg_write_byte(idx, PMP_MODE_OFF);
    pmpaddr_write(idx, 0);
    return 0;
}

int pmp_get_region(unsigned idx, unsigned *cfg, xlen_t *addr)
{
    if (idx >= PMP_NUM_REGIONS) return -1;
    if (cfg)  *cfg  = pmpcfg_read_byte(idx);
    if (addr) *addr = pmpaddr_read(idx);
    return 0;
}
