// verif_rtdev.h - rt-dev backend adapter (connects the company original verification engine)
//
// rt-dev = verification engine of the company original Verification_SDK (Verilator-based own flow).
//   Original structure (per import_test.py / common.cc):
//     - emulator.h/interface.h provide master_write/master_read/tick (primitives)
//     - common.cc implements host_set_bp/wait_bp/check_bp on top
//     - import_test.py generates the run_test_sequence(testname) dispatcher
//
//   Key: rt-dev primitives (master_*/tick) have "exactly the same structure" as our seam.
//      So the adapter is a thin layer bridging rt-dev functions to seam methods.
//      host_*bp is implemented on master_*+tick on both sides, so it is automatically compatible.
//
// Depends: VERIF/include/verif_primitives.h (seam)

#ifndef VERIF_RTDEV_H
#define VERIF_RTDEV_H

#include <cstdint>
#include <cstdio>
#include <string>

#ifndef VERIF_TEST_ADDR
#define VERIF_TEST_ADDR   0x00050004u
#endif
#ifndef VERIF_BREAK_ADDR
#define VERIF_BREAK_ADDR  0x00050000u
#endif
#include "verif_primitives.h"   // seam

// -- rt-dev original engine primitives (provided by the company environment) --
// When building in the company environment, emulator.h/interface.h provide these symbols.
// Unified as rtdev_* wrappers to avoid name clashes (only these 4 need wiring to the original).
extern "C" {
  bool rtdev_master_write(uint32_t addr, uint32_t data);
  bool rtdev_master_read (uint32_t addr, uint32_t* data);
  void rtdev_tick        (int n);
  void rtdev_trigger_irq (uint32_t id);   // rt-dev implements this in the original
}

// -- adapter: rt-dev primitive -> seam --
class verif_rtdev_t : public verif_primitives_t {
public:
  explicit verif_rtdev_t(const std::string& test = "") : test_(test) {}

  bool master_write(uint32_t addr, uint32_t data) override {
    return rtdev_master_write(addr, data);
  }
  bool master_read(uint32_t addr, uint32_t& data) override {
    return rtdev_master_read(addr, &data);
  }
  // rt-dev implements trigger_irq in the original (no chipyard stage-3 limitation).
  //   = co-sim (PLIC/latency, PLIC/enable) can be verified directly with rt-dev.
  void trigger_irq(uint32_t id) override {
    rtdev_trigger_irq(id);
  }
  void tick(int n) override {
    rtdev_tick(n);
  }
  // host_set/wait/check_bp use the verif_primitives_t default impl (on master_*+tick).
  //   -> same pattern as rt-dev common.cc, so results match.

  // -- CA extension -- rt-dev = Verilator-based RTL -> cycle-accurate.
  uint64_t get_cycle() override {
    uint32_t lo = 0, hi = 0;
    rtdev_master_read(0x00050008u, &lo);
    rtdev_master_read(0x0005000Cu, &hi);
    return ((uint64_t)hi << 32) | lo;
  }
  bool supports_ca() override { return true; }

  void set_test(const std::string& t) { test_ = t; }
  const std::string& test() const { return test_; }

private:
  std::string test_;
};

#endif // VERIF_RTDEV_H
