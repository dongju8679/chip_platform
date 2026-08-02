#ifndef VERIF_ADDR_CHIPYARD_H
#define VERIF_ADDR_CHIPYARD_H

/* Mailbox base. Must match Platform/Common/Inc/verif_mbx_base.h on the DUT side.
 * The host and the DUT are built by different toolchains and share no include
 * path, so the value is duplicated here by necessity - keep them together. */
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

/* verif_addr_chipyard.h - chipyard(RV32RocketConfig) DUT address map, host side.
 *
 * Why this file exists
 *   The company original Vseq files (verif/ip/S5740/<IP>/<sub>/Vseq/<test>.cc) are copied in
 *   byte-for-byte. They use rt-dev's symbol names - BREAK_POINT_ADDR above all - which the
 *   host side never defined, so they could not compile and verif_host.h ended up re-implementing
 *   every sequence by hand. Defining those names once, here, is what makes the originals build.
 *
 *   The DUT side has the same constants under Platform/Common/Inc/util.h (BREAK_POINT_ADDR) and
 *   Platform/Common/Inc/verif_ca.h (VERIF_CA_*). Keep the two in sync - they are one contract
 *   seen from two ends.
 *
 * Included by S5740_mcu_tests.h (so every Vseq gets it for free, with no edit to the company
 * .cc files) and by verif_host.h. apply_backend.sh copies it next to SimTSI.cc.
 *
 * -- DUT memory-map dependency --
 *   chipyard RV32RocketConfig has DRAM @0x80000000. The SDK values (0x50000/0x51000) are S5740
 *   RTL addresses and are not reachable in chipyard DRAM, so the chipyard backend overrides them
 *   with DRAM-region addresses. The BP_* numbers (verif_primitives.h) are protocol and stay common.
 *
 * * TEST_ADDR is deliberately NOT defined here. Each test's Inc/<test>.h picks its own
 *   (0x80011000 for the CLINT/PLIC mailbox, 0x80010004 for I2SR). Defining it here would
 *   silently override them.
 */

/* host<->DUT breakpoint mailbox. Two names for one address:
 *     VERIF_BREAK_ADDR  - framework/seam name (verif_primitives.h default is overridden below)
 *     BREAK_POINT_ADDR  - rt-dev name, used by the company Vseq and by DUT-side util.h
 */
#define VERIF_TEST_ADDR   (VERIF_MBX_BASE + VERIF_MBX_OFF_TEST)
#define VERIF_BREAK_ADDR  (VERIF_MBX_BASE + VERIF_MBX_OFF_BREAK)
#ifndef BREAK_POINT_ADDR
#define BREAK_POINT_ADDR  (VERIF_MBX_BASE + VERIF_MBX_OFF_BREAK)
#endif

/* CA: host-visible addresses where the DUT mirrors mcycle/minstret (cycle-accurate counters).
 *   DUT side = Platform/Common/Inc/verif_ca.h :: verif_ca_mark_cycle() (method 1, no DPI).
 *   * these sit at 0x80010040.. and not 0x80010008: 0x80010008 is I2SR_int_muldiv's BREAK_ADDR
 *     (exit flag). A mirror the host polls must not alias another test's flag.
 */
#define VERIF_CYCLE_LO_ADDR (VERIF_MBX_BASE + VERIF_MBX_OFF_CYCLE_LO)
#define VERIF_CYCLE_HI_ADDR (VERIF_MBX_BASE + VERIF_MBX_OFF_CYCLE_HI)
#define VERIF_INSTR_LO_ADDR (VERIF_MBX_BASE + VERIF_MBX_OFF_INSTR_LO)
#define VERIF_INSTR_HI_ADDR (VERIF_MBX_BASE + VERIF_MBX_OFF_INSTR_HI)

/* CA result table written by the DUT (CA_measure). [0]=magic [1]=count, entries from +16. */
#define VERIF_CA_TABLE_ADDR (VERIF_MBX_BASE + VERIF_MBX_OFF_CA_TABLE)
#define VERIF_CA_MAGIC      0xCA5EC0DEu
#define VERIF_CA_ENTRY_SZ   32u

/* co-sim: host->DUT interrupt injection mailbox (cooperative IRQ model without DPI).
 * The DUT IRQ polling routine reads this address and raises the corresponding IRQ. */
#define VERIF_IRQ_INJECT_ADDR (VERIF_MBX_BASE + VERIF_MBX_OFF_IRQ)

/* DUT progress marker (DBG() in the .c) - read by the host_wait_bp timeout diagnostic. */
#define VERIF_DUT_DBG_ADDR    (VERIF_MBX_BASE + VERIF_MBX_OFF_DBG)

#endif /* VERIF_ADDR_CHIPYARD_H */
