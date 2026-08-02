#include "S5740_mcu_tests.h"
#include "CLINT_sw_interrupt.h"

bool CLINT_sw_interrupt()
{
    #ifndef PRELOAD
        host_set_bp(BP_MEM_WRITE);
    #endif

    uint32_t success_condition[BP_MAX] = {1, 2, 3, 4, 5};
    uint32_t bp_num;
    for (bp_num = 0; bp_num < BP_MAX; bp_num++) {
        if (!master_write(0x02000000, 1)) return false;
        host_set_bp(BP_TEST_begin);

        if (host_wait_bp(BP_TEST_end))
            return false;

        if (!(host_check_bp(TEST_ADDR, success_condition[bp_num]))){
            return false;
        }

        /* Release the DUT's dut_wait_bp(BP_TEST_success) at the end of each round.
         * util.c::dut_wait_bp() also accepts BP_TEST_begin as a release, so the next
         * round's host_set_bp(BP_TEST_begin) would unblock it too -- but only for
         * rounds 1..BP_MAX-1. Without this the final round never gets a release and
         * the DUT spins in dut_wait_bp() forever instead of returning from main(). */
        host_set_bp(BP_TEST_success);
    }
    unsigned int bp,cnt;
    if(!master_read(BREAK_POINT_ADDR, bp)) return false;
    if(!master_read(TEST_ADDR, cnt)) return false;

    fprintf(stderr, "[Debug] BP = 0x%x, COUNT = 0x%x\n", bp, cnt);
    return true;
}

bool CLINT_sw_interrupt_check()
{
    return true;
}
