// timing_model.h - the CA timing model (instruction -> cycles)
//
// Design principle: decoupled from the "instruction source".
//   This model takes only the RISC-V instruction bits plus the execution
//   context handed to it by the source, and answers "how many cycles". Whether
//   the instruction came from spike or from a pure C interpreter is irrelevant
//   -> the source can be swapped later.
//   Where actual register values (XPR) are needed (load/store addresses), the
//   source computes them and passes them in via insn_ctx_t.mem_addr - the model
//   knows nothing about the simulator type.
//
// -- Phase history --
//   Phase 1 (cycle-approximate): cost(insn) = a fixed penalty per instruction
//     class. No relationship between adjacent instructions -> it produced
//     LDUSE==LDINDEP, BRTAKEN==BRNTAKN, DHIT==DMISS, diverging in trend from
//     RTL.
//   Phase 2 (this file): reflects the "previous instruction" and the "execution
//     context".
//     (1) Load-Use stall  : previous was a load and this instruction uses its rd
//                           as a source -> +1
//     (2) Branch taken    : next_pc != pc+len (the real branch direction) -> +1
//                           (front-end bubble)
//     (3) MulDiv          : separate multi-cycle mul/div + (hook) result
//                           dependency penalty
//     (4) Address-aware memory : per-region latency using the real access
//                           address supplied by the source + a D$ model
//        (the execution-driven advantage - addresses are computed from live
//         register values without a commit log)
//
// -- Where the penalty constants come from --
//   All are approximations, calibrated against RTL (Verilator,
//   RV32RocketConfig) measurements.
//   Basis: the CONTROLLED PAIRS in chip_docs/verif/docs/CA_measure.md
//   (build/logs/CA_measure.log)
//     load-use interlock  +1.01 cyc/event -> load_use_pen = 1
//     taken-branch bubble +1.08 cyc/event -> br_taken_pen = 1
//                                            (initially proposed +2 -> corrected
//                                             against measurement)
//     L1+L2 cache miss   +26.88 cyc/event -> dram_miss_pen = 27
//     mul 5.92 cyc/iter -> mul_pen = 5,  divu 34.36 -> div_pen = 33,
//     csrr 2.91 -> system_pen = 2
//   NOTE: the rest (tcm/sram/clint/periph/trap) are still to be validated
//   against RTL measurements.

#ifndef TIMING_MODEL_H
#define TIMING_MODEL_H

#include <cstdint>
#include <cstring>

// -- RISC-V instruction classification/decode (RV32IMAC) --
//   Pure bit functions only - no access to simulator state (keeps it source independent).
namespace rvclass {

  // Standard 32-bit instruction opcodes (low 7 bits)
  enum : uint32_t {
    OP_LOAD     = 0x03,   // lw, lh, lb ...
    OP_STORE    = 0x23,   // sw, sh, sb
    OP_BRANCH   = 0x63,   // beq, bne, blt ...
    OP_JAL      = 0x6F,   // jal
    OP_JALR     = 0x67,   // jalr
    OP_OP       = 0x33,   // add, sub, and, ... (R-type; includes M-extension mul/div)
    OP_OPIMM    = 0x13,   // addi, andi ...
    OP_SYSTEM   = 0x73,   // csr, ecall ...
    OP_AMO      = 0x2F,   // atomic (A extension)
  };

  // Is the instruction compressed (16-bit)? If the low 2 bits are not 11, it is compressed (RVC).
  static inline bool is_compressed(uint32_t insn) {
    return (insn & 0x3u) != 0x3u;
  }
  static inline uint32_t insn_len(uint32_t insn) {
    return is_compressed(insn) ? 2u : 4u;
  }

  // ---- RVC fields (C extension, RV32) ----
  static inline uint32_t c_funct3(uint32_t i) { return (i >> 13) & 0x7u; }
  static inline uint32_t c_rs1c  (uint32_t i) { return 8u + ((i >> 7) & 0x7u); }  // rs1'
  static inline uint32_t c_rs2c  (uint32_t i) { return 8u + ((i >> 2) & 0x7u); }  // rs2'
  static inline uint32_t c_rd    (uint32_t i) { return (i >> 7) & 0x1Fu; }        // full rd
  static inline uint32_t c_rs2   (uint32_t i) { return (i >> 2) & 0x1Fu; }        // full rs2

