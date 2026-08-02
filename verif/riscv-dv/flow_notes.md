# riscv-dv based RV32RocketConfig functional verification - background / troubleshooting log

> **v304 integration note** - the original is `riscv-dv-flow/README.md` in
> `$CY=~/chipyard-riscv-dv`.
> On being brought into chip_platform v304 it was renamed to
> `verif/riscv-dv/flow_notes.md`
> (the `README.md` in the same directory is a separate, newly written document
> that covers the v304 integration).
> For a summary of the procedure see [`README.md`](./README.md); for the
> command-oriented document see [`riscv_dv.md`](./riscv_dv.md).
> The path notation in this document (`$CY/riscv-dv-flow/...`) refers to the
> **original working copy**.

A random instruction program produced by riscv-dv is run on both **Rocket RTL
(Verilator)** and **Spike (ISS)**, and the architectural state (GPR writes) of
the two execution traces is compared.
Any mismatch is a candidate RTL bug.

```
riscv-dv pyflow --> .S --> riscv64-unknown-elf-gcc --> .elf
                                                        |--> spike         --> spike log  --+
                                                        `--> Verilator RTL --> rocket log --+
                                                                                            `-> trace CSV comparison
```

## What is installed

| Item | Path | Note |
|---|---|---|
| riscv-dv | `~/repo_riscv/riscv-dv` | commit `b7a0b4b0` |
| Python venv | `~/repo_riscv/riscv-dv-venv` | a **dedicated venv** to avoid polluting the conda env |
| Work/results directory | `~/repo_riscv/riscv-dv-work` | generated files, logs, CSVs |
| Toolchain / Spike | `$CY/.conda-env/riscv-tools` | gcc 13.2.0, spike 1.1.1-dev |
| RTL simulator | `$CY/sims/verilator/simulator-chipyard.harness-RV32RocketConfig` | **reuses the existing build** (no rebuild) |

`CY=<chipyard tree>` (a dedicated copy). Nothing is run in the
pristine a pristine chipyard tree. The script checks the `$CY` path
at startup and refuses otherwise.

## Running

```bash
cd $CY/riscv-dv-flow
./run_riscv_dv.sh                                     # default: arithmetic_basic x1
./run_riscv_dv.sh -t riscv_rand_instr_test -n 5       # choose test/iterations
./run_riscv_dv.sh -s 1234                             # fixed seed (reproducible)
./run_riscv_dv.sh -k 512                              # stack size in words
```

Result: `~/repo_riscv/riscv-dv-work/<test>/rtl_sim/compare_<i>.log`
-> `[PASSED]: N matched` or `[FAILED]: ... mismatch`.

## Current results

`riscv_arithmetic_basic_test` (10k instructions, seed 1000):

```
RTL:        *** PASSED *** Completed after 35136 simulation cycles
comparison: [PASSED]: 2622 matched          (0 mismatches)
```

Of these, **574 mul/div instructions** were compared down to the destination
register value. Before the commit log these did not appear in the trace at all,
so the M extension is only genuinely verified from now on.

```
80000128,mul,s10:e5c0de40,...  "mul  s10, a1, s4"
8000017e,div,ra:00000000,...   "div  ra, a5, t1"
```

`riscv_arithmetic_basic_test` uses `--no_data_page=1`, so there are no loads.

### Known limitation: `riscv_rand_instr_test` + RV32A generation failure

Running `riscv_rand_instr_test` with `rv32imac` dies about 40 seconds into the
**generation stage**:

```
riscv_load_store_instr_lib.py:117 post_randomize -> add_mixed_instr
  -> riscv_instr_stream.randomize_gpr -> vsc SolveFailure
```

The same test generates fine with the stock `rv32imc` target -> **RV32A is the
trigger**. `randomize_gpr` constrains rs1/rs2/rd to `avail_regs`, but the
directed load/store stream reserves registers for addressing, leaving a small
pool. Adding AMO (which needs 3: rs1/rs2/rd) makes the constraints unsolvable.
**This is a limitation of the pyflow generator and unrelated to the RTL.**

### Known limitation: pyflow's RV32A (AMO) generation is broken

`riscv_amo_test` was added to `target/rv32imac/testlist.yaml` in order to get
separate A coverage, but **the AMO stream library itself dies at generation
time**:

```
riscv_amo_instr_lib.py:41  self.avail_regs = vsc.randsz_list_t(vsc.enum_t(riscv_reg_t))
  -> vsc/rand_obj.py:140 __setattr__ -> vsc/types.py:1261 __next__
AttributeError: 'NoneType' object has no attribute 'size'
```

