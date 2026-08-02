#include "S5740_mcu_tests.h"
#include "PLIC_enable.h"

bool PLIC_enable()
{
#ifndef PRELOAD
        host_set_bp(BP_MEM_WRITE);
#endif
        uint32_t success_condition = 1;

        long long input = 0;
        long long bp_num;
        for(bp_num = 0; bp_num < MAX_PLIC_NUMBER * 2; bp_num++) {
                if (bp_num < MAX_PLIC_NUMBER) {
                        input = ((long long)1 << bp_num);
                }
                else {
                        if(bp_num == MAX_PLIC_NUMBER)
                                host_wait_bp(BP_TEST_init);
                        input = ((long long)1 << (bp_num - MAX_PLIC_NUMBER));
                }
                trigger_irq(input);
                host_set_bp(BP_TEST_begin);

                if(host_wait_bp(BP_TEST_end))
                        return false;


                if(bp_num < MAX_PLIC_NUMBER){
                        if(!(host_check_bp(TEST_ADDR, success_condition))) {
                                return false;
                        }
                        success_condition++;
                }
                else{
                        if(!(host_check_bp(TEST_ADDR, 0))) {
                                return false;
                        }
                }
        }
        unsigned int bp, cnt;
        if (!master_read(BREAK_POINT_ADDR, bp)) return false;
        if (!master_read(TEST_ADDR, cnt)) return false;
        fprintf(stderr, "[Debug] BP = 0x%x, COUNT = 0x%x\n", bp, cnt);
        return true;
}

bool PLIC_enable_check()
{
        return true;
}