  // Load detection (rv32imac: 32-bit LOAD + C.LW + C.LWSP)
  static inline bool is_load(uint32_t insn) {
    if (!is_compressed(insn)) return (insn & 0x7Fu) == OP_LOAD;
    uint32_t q = insn & 0x3u, f3 = c_funct3(insn);
    return (q == 0 && f3 == 2) ||     // C.LW
           (q == 2 && f3 == 2);       // C.LWSP
  }
  static inline bool is_store(uint32_t insn) {
    if (!is_compressed(insn)) return (insn & 0x7Fu) == OP_STORE;
    uint32_t q = insn & 0x3u, f3 = c_funct3(insn);
    return (q == 0 && f3 == 6) ||     // C.SW
           (q == 2 && f3 == 6);       // C.SWSP
  }
  static inline bool is_amo  (uint32_t insn) {
    return !is_compressed(insn) && (insn & 0x7Fu) == OP_AMO;
  }
  static inline bool is_branch(uint32_t insn){
    if (!is_compressed(insn)) return (insn & 0x7Fu) == OP_BRANCH;
    uint32_t q = insn & 0x3u, f3 = c_funct3(insn);
    return q == 1 && (f3 == 6 || f3 == 7);   // C.BEQZ / C.BNEZ
  }
  static inline bool is_jump (uint32_t insn) {
    if (!is_compressed(insn)) {
      uint32_t op = insn & 0x7Fu; return op == OP_JAL || op == OP_JALR;
    }
    uint32_t q = insn & 0x3u, f3 = c_funct3(insn);
    if (q == 1 && (f3 == 1 || f3 == 5)) return true;              // C.JAL / C.J
    if (q == 2 && f3 == 4 && c_rs2(insn) == 0 && c_rd(insn) != 0)
      return true;                                                 // C.JR / C.JALR
    return false;
  }
  static inline bool is_system(uint32_t insn){
    return !is_compressed(insn) && (insn & 0x7Fu) == OP_SYSTEM;
  }

  // M extension (funct7 == 0x01). funct3 separates the mul family from the div family.
  static inline bool is_muldiv(uint32_t insn) {
    if (is_compressed(insn)) return false;
    uint32_t op = insn & 0x7Fu, funct7 = (insn >> 25) & 0x7Fu;
    return (op == OP_OP) && (funct7 == 0x01u);
  }
  static inline bool is_div(uint32_t insn) {            // div/divu/rem/remu
    return is_muldiv(insn) && (((insn >> 12) & 0x7u) >= 4u);
  }
  static inline bool is_mul(uint32_t insn) {            // mul/mulh/mulhsu/mulhu
    return is_muldiv(insn) && (((insn >> 12) & 0x7u) < 4u);
  }

  // ---- Destination register (for tracking load results; 0=x0 when absent) ----
  static inline uint32_t dest_reg(uint32_t insn) {
    if (!is_compressed(insn)) {
      uint32_t op = insn & 0x7Fu;
      switch (op) {
        case OP_LOAD: case OP_OP: case OP_OPIMM: case OP_JAL: case OP_JALR:
        case OP_AMO:  case 0x37 /*LUI*/: case 0x17 /*AUIPC*/: case OP_SYSTEM:
          return (insn >> 7) & 0x1Fu;
        default: return 0;
      }
    }
    uint32_t q = insn & 0x3u, f3 = c_funct3(insn);
    if (q == 0 && f3 == 2) return c_rs2c(insn);              // C.LW -> rd'
    if (q == 2 && f3 == 2) return c_rd(insn);                // C.LWSP -> rd
    if (q == 1 && (f3 == 0 || f3 == 2 || f3 == 3)) return c_rd(insn); // C.ADDI/LI/LUI
    if (q == 1 && f3 == 4) return c_rs1c(insn);              // C.SRLI/SRAI/ANDI/SUB..
    if (q == 2 && (f3 == 0 || f3 == 4)) return c_rd(insn);   // C.SLLI / C.MV / C.ADD
    return 0;
  }

