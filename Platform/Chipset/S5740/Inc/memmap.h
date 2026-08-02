/* memmap.h - chipyard DRAM port (v301, Phase 1)
 *
 * The rt-dev original is based on ITIM/DTIM (0x20000/0x40000).
 * chipyard Phase 1 has a single DRAM region at 0x80000000.
 * The mapping is chosen so the range check in util.c's __wrap_memset/memcpy
 * permits DRAM.
 *
 * NOTE: when Phase 2 adds real ITIM/DTIM via the chipyard Config, only this
 *   file needs updating to the real addresses (util.c stays untouched).
 *
 * Range check logic (util.c):
 *   valid = [ITIM_BASE, ITIM_BASE+ITIM_SIZE) union [DTIM_BASE, DTIM_BASE+DTIM_SIZE)
 * chipyard: the whole DRAM from 0x80000000 is mapped as "ITIM+DTIM contiguous"
 *   ITIM_BASE = 0x80000000, ITIM_SIZE = 0x10000 (64K code)
 *   DTIM_BASE = 0x80010000, DTIM_SIZE = 0x10000 (64K: the mailbox region)
 *   -> 0x80000000~0x8001FFFF is valid (covers both code and mailbox)
 */

/* chipyard DRAM mapping (Phase 1) */
#define ITIM_BASE      0x80000000
#define ITIM_SIZE      0x00010000   /* 64K: code/data/stack */
#define DTIM_BASE      0x80010000
#define DTIM_SIZE      0x00010000   /* 64K: host mailbox region */

/* Original rt-dev symbols (for reference; Phase 1 uses the chipyard values above) */
/* ROM_BASE/RF_BASE/STACK_POINTER etc. are superseded by crt.S's __stack_top */
