/* DEMO_handshake.cc - host Vseq for the framework's own host_*bp round-trip demo.
 *
 * Not an IP test: it belongs to verif/framework, and its companion firmware is
 * verif/framework/handshake/Src/handshake.c. It lives in a Vseq/ folder anyway so that
 * verif_host.h can dispatch it like everything else (apply_backend.sh copies verif/framework
 * Vseq/Inc alongside verif/ip).
 *
 * Sequence: wait for the DUT to publish a result, check it, release the DUT.
 *
 * * Carried over verbatim from verif_host.h::run_vseq, including the breakpoint numbers.
 *   Be aware before reusing it: handshake.c signals with its own literals (BP_DONE=2,
 *   BP_RELEASE=3) while this side uses the seam enum (BP_TEST_begin=0xFFFFFFF2,
 *   BP_TEST_end=0xFFFFFFF5), and Implementation/Makefile.VERIF only builds firmware from
 *   verif/ip/<TARGET>/<F>/<SF>/Src - so there is no build rule for handshake.c and this demo
 *   is currently unexercised. The mismatch is left as found rather than silently "fixed".
 */
#include "S5740_mcu_tests.h"

bool DEMO_handshake()
{
    fprintf(stderr, "[verif] DEMO_handshake: waiting bp=%u\n", BP_TEST_begin);
    if (host_wait_bp(BP_TEST_begin)) return false;

    unsigned int got = 0;
    if (!master_read(VERIF_TEST_ADDR, got)) return false;
    bool ok = (got == 0xABCDu);
    fprintf(stderr, "[Debug] handshake result = 0x%X (expect 0xABCD)\n", got);

    master_write(VERIF_BREAK_ADDR, BP_TEST_end);   /* release the DUT */
    return ok;
}

bool DEMO_handshake_check()
{
    return true;
}