  // ---- Source registers (up to 2; empty slots are 0=x0) ----
  //   For load-use detection: "does this instruction read register r as a source".
  struct src_regs_t { uint32_t rs1, rs2; };
  static inline src_regs_t src_regs(uint32_t insn) {
    src_regs_t s{0, 0};
    if (!is_compressed(insn)) {
      uint32_t op = insn & 0x7Fu, f3 = (insn >> 12) & 0x7u;
      uint32_t rs1 = (insn >> 15) & 0x1Fu, rs2 = (insn >> 20) & 0x1Fu;
      switch (op) {
        case OP_OP: case OP_STORE: case OP_BRANCH: case OP_AMO:
          s.rs1 = rs1; s.rs2 = rs2; break;
        case OP_OPIMM: case OP_LOAD: case OP_JALR:
          s.rs1 = rs1; break;
        case OP_SYSTEM:
          if (f3 >= 1 && f3 <= 3) s.rs1 = rs1;   // csrrw/s/c (the imm forms have no source)
          break;
        default: break;                          // LUI/AUIPC/JAL/FENCE
      }
      return s;
    }
    // RVC (only the forms that appear on rv32imac)
    uint32_t q = insn & 0x3u, f3 = c_funct3(insn);
    if (q == 0) {
      if (f3 == 0) s.rs1 = 2;                                  // C.ADDI4SPN (x2)
      else if (f3 == 2) s.rs1 = c_rs1c(insn);                  // C.LW
      else if (f3 == 6) { s.rs1 = c_rs1c(insn); s.rs2 = c_rs2c(insn); } // C.SW
    } else if (q == 1) {
      if (f3 == 0) s.rs1 = c_rd(insn);                         // C.ADDI
      else if (f3 == 3 && c_rd(insn) == 2) s.rs1 = 2;          // C.ADDI16SP
      else if (f3 == 4) {
        s.rs1 = c_rs1c(insn);                                  // C.SRLI/SRAI/ANDI
        if (((insn >> 10) & 0x3u) == 3) s.rs2 = c_rs2c(insn);  // C.SUB/XOR/OR/AND
      }
      else if (f3 == 6 || f3 == 7) s.rs1 = c_rs1c(insn);       // C.BEQZ/BNEZ
    } else if (q == 2) {
      if (f3 == 0) s.rs1 = c_rd(insn);                         // C.SLLI
      else if (f3 == 2) s.rs1 = 2;                             // C.LWSP (x2)
      else if (f3 == 4) {
        uint32_t rs2 = c_rs2(insn), rd = c_rd(insn), b12 = (insn >> 12) & 1u;
        if (rs2 != 0) { s.rs1 = rs2; if (b12 && rd) s.rs2 = rd; } // C.MV / C.ADD
        else if (rd != 0) s.rs1 = rd;                             // C.JR / C.JALR
      }
      else if (f3 == 6) { s.rs1 = 2; s.rs2 = c_rs2(insn); }    // C.SWSP
    }
    return s;
  }

  static inline bool uses_reg(uint32_t insn, uint32_t r) {
    if (r == 0) return false;                    // x0 carries no dependency
    src_regs_t s = src_regs(insn);
    return s.rs1 == r || s.rs2 == r;
  }

