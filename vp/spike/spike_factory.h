// spike_factory.h - the sim_t construction factory (isolates version differences)
//   In SPIKE_NATIVE builds this constructs a sim_t using the installed riscv-isa-sim.
//   The ctor signature differs per version, so only this one place needs adapting to the environment.
//
//   The current implementation targets the signature of the riscv-isa-sim
//     (2024+ master line) installed in <chipyard tree>/.conda-env/riscv-tools:
//       sim_t(cfg, halted, mems, plugin_device_factories(sargs), args,
//             dm_config, log_path, dtb_enabled, dtb_file,
//             socket_enabled, cmd_file, instruction_limit)
//     - make_mems() is not exposed by the library, so mem_t is constructed directly.
//     - sim_t "keeps cfg as a pointer", so the cfg and isa strings must be static
//       and outlive sim (to avoid dangling references).
//     - ELF loading is performed by htif_t::start() (public) - in our structure we
//       do not use run() but drive proc->step() directly, so start() is called explicitly.
#ifndef SPIKE_FACTORY_H
#define SPIKE_FACTORY_H
#include <memory>
#include <string>
#include <vector>
#if defined(SPIKE_NATIVE)
#include <riscv/sim.h>
#include <riscv/cfg.h>
#include <riscv/devices.h>

static inline std::unique_ptr<sim_t> make_spike_sim(const std::string& isa,
                                                    const std::string& elf) {
  // sim_t references cfg/isa/priv by pointer -> keep them static so they outlive sim.
  static std::string s_isa;
  static cfg_t s_cfg;
  s_isa = isa;
  s_cfg.isa  = s_isa.c_str();
  s_cfg.priv = "M";                       // the firmware is M-mode only (crt.S)
  // Memory: DRAM @0x80000000, 256MB (identical to the chipyard RV32RocketConfig DTS).
  //   mem_t is sparse, so only the pages actually used are allocated.
  //   (riscv/platform.h already defines a DRAM_BASE macro, hence the VP_ prefix)
  const reg_t VP_DRAM_BASE = 0x80000000u, VP_DRAM_SIZE = 0x10000000u;
  s_cfg.mem_layout.clear();
  s_cfg.mem_layout.push_back(mem_cfg_t(VP_DRAM_BASE, VP_DRAM_SIZE));

  std::vector<std::pair<reg_t, abstract_mem_t*>> mems;
  mems.emplace_back(VP_DRAM_BASE, new mem_t(VP_DRAM_SIZE));  // kept alive for sim's lifetime
  std::vector<device_factory_sargs_t> plugins;   // none
  std::vector<std::string> htif_args{ elf };     // htif loads the ELF

  auto s = std::make_unique<sim_t>(&s_cfg, /*halted=*/false, mems, plugins,
                                   htif_args, debug_module_config_t{},
                                   /*log_path=*/nullptr,
                                   /*dtb_enabled=*/true, /*dtb_file=*/nullptr,
                                   /*socket_enabled=*/false,
                                   /*cmd_file=*/nullptr,
                                   /*instruction_limit=*/std::nullopt);
  // Since we drive proc->step() directly instead of run(), ELF loading + reset
  // (htif start) is performed explicitly here. (run() would spin its own event loop)
  s->start();
  return s;
}
#endif
#endif // SPIKE_FACTORY_H
