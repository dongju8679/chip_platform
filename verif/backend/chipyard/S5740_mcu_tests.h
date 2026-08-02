#ifndef S5740_MCU_TESTS_H
#define S5740_MCU_TESTS_H
/* S5740_mcu_tests.h - chipyard backend implementation of rt-dev's host test API.
 *
 * rt-dev exposes master_write/master_read/tick/trigger_irq/host_*_bp as FREE functions,
 * so its Vseq (.cc) files call them directly. On the chipyard backend those primitives
 * live in verif_tsi_t (verif_primitives_t). This header re-exports them as free functions
 * forwarding to the active instance, which lets rt-dev Vseq files compile unmodified.
 *
 * g_verif is set by verif_tsi_t::idle() before any test runs.
 *
 * -- the two conventions this adapter reconciles --
 * The seam (verif_primitives.h) and rt-dev do not report success the same way, and the
 * company Vseq files are written against rt-dev. Translating here is the whole job of an
 * adapter; the alternative was rewriting every sequence, which is what this refactor removed.
 *
 *   master_write / master_read : rt-dev returns bus success. The seam already does
 *                                (bool). Forward the value instead of dropping it -
 *                                the Vseq files do `if (!master_write(...)) return false;`.
 *
 *   host_wait_bp               : * POLARITY IS INVERTED between the two.
 *                                seam  -> true  = the breakpoint was reached
 *                                rt-dev-> 0     = the breakpoint was reached,
 *                                          non-0 = aborted (BP_END / BP_TEST_fail / timeout)
 *                                Every company Vseq reads `if (host_wait_bp(BP_TEST_end))
 *                                return false;`. The DUT-side twin confirms the convention:
 *                                Platform/Common/Src/util.c :: dut_wait_bp() returns 0 when
 *                                the breakpoint is seen and 1 on BP_END/BP_TEST_fail.
 *                                So the negation below is not a workaround - it is the
 *                                seam-to-rt-dev translation, and it belongs in this file.
 *
 *   host_check_bp              : same polarity on both sides (true = value matched).
 *                                It was simply missing here, which is the second reason the
 *                                company Vseq files did not compile.
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "verif_addr_chipyard.h"   /* chipyard DUT addresses: BREAK_POINT_ADDR, VERIF_*   */
                                   /* (must precede verif_primitives.h - it overrides the */
                                   /*  seam's #ifndef defaults for VERIF_TEST/BREAK_ADDR) */
#include "verif_primitives.h"

extern verif_primitives_t* g_verif;

static inline bool master_write(unsigned int addr, unsigned int data) { return g_verif->master_write(addr, data); }
static inline bool master_read (unsigned int addr, unsigned int& data){ return g_verif->master_read(addr, data); }
static inline void tick        (int n)            { g_verif->tick(n); }
static inline void trigger_irq (unsigned int mask){ g_verif->trigger_irq(mask); }
static inline void host_set_bp (unsigned int bp)  { g_verif->host_set_bp(bp); }
/* returns 0 when bp was reached, non-0 on abort/timeout (rt-dev convention - see above) */
static inline bool host_wait_bp(unsigned int bp)  { return !g_verif->host_wait_bp(bp); }
static inline bool host_check_bp(unsigned int addr, unsigned int expect) { return g_verif->host_check_bp(addr, expect); }

/* -- CA (cycle-accurate) extension --
 * get_cycle()/supports_ca() are on the seam, so they forward like the rest. get_instret() is
 * not - it is a verif_tsi_t method. Declaring it here and defining it in verif_host.h after the
 * class is the least invasive wiring: the seam header stays untouched, and a Vseq that needs the
 * instruction counter (CA_measure) can still call it. The definition lives in the one TU
 * (SimTSI.cc) that includes verif_host.h.
 */
static inline uint64_t host_get_cycle()   { return g_verif->get_cycle(); }
static inline bool     host_supports_ca() { return g_verif->supports_ca(); }
uint64_t               host_get_instret();

#endif /* S5740_MCU_TESTS_H */