  // ---- Ingredients for computing the memory access address (base reg number + offset) ----
  //   The source reads the "real value" of the base register and forms addr = XPR[base]+offset.
  //   This is the execution-driven advantage: live register values can be read
  //     directly, so the access address is known exactly without a commit log.
  struct mem_ref_t { bool valid; uint32_t base; int32_t offset; };
  static inline mem_ref_t mem_ref(uint32_t insn) {
    mem_ref_t m{false, 0, 0};
    if (!is_compressed(insn)) {
      uint32_t op = insn & 0x7Fu;
      if (op == OP_LOAD || op == OP_JALR) {
        m.valid = (op == OP_LOAD);
        m.base = (insn >> 15) & 0x1Fu;
        m.offset = (int32_t)insn >> 20;                        // I-imm (sign extended)
      } else if (op == OP_STORE) {
        m.valid = true;
        m.base = (insn >> 15) & 0x1Fu;
        m.offset = (int32_t)(((insn >> 25) << 5) | ((insn >> 7) & 0x1Fu));
        m.offset = (m.offset << 20) >> 20;                     // S-imm sign extension
      } else if (op == OP_AMO) {
        m.valid = true; m.base = (insn >> 15) & 0x1Fu; m.offset = 0;
      }
      return m;
    }
    uint32_t q = insn & 0x3u, f3 = c_funct3(insn);
    if (q == 0 && (f3 == 2 || f3 == 6)) {                      // C.LW / C.SW
      m.valid = true; m.base = c_rs1c(insn);
      m.offset = (int32_t)((((insn >> 6) & 1u) << 2) |
                           (((insn >> 10) & 0x7u) << 3) |
                           (((insn >> 5) & 1u) << 6));
    } else if (q == 2 && f3 == 2) {                            // C.LWSP
      m.valid = true; m.base = 2;
      m.offset = (int32_t)((((insn >> 4) & 0x7u) << 2) |
                           (((insn >> 12) & 1u) << 5) |
                           (((insn >> 2) & 0x3u) << 6));
    } else if (q == 2 && f3 == 6) {                            // C.SWSP
      m.valid = true; m.base = 2;
      m.offset = (int32_t)((((insn >> 9) & 0xFu) << 2) |
                           (((insn >> 7) & 0x3u) << 6));
    }
    return m;
  }
}

// -- Execution context (source -> model) --
//   The source (verif_spike.h etc.) fills this in and passes it for each executed instruction.
//   pc/next_pc  : PC before/after execution. taken = next_pc != pc+len.
//   mem_addr    : the real access address of a load/store/amo (real base register value + offset).
//                 If the source cannot supply it, has_mem_addr=false -> falls back to a flat penalty.
struct insn_ctx_t {
  uint32_t insn        = 0x13;    // nop
  uint64_t pc          = 0;
  uint64_t next_pc     = 0;
  bool     has_mem_addr= false;
  uint64_t mem_addr    = 0;
};

// -- Memory map (per chip_docs/verif/docs/memory_layout.md and the chipyard RV32RocketConfig DTS) --
//   CLINT 0x02000000, peripherals (UART etc.) 0x10000000~, DRAM 0x80000000~.
//   Close/TCM assumes a low-address region such as the S5740 SDK link area (0x50000 range).
namespace memmap {
  enum : uint64_t {
    CLINT_BASE  = 0x02000000ull, CLINT_END  = 0x02FFFFFFull,
    SRAM_BASE   = 0x08000000ull, SRAM_END   = 0x0FFFFFFFull,   // scratchpad-like
    PERIPH_BASE = 0x10000000ull, PERIPH_END = 0x1FFFFFFFull,   // UART 0x10020000 etc.
    DRAM_BASE   = 0x80000000ull,
  };
}

// -- Timing cost parameters (Phase 2: calibrated against RTL measurements) --
//   All are approximate constants, still to be validated against RTL. See the file header comment for the basis.
struct timing_params_t {
  uint32_t base          = 1;   // baseline IPC=1 (in-order, single issue)
  // Relationship penalties (new in Phase 2)
  uint32_t load_use_pen  = 1;   // interlock when a load result is used immediately  [RTL +1.01]
  uint32_t br_taken_pen  = 1;   // taken-branch bubble. Initially proposed +2 (flush) ->
                                //   corrected to the RTL controlled pair +1.08  [RTL +1.08]
  uint32_t jump_pen      = 1;   // jal/jalr redirect (always taken)
  uint32_t muldiv_use_pen= 0;   // extra cost when a muldiv result is used immediately.
                                //   RTL measurement gives MULINDP ~= MULDEP (the Rocket
                                //   MulDiv is iterative, so independence does not overlap) -> 0. The hook is kept.
  // Multi-cycle operations
  uint32_t mul_pen       = 5;   // 6 cyc total  [RTL 5.92 cyc/mul]
  uint32_t div_pen       = 33;  // 34 cyc total [RTL 34.36 cyc/div]
  uint32_t amo_pen       = 2;   // atomic (not measured on RTL - to be validated)
  uint32_t system_pen    = 2;   // csr, 3 cyc total [RTL 2.91]
  // Address-aware memory latency (new in Phase 2; added on top of base)
  uint32_t tcm_pen       = 0;   // close/TCM: 1 total  (to be validated)
  uint32_t sram_pen      = 2;   // SRAM: 3 total       (to be validated)
  uint32_t clint_pen     = 19;  // CLINT: 20+ total    (to be validated)
  uint32_t periph_pen    = 19;  // UART etc.: 20+ total (to be validated)
  uint32_t dram_hit_pen  = 0;   // DRAM D$ hit: 1 total  [RTL DHIT CPI 0.979]
  uint32_t dram_miss_pen = 27;  // DRAM D$ miss: 28 total [RTL pair +26.88]
  uint32_t mem_flat_pen  = 1;   // fallback when no address information is available (the Phase 1 value)
  // Trap entry (pipeline flush + redirect)
  uint32_t trap_pen      = 4;   // approximate (to be validated against RTL)
};

