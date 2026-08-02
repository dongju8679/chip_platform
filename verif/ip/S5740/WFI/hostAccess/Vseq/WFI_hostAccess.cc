#include "S5740_mcu_tests.h"
#include "WFI_hostAccess.h"
#include <stdlib.h>

static unsigned int base[2] = {TEST_ITIM_BASE, TEST_DTIM_BASE};
static unsigned int ref[2][TEST_MEM_SIZE/4] = {0,};

bool WFI_hostAccess()
{
    tick(10000);
    srand(time(NULL));
    for (unsigned int i = 0; i < ITERATION; i++) {
        unsigned int target = rand() %2;
        unsigned int offset = (rand() % TEST_MEM_SIZE) & (~0x3);
        unsigned int write = rand() % 1;
        if (write) {
            unsigned int data = rand();
            if(!master_write(base[target] + offset, data)) return false;
            ref[target][offset/4] = data;
        }
        else {
            unsigned int data;
            if(!master_read(base[target] + offset, data)) return false;
            if(data != ref[target][offset/4]){
                fprintf(stderr, "[test] read data wrong, addr = 0x%x, expected = 0x%x, actual = 0x%x\n", \
                    base[target]+offset, ref[target][offset/4], data);
                printf("[test] read data wrong, addr = 0x%x, expected = 0x%x, actual = 0x%x\n", \
                    base[target]+offset, ref[target][offset/4], data);
                return false;
            }
        }
    }
    trigger_irq(1);
    return true;
}

bool WFI_hostAccess_check()
{    
    for (unsigned int target = 0; target < 2; target++) {
        for (unsigned int offset = 0; offset < TEST_MEM_SIZE; offset += 4) {
            unsigned int data;
            if(!master_write(base[target] + offset, data)) return false;
            if (data != ref[target][offset/4]) {
                fprintf(stderr, "[check] read data wrong, addr = 0x%x, expected = 0x%x, actual = 0x%x\n", \
                    base[target]+offset, ref[target][offset/4], data);
                printf("[check] read data wrong, addr = 0x%x, expected = 0x%x, actual = 0x%x\n", \
                    base[target]+offset, ref[target][offset/4], data);
                return false;
            }
        }
    }
    return true;    
}