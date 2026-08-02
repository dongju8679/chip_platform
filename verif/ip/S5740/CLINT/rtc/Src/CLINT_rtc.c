#include "S5740_test.h"
#include "CLINT_rtc.h"

void CLINT_rtc()
{
    unit32_t clint_time[2];
    uint64_t *realtime64 = (uint64_t *)clint_time;
    uint64_t before = 0;
    while(true){
        clint_time[0] = inpdw(0x2B00);
        clint_time[1] = inpdw(0x2B04);
        if(*realtime64 < before) exit(CODE_FAIL);
        if(*realtime64 >= (0x7EADBEEFDEADBEEF + 0x1000000)) break;
        before = *realtime64;
    }
}

int main()
{
    CLINT_rtc();
    return CODE_SUCCESS;
}  
