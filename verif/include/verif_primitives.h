// verif_primitives.h - VERIF host primitive interface (= framework<->backend seam)
//
// This header is "the interface the framework knows". VERIF tests (Vseq) know only this.
// Backends (chipyard/rt_dev/...) each implement this interface.
//   - chipyard impl = verif_tsi_t in backend/chipyard/verif_host.h
//   - rt_dev impl   = (company existing) tl_master DPI  [not included, placeholder]
//
// Signatures follow S5740_mcu_tests.h in the Verification_SDK (MCU_SDK_RTDEV_GUIDE section 3.2).
// A backend adapter inherits/implements this abstract class, or provides methods with the same signature.
#ifndef VERIF_PRIMITIVES_H
#define VERIF_PRIMITIVES_H

#include <cstdint>

// rt_dev BP agreed values (per S5740_mcu_tests.h; replace when the real header is available)
enum verif_bp_t {
  BP_MEM_WRITE    = 0xFFFFFFF0,
  BP_TEST_init    = 0xFFFFFFF1,
  BP_TEST_begin   = 0xFFFFFFF2,
  BP_TEST_success = 0xFFFFFFF3,
  BP_TEST_fail    = 0xFFFFFFF4,
  BP_TEST_end     = 0xFFFFFFF5,
  BP_END          = 0xFFFFFFFF,
};

// Test address agreed values - NOTE: DUT memory-map dependent (backend overrides).
//   Below are the S5740 RTL values (observed in PLIC_latency; MCU_SDK_RTDEV_GUIDE section 3.3).
//   The chipyard backend redefines them to the DRAM region (see backend/chipyard/verif_host.h).
//   BP_* numbers (enum above) are protocol, common regardless of DUT.
#ifndef VERIF_TEST_ADDR
#define VERIF_TEST_ADDR   0x50000u
#endif
#ifndef VERIF_BREAK_ADDR
#define VERIF_BREAK_ADDR  0x51000u
#endif

// -- host primitive interface (seam) --
// A backend implements these 6. (host_*bp is built on master_*+tick)
class verif_primitives_t {
public:
  virtual ~verif_primitives_t() {}

  // base primitives (backend implements)
  virtual bool master_write(uint32_t addr, uint32_t data) = 0;
  virtual bool master_read (uint32_t addr, uint32_t& data) = 0;
  virtual void trigger_irq (uint32_t mask) = 0;   // stage 3 (may be unimplemented on chipyard)
  virtual void tick        (int n) = 0;

  // higher-level helpers (default impl on master_*+tick; backend may override)
  virtual void host_set_bp(uint32_t bp_num) {
    master_write(VERIF_BREAK_ADDR, bp_num);
  }
  virtual bool host_wait_bp(uint32_t bp_num) {
    uint32_t v;
    while (1) {
      if (!master_read(VERIF_BREAK_ADDR, v)) return false;
      if (v == bp_num) return true;
      tick(1);
    }
  }
  virtual bool host_check_bp(uint32_t addr, uint32_t expect) {
    uint32_t v; if (!master_read(addr, v)) return false; return v == expect;
  }

  // -- CA (cycle-accurate) extension --
  // For CA verification ("how many cycles"). Not needed for functional-only (default provided).
  //   get_cycle()  : current cycle counter. Meaningful only on CA backends.
  //   supports_ca(): whether this backend is cycle-accurate. CA tests use it to decide SKIP.
  //     - chipyard(RTL/Verilator): true  (accurate cycles)
  //     - spike(functional ISS)   : false (inaccurate cycles -> CA SKIP)
  //     - vp/spike-ca(timing model): true  (cycles from a SW timing model)
  virtual uint64_t get_cycle() { return 0; }     // default=0 (non-CA backend)
  virtual bool     supports_ca() { return false; }
};

// -- CA measurement struct (host side) --
// Judge measured cycles of [cycle_start, cycle_end] against expected +/- tolerance.
struct ca_measurement_t {
  uint64_t    cycle_start = 0;   // measurement start cycle
  uint64_t    cycle_end   = 0;   // measurement end cycle
  uint64_t    measured    = 0;   // measured = end - start (or PMU value)
  uint64_t    expected    = 0;   // expected (cycles)
  uint64_t    tolerance   = 0;   // tolerance (+/- cycles)
  const char* name        = "";  // measurement item name
};

// CA judgment: |measured - expected| <= tolerance  (range check, both directions)
static inline bool ca_pass(const ca_measurement_t& m) {
  uint64_t diff = (m.measured > m.expected) ? (m.measured - m.expected)
                                            : (m.expected - m.measured);
  return diff <= m.tolerance;
}

#endif // VERIF_PRIMITIVES_H
