// PLIC_latency.cc - host sequence (CA)
//   CA flow: host_wait_bp(begin) -> trigger_irq -> host_wait_bp(end) -> read the PMU latency.
//
//   Unlike the CLINT/PLIC_enable neighbours this file had no company original body - only a
//   note describing the shape. The sequence below is the one that used to sit inline in
//   verif_host.h::run_vseq; it was moved here in the v304 refactor so that verif_host.h is
//   dispatch only. Replace it wholesale if the company original ever arrives.
//
//   The standalone CA measurement (host-side cycle counting around the injection) is done by
//   main in backend/<B>/verif_ca_run.cc; this is the functional-path entry point.
#include "S5740_mcu_tests.h"
#include "PLIC_latency.h"

bool PLIC_latency()
{
#ifndef PRELOAD
    host_set_bp(BP_MEM_WRITE);
#endif
    if (host_wait_bp(BP_TEST_begin)) return false;   // wait for the DUT to be ready
    trigger_irq(1u << PLIC_IRQ_ID);                  // inject the IRQ
    if (host_wait_bp(BP_TEST_end)) {
        host_set_bp(BP_TEST_end);                    // release the DUT before reporting failure
        return false;
    }
    unsigned int lat = 0;
    if (!master_read(PLIC_LAT_RESULT_ADDR, lat)) return false;
    fprintf(stderr, "[Debug] PLIC latency = %u cycles\n", lat);
    host_set_bp(BP_TEST_end);
    return lat != 0;                                 // 0 means the ISR never wrote a measurement
}

bool PLIC_latency_check()
{
    return true;
}