This is not a configuration problem but a **compatibility issue between riscv-dv
pyflow and pyvsc 0.9.5** (it blows up during object construction).
In summary, with this version combination both RV32A paths are blocked:

| Path | Result |
|---|---|
| mixing AMO into a random stream (`riscv_rand_instr_test`) | `SolveFailure` (register pool exhausted) |
| a dedicated AMO stream (`riscv_amo_test`) | `AttributeError` (pyvsc incompatibility) |

**The verified scope is effectively RV32IMC. A has not been verified yet.**
The `rv32imac` target and core settings exist and `misa` does report IMAC, but no
A instructions are actually generated. To cover A, either try downgrading pyvsc
(e.g. to a combination riscv-dv has validated) or write the AMO test by hand.

### Known limitation: the load/store stream generates an unassemblable `c.sw`

Trying to cover the load path, a small test was generated with
`riscv_load_store_rand_instr_stream` and **the assembler rejected it**:

```
ls_0.S:109: Error: illegal operands `c.sw a3,-34(a5)'
```

`C.SW` is a CS-format instruction, so its offset is **zero-extended and 4-byte
aligned (0-124)**. `-34` is negative and not a multiple of 4, so the encoding is
simply impossible.

The declaration in `rv32c_instr.py` is `imm_t.UIMM`, the same as the UVM version,
and there is a guard at `riscv_load_store_instr_lib.py:145`:

```python
if ((self.offset[i] in range(128)) and (self.offset[i] % 4 == 0) and ...):
```

But that guard only applies to **the instructions the directed load/store stream
creates itself**. The C_SW/C_LW instructions mixed in by `add_mixed_instr()` do
not go through that path, so negative offsets come out as is. -> **Another pyflow
bug.**

As a result, the path to generating load/store-heavy programs with pyflow is
blocked.

### Status of load/store writeback verification

Because of the bug above, **no comparison has been run yet with a load-heavy
riscv-dv program.**
The pending-writeback handling itself was nonetheless confirmed in two ways:

1. A synthetic log unit test - the pending write of an `lw` is retroactively
   applied (`t2:deadbeef`)
2. The **574 mul/div instructions** in the real 10k test - they take the **same
   long-latency writeback path** as loads (`ll_wen` -> `x<rd> p<rd> 0x<data>`)
   and all matched down to the value

So the mechanism is verified; what is blocked is the generator side, "producing a
load-heavy program".

### Speed

`riscv_rand_instr_test` (10k instructions + 5 sub-programs + 7 kinds of directed
stream) takes over 20 minutes to generate even with `--target rv32imc`. This is a
known speed limitation of pyflow.

## pyflow - how to use riscv-dv without a commercial simulator

riscv-dv is originally a UVM/SystemVerilog (VCS/Questa) generator, but it has a
**pyflow** mode (pure Python + PyVSC) that generates without a commercial
simulator. Select it with `--simulator pyflow`.
The supported ISAs are RV32IMAFDC / RV64IMAFDC, **M-mode only**, which fits our
core (RV32IMAC, M-mode, no MMU) exactly. Alternatives such as riscv-torture were
therefore unnecessary.

The downside is speed. Generating one 10k-instruction program takes about 5
minutes (a known limitation, stated in the documentation).

## The RV32IMAC target

riscv-dv's default targets do not include `rv32imac` (`rv32imc` exists but lacks
atomics), so it was added.

- `pygen/pygen_src/target/rv32imac/riscv_core_setting.py` - adds `RV32A` to
  `supported_isa`
- `target/rv32imac/` - testlist + the `.sv` core setting (for the UVM flow, kept
  for symmetry)
- `run.py` - `--target rv32imac` -> `isa=rv32imac_zicsr_zifencei`, `mabi=ilp32`

Since it is based on `rv32imc`, the core characteristics already match:
`SATP_MODE=BARE` (no MMU), `supported_privileged_mode=[MACHINE_MODE]` (no S/U),
`support_pmp=0`, `support_sfence=0`. No paging or S-mode tests are generated.

The generated code has `misa = 0x40001105` (MXL=RV32, bit0=A, bit2=C, bit8=I,
bit12=M), confirming IMAC.

## Memory map / termination signal

riscv-dv's `scripts/link.ld` is used **as is**, because it already matches our
environment.

- `. = 0x80000000` - matches the chipyard DRAM base (DTS:
  `memory@80000000, 0x10000000` = 256MB)
- The `tohost` / `fromhost` symbols - fesvr (HTIF) finds them and uses them as
  the termination signal. The generated code ends with an `ecall` and then writes
  `gp` to `tohost` in `write_tohost`. The same convention as riscv-tests.

`link_chipyard.ld` is kept alongside it but **is not used in the current flow** -
see below.

## Troubleshooting log 2: the default `+verbose` trace is not enough for verification

riscv-dv only ships converters for spike/ovpsim/sail/whisper/renode and none for
Rocket, so one was written; it first used Rocket's default `+verbose` trace
format:

```
C0:  19 [1] pc=[00010000] W[r10=00010000][1] R[r 0=...] R[r 0=...] inst=[00000517] DASM(00000517)
         ^valid           ^rd  ^wdata      ^rf_wen