// -- D$ model (Phase 2) --
//   RV32RocketConfig L1 D$: 32KB, 64B line, 64 set x 8 way (chip_docs/verif/docs/CA_measure.md).
//   A set-associative LRU model with the same geometry. L2 is not modelled
//   separately; it is folded into the miss penalty (dram_miss_pen = the measured
//   L1+L2 miss cost) - CA_measure's DMISS pattern thrashes L2 as well, so this
//   approximation matches measurement. (Limitation: the L2-hit/L1-miss range is overestimated - see the docs)
struct dcache_model_t {
  static const uint32_t SETS = 64, WAYS = 8, LINE_SHIFT = 6;   // 64B line
  uint64_t tag[SETS][WAYS];
  uint32_t age[SETS][WAYS];        // LRU: larger = more recent
  uint32_t clk = 0;
  bool     valid[SETS][WAYS];

  dcache_model_t() { std::memset(valid, 0, sizeof(valid)); }

  // One access: returns whether it hit and updates LRU (on a miss, replaces the LRU way = write-allocate)
  bool access(uint64_t addr) {
    uint64_t line = addr >> LINE_SHIFT;
    uint32_t set  = (uint32_t)(line % SETS);
    uint64_t t    = line / SETS;
    ++clk;
    for (uint32_t w = 0; w < WAYS; w++)
      if (valid[set][w] && tag[set][w] == t) { age[set][w] = clk; return true; }
    uint32_t victim = 0;
    for (uint32_t w = 0; w < WAYS; w++) {
      if (!valid[set][w]) { victim = w; break; }
      if (age[set][w] < age[set][victim]) victim = w;
    }
    valid[set][victim] = true; tag[set][victim] = t; age[set][victim] = clk;
    return false;
  }
  void reset() { std::memset(valid, 0, sizeof(valid)); clk = 0; }
};

// -- The timing model proper (Phase 2) --
//   account(ctx): accounts for one instruction. The "previous instruction state"
//   (prev_*) is kept as members - execution is sequential, so remembering just the previous one suffices for load-use / muldiv-use detection.
struct timing_model_t {
  timing_params_t p;
  uint64_t total = 0;            // accumulated cycles
  dcache_model_t dcache;
  int phase = 2;                 // 1 = Phase 1 regression (for A/B comparison), 2 = this model

  // Previous instruction state (the core of Phase 2)
  bool     prev_valid  = false;
  bool     prev_load   = false;  // was the previous instruction a load
  bool     prev_muldiv = false;  // was the previous instruction a mul/div
  uint32_t prev_rd     = 0;      // destination register of the previous instruction

  // Statistics (for documentation/debugging)
  uint64_t n_insn = 0, n_load_use = 0, n_br_taken = 0, n_dmiss = 0, n_trap = 0;

  // Extra cycles for a memory access (per address region; the D$ applies to DRAM only)
  uint32_t mem_cost(uint64_t addr) {
    using namespace memmap;
    if (addr >= DRAM_BASE) {
      if (dcache.access(addr)) return p.dram_hit_pen;
      n_dmiss++;
      return p.dram_miss_pen;
    }
    if (addr >= CLINT_BASE  && addr <= CLINT_END)  return p.clint_pen;
    if (addr >= SRAM_BASE   && addr <= SRAM_END)   return p.sram_pen;
    if (addr >= PERIPH_BASE && addr <= PERIPH_END) return p.periph_pen;
    return p.tcm_pen;                       // low-address close/TCM
  }

