// ============================================================
// FAKE implementation - for local build testing only
// ============================================================
// A stub for environments where the real SimExtInterrupts DPI (implemented by
// the hardware team) is absent from chipyard, used only to confirm that the
// chip_platform side (verif_host.h) at least compiles and links.
// It does not drive the RTL interrupt lines at all (it is not real behaviour).
//
// WARNING: never use this file in an environment that has the hardware team's
//    real SimExtInterrupts (e.g. the company machines / dscloud) - the function
//    names collide with the real implementation and produce "duplicate
//    definition" link errors.
#ifndef TESTCHIP_EXTERNAL_INTERRUPTS_H
#define TESTCHIP_EXTERNAL_INTERRUPTS_H

#include <cstdint>
#include <cstdio>

// Declared static inline so including the header from several .cc files does
// not produce duplicate definitions.
// (The real implementation is a separate .cc file, but the fake one is
//  self-contained in a single header so it builds without Chisel
//  addResource/HarnessBinder.)

static inline void sim_ext_intr_trigger(int chip_id, int interrupt_mask) {
    // FAKE: no effect on the actual RTL. Only the fact that it was called is logged.
    fprintf(stderr, "[FAKE sim_ext_intr_trigger] chip_id=%d mask=0x%x (not real hardware; local build testing only)\n",
            chip_id, interrupt_mask);
}

static inline int sim_ext_intr_tick(int* intr_value, int n_interrupts, int chip_id) {
    if (intr_value) *intr_value = 0;   // always 0 (no interrupt is actually raised)
    return 0;
}

static inline void sim_ext_intr_set_pulse_width(int width) {
    // FAKE: no-op
}

#endif // TESTCHIP_EXTERNAL_INTERRUPTS_H