```

`rv32ui-p-add` gave `[PASSED] 234 matched`, but a riscv-dv random program gave
`[FAILED] 65 matched, 76 mismatch`. On inspection this was **not an RTL bug but a
limitation of the trace**:

```
C0: 377 [1] pc=[80000134] W[r21=00000000][0] ... inst=[02e22ab3] mulhsu s5, tp, a4
                                          ^^^ rf_wen = 0
```

Because of `coreMonitorBundle.wrenx = wb_wen && !wb_set_sboard` in
`RocketCore.scala`, **every long-latency writeback that goes through the
scoreboard (load, mul/div) is reported with `rf_wen=0`.**
That is, the destination register never appears in the trace at all. In that
program, 29 mul/div instructions and every load were `[0]`. Verification of
RV32I**M**AC does not hold if loads and the M extension are invisible.
(`rv32ui-p-add` passing was a coincidence: it has almost no loads and mostly
writes zeros, so the state change was not observable either way.)

On the 10k random instruction test it was `[FAILED] 73 matched, 2176 mismatch` -
effectively impossible to compare. (The same program terminated normally with
`*** PASSED ***` on the RTL side - i.e. the core was not wrong, the trace was
insufficient.)

### Solution: enable the rocket-chip commit log

`RocketCore.scala` has a separate **spike-compatible commit log** path enabled by
`enableCommitLog`, and it includes output dedicated to long-latency writebacks:

```scala
when (ll_wen && rf_waddr =/= 0.U) { printf("x%d p%d 0x%x\n", rf_waddr, rf_waddr, rf_wdata) }
```

`enableCommitLog` is hardcoded as `val enableCommitLog = false` in
`tile/Core.scala` and there is no chipyard config fragment for it, so **the source
was edited and Verilator rebuilt.**
(The existing `+verbose` binary was backed up as
`simulator-chipyard.harness-RV32RocketConfig.verbose-backup`.)

#### A pre-existing environment problem hit during the rebuild (unrelated to riscv-dv)

The first rebuild broke at the C++ compile stage:

```
gen-collateral/SimTSI.cc:6:10: fatal error: verif_host.h: No such file or directory
```

This copy's `testchipip/src/main/resources/testchipip/csrc/SimTSI.cc` is a locally
modified version and includes `verif_host.h` for `+verif=`. The header itself
**does exist** in csrc, but it is untracked and not registered as a Chisel
resource, so the build does not copy it into `gen-collateral/`.
A reference chipyard tree's `gen-collateral/` had copies placed there manually
long ago, which is why it built there.

It was not one header but **22** of them, missing in a chain (`verif_host.h`,
`testchip_external_interrupts.h`, `verif_primitives.h`, `CLINT_*.{cc,h}`,
`PLIC_*.{cc,h}`, `I2SR_*`, `WFI_*`, `sw.h`, `timer.h` and others).
All of them exist in this copy's `testchipip/.../csrc/` but are untracked and
therefore not copied into `gen-collateral/`.

-> Solution: copy the 22 files from csrc into `gen-collateral/`.
**With one exception: `testchip_external_interrupts.h`** - the csrc version is
the real header containing only `extern` declarations, and its implementation
`SimExtInterrupts.cc` is absent from the build file list, so linking breaks.
In this environment the **self-contained stub version** from
a reference `gen-collateral` must be used (its file comment says it is a
fake implementation for environments without the real SimExtInterrupts DPI).

Reproduction script: `fix_gen_collateral.sh`. **It must be re-run every time
Verilator is rebuilt.**

This problem is a pre-existing environment issue unrelated to riscv-dv. That said,
if `+verif=` is not going to be used, reverting the local modification of
`testchipip`'s `SimTSI.cc` (`git checkout`) is the cleaner alternative.

Commit log format:

```
3 0x80000000 (0xf14022f3) x5 0x00000000        retire, result value final
3 0x80000004 (0x00004301)                      retire, no register write
3 0x80000008 (0x0000a283) x5 p5 0xXXXXXXXX     retire, write still pending
x5 p5 0x80000000                               the pending write lands later
```

Side effect: in commit log mode the `cycle` CSR counts retires rather than cycles
(`CSR.scala:593`). These tests do not read `cycle`, so it does not matter.

## Troubleshooting log 3: mtvec alignment - a genuine Rocket vs Spike divergence

With the commit log build, the RTL fell into an infinite loop at the end of the
program.
**This was not a trace problem but a real behavioural difference.**

```
80002cf6  c.li gp, 1        # the test-pass code
80002cf8  ecall             # traps here
80003600  fence r,r         # <- the RTL jumps here and loops forever
```

- The program enables **VECTORED** mode with `mtvec = mtvec_handler | 1`
  (`ori x15, x15, 1`).
- The `mtvec_handler` symbol address is `0x80003660`.
- **Spike** traps to `0x80003660` -> normally reaches `ecall_handler` ->
  `write_tohost` -> terminates.
- **Rocket** traps to `0x80003600` -> not an instruction boundary, so it executes
  garbage and loops forever.

The cause is `rocket-chip/src/main/scala/rocket/CSR.scala:1667`:

```scala
def formTVec(x: UInt) = x andNot Mux(x(0),
    ((((BigInt(1) << mtvecInterruptAlign) - 1) << mtvecBaseAlign) | 2).U, 2.U)