  // Phase 1 cost table (retained for A/B comparison: a fixed penalty per instruction class, with no relationship or address awareness)
  uint32_t cost_phase1(uint32_t insn) const {
    using namespace rvclass;
    uint32_t c = p.base;
    if      (is_load(insn))   c += 1;   // old load_pen
    else if (is_store(insn))  c += 1;   // old store_pen
    else if (is_amo(insn))    c += 2;   // old amo_pen
    else if (is_branch(insn)) c += 1;   // old branch_pen (independent of taken!)
    else if (is_jump(insn))   c += 1;   // old jump_pen
    else if (is_system(insn)) c += 1;   // old system_pen
    else if (is_muldiv(insn)) c += 3;   // old muldiv_pen (mul and div identical!)
    return c;
  }

  // Cycle cost of one instruction (reflecting the previous instruction state + execution context)
  uint32_t cost(const insn_ctx_t& c) {
    if (phase == 1) return cost_phase1(c.insn);
    using namespace rvclass;
    uint32_t insn = c.insn;
    uint32_t cyc = p.base;

    // (1) Load-Use stall: the previous instruction was a load and this one uses its rd as a source
    if (prev_valid && prev_load && uses_reg(insn, prev_rd))
      { cyc += p.load_use_pen; n_load_use++; }

    // (3') MulDiv result dependency (hook; 0 by default - RTL measurements show no extra cost because the unit is iterative)
    if (prev_valid && prev_muldiv && uses_reg(insn, prev_rd))
      cyc += p.muldiv_use_pen;

    // Cost per instruction group
    if (is_load(insn) || is_store(insn) || is_amo(insn)) {
      // (4) Address-aware memory latency (only when the source supplied a real address; otherwise a flat fallback)
      cyc += c.has_mem_addr ? mem_cost(c.mem_addr) : p.mem_flat_pen;
      if (is_amo(insn)) cyc += p.amo_pen;
    }
    else if (is_branch(insn)) {
      // (2) Branch taken: the real direction is determined from the PC before and after (taken if it is not the next sequential address)
      if (c.next_pc != c.pc + insn_len(insn)) { cyc += p.br_taken_pen; n_br_taken++; }
    }
    else if (is_jump(insn))   cyc += p.jump_pen;     // always redirects
    else if (is_div(insn))    cyc += p.div_pen;      // (3) multi-cycle div
    else if (is_mul(insn))    cyc += p.mul_pen;      // (3) multi-cycle mul
    else if (is_system(insn)) cyc += p.system_pen;
    // Everything else (ALU/immediate/LUI/AUIPC) costs only base (forwarding is assumed, so a dependency chain is also 1)
    return cyc;
  }

  // Account one executed instruction into the model (accumulate + refresh the previous instruction state)
  uint32_t account(const insn_ctx_t& c) {
    uint32_t cyc = cost(c);
    total += cyc;
    n_insn++;
    // Refresh the previous instruction state - execution-driven is sequential, so this single set suffices
    prev_valid  = true;
    prev_load   = rvclass::is_load(c.insn);
    prev_muldiv = rvclass::is_muldiv(c.insn);
    prev_rd     = rvclass::dest_reg(c.insn);
    return cyc;
  }

  // Phase 1 compatibility (for sources without context - operates with flat penalties only)
  uint32_t account(uint32_t insn) {
    insn_ctx_t c; c.insn = insn; c.pc = 0; c.next_pc = rvclass::insn_len(insn);
    return account(c);
  }

  // Trap/interrupt entry penalty (pipeline flush + mtvec redirect).
  //   The source calls this when it detects "this step took a trap instead of committing an instruction".
  //   (verif_spike.h: minstret unchanged across the step = treated as trap entry)
  void account_trap() {
    total += p.trap_pen;
    n_trap++;
    prev_valid = false;          // pipeline flush -> the link to the previous instruction is broken
  }

  uint64_t cycles() const { return total; }
  void reset() {
    total = 0; prev_valid = false; prev_load = false; prev_muldiv = false;
    prev_rd = 0; n_insn = n_load_use = n_br_taken = n_dmiss = n_trap = 0;
    dcache.reset();
  }
};

#endif // TIMING_MODEL_H
