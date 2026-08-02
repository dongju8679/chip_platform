// PLIC_latency.c - CA verification firmware: measure interrupt latency
//   _start -> main -> PLIC_latency() -> exit(tohost)
//   host(verif_ca_run) does trigger_irq -> ISR -> writes PMU measurement to RESULT_ADDR.
//   BP values are firmware-side defines matching the seam (verif_primitives.h) enum.
#include "S5740_test.h"
#include "PLIC_latency.h"

#define BP_ADDR         0x80010000u
#define BP_TEST_begin   2u
#define BP_TEST_end     3u
#define IRQ_INJECT_ADDR 0x80010010u   // chipyard co-sim IRQ mailbox

static inline void host_set_bp(unsigned bp){ outpdw(BP_ADDR, bp); }

volatile uint32_t g_latency = 0;

void isr(void) {
    uint32_t c1 = inpdw(PMU_COUNTER_ADDR(PLIC_IRQ_ID));
    PMUCounter pc; __builtin_memcpy(&pc, &c1, sizeof(pc));
    g_latency = pc.accLatency;
    outpdw(PLIC_LAT_RESULT_ADDR, g_latency);
}

void PLIC_latency(void) {
    host_set_bp(BP_TEST_begin);
    while (g_latency == 0) {
        uint32_t inj = inpdw(IRQ_INJECT_ADDR);
        if (inj & (1u << PLIC_IRQ_ID)) { outpdw(IRQ_INJECT_ADDR, 0); isr(); }
    }
    host_set_bp(BP_TEST_end);
}

int main(void) { PLIC_latency(); return CODE_SUCCESS; }
