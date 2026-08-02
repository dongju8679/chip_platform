#ifndef VERIF_MBX_BASE_H
#define VERIF_MBX_BASE_H
/* verif_mbx_base.h - one base address for the whole host-DUT mailbox region.
 *
 * Every mailbox address in the tree is an offset from a single base. Keeping
 * them derived rather than written out means a target whose memory map differs
 * needs one -D, not a dozen edits that can drift apart.
 *
 *   VERIF_MBX_BASE + 0x0000   BREAK_POINT / VERIF_BREAK_ADDR
 *                  + 0x0004   VERIF_TEST_ADDR
 *                  + 0x0010   VERIF_IRQ_INJECT_ADDR
 *                  + 0x0020   VERIF_DUT_DBG_ADDR
 *                  + 0x0040   cycle mirror lo/hi
 *                  + 0x0048   instret mirror lo/hi
 *                  + 0x0080   BENCH_smoke scratch
 *                  + 0x0100   CA result table
 *                  + 0x1000   per-test debug base (TEST_DEBUG_BASE_ADDR)
 *
 * The DUT and the host must agree on this value. They are built by different
 * toolchains and share no headers, so both sides include this file.
 *
 * Default 0x80010000 is the chipyard RV32RocketConfig layout, where DRAM is
 * flat and this region is simply unused memory above the image.
 *
 * On a target with tightly-integrated memories - SCRConfig, for instance, where
 * ITIM covers 0x80000000..0x8001FFFF and DTIM starts at 0x80020000 - the default
 * lands in *instruction* memory. A data store there faults. Override it:
 *
 *     EXTRA_CFLAGS="-DVERIF_MBX_BASE=0x80038000u"
 *
 * and pass the same value to the host build.
 */

#ifndef VERIF_MBX_BASE
#define VERIF_MBX_BASE 0x80010000u
#endif

#define VERIF_MBX_OFF_BREAK      0x0000u
#define VERIF_MBX_OFF_TEST       0x0004u
#define VERIF_MBX_OFF_IRQ        0x0010u
#define VERIF_MBX_OFF_DBG        0x0020u
#define VERIF_MBX_OFF_CYCLE_LO   0x0040u
#define VERIF_MBX_OFF_CYCLE_HI   0x0044u
#define VERIF_MBX_OFF_INSTR_LO   0x0048u
#define VERIF_MBX_OFF_INSTR_HI   0x004Cu
#define VERIF_MBX_OFF_SMOKE      0x0080u
#define VERIF_MBX_OFF_CA_TABLE   0x0100u
#define VERIF_MBX_OFF_TEST_DEBUG 0x1000u

#endif /* VERIF_MBX_BASE_H */
