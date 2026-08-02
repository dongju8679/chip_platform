#include "S5740_mcu_tests.h"
#include "CLINT_timer_interrupt.h"

bool CLINT_timer_interrupt()
{
    #ifndef PRELOAD
        host_set_bp(BP_MEM_WRITE);
        tick(90);
    #endif

    uint32_t success_condition = 1;
    uint32_t bp_num;
    for (bp_num = 0; bp_num < BP_MAX; bp_num++) {        
        
        host_set_bp(BP_TEST_begin);
        tick(5000);

        if (host_wait_bp(BP_TEST_end))
            return false;

        if (!(host_check_bp(TEST_ADDR, success_condition))){
            unsigned int got = 0;
            master_read(TEST_ADDR, got);
            printf("fail here at %d th iteration, read value %d\n", bp_num, got);
            return false;
        }
        success_condition++;

        /* Release the DUT's dut_wait_bp(BP_TEST_success) -- see CLINT_sw_interrupt.cc.
         * Needed for the final round, which gets no following BP_TEST_begin. */
        host_set_bp(BP_TEST_success);
    }
    unsigned int bp = 0, cnt = 0;

    /* BREAK_POINT_ADDR must be read before it is printed -- matches CLINT_sw_interrupt.cc.
     * Without this read bp was used uninitialised and the [Debug] line reported BP = 0x0. */
    if(!master_read(BREAK_POINT_ADDR, bp)) {
        printf("fail at final check\n");
        return false;
    }

    if(!master_read(TEST_ADDR, cnt)) {
        printf("fail at final check\n");
        return false;
    }

    fprintf(stderr, "[Debug] BP = 0x%x, COUNT = 0x%x\n", bp, cnt);
    return true;
}

bool CLINT_timer_interrupt_check()
{
    return true;
}