```

With XLEN=32, `mtvecInterruptAlign = log2Ceil(32) = 5` and `mtvecBaseAlign = 2`
-> mask = `((1<<5)-1)<<2 | 2` = `0x7E`.
In VECTORED mode Rocket force-clears **bits [6:1] of mtvec**, because
`BASE ~ BASE+4*32` (=128B) must be reserved for the vector table.

```
0x80003661 & ~0x7E = 0x80003601  ->  exception entry = 0x80003601 >> 2 << 2 = 0x80003600
```

Per the RISC-V spec, mtvec is WARL and it explicitly states that *"an
implementation may impose additional alignment constraints on BASE when
MODE=Vectored"*. So **neither Rocket nor Spike violates the spec.**

### The real cause: an alignment computation bug in riscv-dv

In vectored mode riscv-dv places the handler with `.align cfg.tvec_ceil`
(`riscv_asm_program_gen.py:801`), and that value is wrong
(`riscv_instr_gen_config.py`):

```python
# comment: "requiring up to 4xXLEN-byte alignment"  <- 4*XLEN bytes is correct
self.tvec_ceil = math.ceil(math.log2((self.XLEN * 4) / 8))   # log2(128/8)=4 -> 16 bytes
```

The comment correctly says "4xXLEN **bytes**", but the code mistakes `4*XLEN` for
a bit count and divides by 8 once more. The result is an **alignment 8 times too
small.**

| | riscv-dv computation | actually required (= Rocket) |
|---|---|---|
| XLEN=32 | `.align 4` = 16B | `.align 7` = **128B** |
| XLEN=64 | `.align 5` = 32B | `.align 8` = **256B** |

-> Fix: `math.ceil(math.log2(self.XLEN * 4))`. Both XLEN values then match
Rocket's requirement exactly.

Note: this cannot be fixed with the `--tvec_alignment` CLI option.
`tvec_alignment` is a vsc random variable and a soft constraint
(`tvec_alignment == tvec_ceil`) overrides the argv value. `tvec_ceil` itself must
be fixed.

**This is an upstream riscv-dv bug.** It came from the trap vector placement
rather than the random instructions themselves, but it was caught precisely
because ISS and RTL were run side by side and compared - exactly the purpose this
flow was built for.
In real code, too, a vectored mtvec used without 128B alignment silently
misbehaves on Rocket only.

## Trace comparison

`~/repo_riscv/riscv-dv/scripts/rocket_log_to_trace_csv.py` (newly written).
It auto-detects the two formats line by line - commit log (recommended) /
`+verbose` (for the backup binary).

Conversion rules (semantics matched to spike `--log-commits`):

- `x<rd> 0x<data>` -> record the GPR write immediately (ABI name via `gpr_to_abi`
  + 8-digit hex)
- `x<rd> p<rd> 0xXXXX` -> queue it as pending, and when the later
  `x<rd> p<rd> 0x<data>` line arrives, **retroactively fill in the original
  instruction entry**. Execution is in-order single-issue, so a per-register FIFO
  suffices.
- Writes to `x0` are ignored (same as spike)
- **Skip the boot ROM**: Rocket starts from the boot ROM at `0x10000`, so
  everything before the PC first reaches `0x80000000` is discarded. This
  corresponds to Spike discarding its own trampoline (`0x1010`).
- **Stop at the trap**: the spike converter stops at `ecall`. The commit log never
  prints the instruction that raised an exception (`t.valid && !t.exception`), so
  `ecall` is not visible. Instead the ELF symbol `mtvec_handler` address is passed
  as `--end_pc` to stop at **the first trap entry**.
  Everything after that is the HTIF tohost trap handler and is not a comparison
  subject.

The commit log has no disassembly, so the driver appends `DASM(<bits>)` to each
line and pipes it through `spike-dasm` to fill in the mnemonics.

The comparison uses riscv-dv's stock `scripts/instr_trace_compare.py` unchanged.
It compares, in order, only the instructions that change GPR state.

## Troubleshooting log 1: "the RTL appears to be stuck"

Loading a riscv-dv-generated ELF onto the RTL left it stuck at the boot ROM `wfi`
with no progress for over 13 minutes. The same simulator PASSed `rv32ui-p-add` in
a few seconds.

It was not stuck - **the ELF load was slow.** On the default path, fesvr pushes
the entire program image over the serial TSI link before asserting MSIP to wake
the core from `wfi`.
riscv-dv emits `.user_stack` / `.kernel_stack` as `.rept 4999; .4byte 0x0` =
**20KB of explicit zeros each**, so the transfer volume is large and costs
millions of cycles.

### Solution: `+loadmem` backdoor loading

The chipyard harness supports `+loadmem=<elf>`. The DPI-side `memory_init` writes
directly into the backing memory with `load_elf`, and `testchip_tsi_t` detects
this and skips the TSI transfer.
**The serial transfer becomes a memcpy.**

| | Loading method | Result |
|---|---|---|
| 40KB image, no `+loadmem` | TSI serial | appears stuck in the boot ROM for over 2 minutes |
| 40KB image, with `+loadmem` | DPI backdoor | `*** PASSED ***` in **8.3 s / 5,956 cycles** |

In other words, almost all of the original cycles were loading. The driver always
passes `+loadmem`.

Side findings:

- Marking the stack `NOLOAD` in the linker script **had no effect** - fesvr zeroes
  up to `memsz` anyway. (Which is why `link_chipyard.ld` is not used; it is kept
  only as a record.)
- Reducing the stack word count did help (43 s) but was not a real fix. riscv-dv
  had no `--stack_len` option, so one was added
  (`riscv_instr_gen_config.py`) and it is adjustable with `-k`.
  Now that `+loadmem` is used, the default (5000) is left alone.
  Note: riscv-dv uses `stack_len` rather than `kernel_stack_len` for the kernel
  stack too (an upstream quirk).
- It was not a DRAM size problem - the DTS `memory@80000000` is 256MB, which is
  plenty.

## Changes made to the riscv-dv repository

`~/repo_riscv/riscv-dv` is an upstream clone with the following
modifications/additions:

| File | Change |
|---|---|
| `scripts/rocket_log_to_trace_csv.py` | **new** - Rocket commit log -> trace CSV |
| `pygen/pygen_src/target/rv32imac/` | **new** - the RV32IMAC core settings |
| `target/rv32imac/` | **new** - testlist + `.sv` core settings |
| `run.py` | added the `rv32imac` target branch |
| `pygen/pygen_src/riscv_instr_gen_config.py` | added the `--stack_len` option, **fixed the `tvec_ceil` alignment bug** |
| `target/rv32imac/testlist.yaml` | added a `riscv_amo_test` entry (currently fails to generate due to the pyflow bug) |

## Changes made to the chipyard repository

| File | Change |
|---|---|
| `generators/rocket-chip/src/main/scala/tile/Core.scala` | `enableCommitLog = false` -> `true` |
| `riscv-dv-flow/` | new - the driver script + this document |

`enableCommitLog` is a global `val` affecting the whole core, so it could not be
split out into a config. To revert, change it back to `false` and rebuild, or use
the backup binary.
