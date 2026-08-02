#include "i2sr.h"
#include "interrupt.h"

TRAP_ENTRY void ISR(void) {
    unsigned int id = csr_read(CSR_CURRID);
    get_ext_int_handler(id)(id);
}

void register_handler(unsigned int id, void* handler) {
    volatile unsigned int inst[2];
    inst[0] = ((0xBC0+id-1) << 20) | (10 << 15) | MATCH_CSRRW;
    inst[1] = 0x8082;
    asm volatile ("fence.i");
    ((void (*)(void*))inst)(handler);
}

int i2sr_register_ext_int_handler(int id, int_fp handler, int priority) {
    if(priority > 5)
        return CODE_FAIL;
    register_ext_int_handler(id, handler, priority);
    if(priority >= 2 && priority <= 5)
        register_handler(id, (void*)ISR);

    return CODE_SUCCESS;
}
