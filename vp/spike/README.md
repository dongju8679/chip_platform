# The spike backend

Uses spike (riscv-isa-sim) as a verification backend. A sibling of chipyard -
the same interface (`verif_primitives_t`), a different engine (libriscv).

## Files

| File | Role | Directories used |
|---|---|---|
| `verif_spike.h` | the adapter (verif_spike_t) | VERIF/include/ (the interface), riscv-isa-sim (the engine) |
| `verif_spike_run.cc` | the host main (verification driver) | verif_spike.h, build/<TEST>.elf |
| `build.sh` | build (links libriscv) | VERIF/include/, $RISCV/{include,lib} |
| `run.sh` | the run entry point | invoked by Makefile.VERIF, build/<TEST>.elf |
| `gdb_rsp.h` | **the GDB RSP server** (debugging) | processor_t (registers), simif_t (memory) |
| `verif_spike_gdb.cc` | **the GDB-wait driver main** | verif_spike.h, gdb_rsp.h |
| `debug.sh` | **the debug entry point** (-g build + start the server) | Makefile.VERIF, verif_spike_gdb |

## Interface mapping

| verif_primitives_t | spike implementation |
|---|---|
| `master_write` | `mmu->store_uint32` |
| `master_read` | `mmu->load_uint32` |
| `trigger_irq` | `state.mip \|= (1<<id)` - easier than on chipyard |
| `tick` | `proc->step(n)` |
| `done` | `mmu->load_uint64(tohost_addr) != 0` |

## Build and run

```bash
# Requires the riscv-isa-sim library to be installed:
#   git clone riscv-isa-sim && ./configure --prefix=$RISCV && make && make install
export RISCV=/path/to/riscv     # confirmed: <chipyard tree>/.conda-env/riscv-tools

# Build -> verif_spike_run, verif_ca_run, verif_ca_run_dram
vp/spike/build.sh

# Run (through the Makefile)
cd Implementation
make -f Makefile.VERIF F=CLINT SF=rtc            # the firmware ELF
make -f Makefile.VERIF F=CLINT SF=rtc run BACKEND=spike
```

### CA_measure (per-block CPI, execution-driven + the Phase 2 timing model)

```bash
# Firmware (the chipyard baremetal-track ELF is reused as is)
make -f Implementation/Makefile.VERIF src_dir=. F=CA SF=measure \
     TRACK=baremetal OPT=-O2 EXTRA_CFLAGS=-DPRELOAD

# Run: mcycle is mirrored to the timing model's value, so the firmware works unmodified
./vp/spike/verif_spike_run build/CA_measure.elf +verif=CA_measure

# Revert to the Phase 1 model (for A/B comparison)
VP_TIMING_PHASE=1 ./vp/spike/verif_spike_run build/CA_measure.elf +verif=CA_measure
```

For the results and the RTL comparison see
`chip_docs/verif/docs/ca_execution_driven.md`.

### Trap/interrupt latency (the verif_ca_run path)

```bash
vp/spike/tests/build_irq_test.sh
./vp/spike/verif_ca_run_dram vp/spike/tests/irq_latency.elf \
    +verif=IRQ_latency +expected=12 +tolerance=10
# -> measured = trap_pen(4) + the ISR instruction cost (model cycles)
```

`verif_ca_run_dram` = the build for chipyard-track ELFs (the mailbox is
overridden to 0x8001000x with -D).

### Source-level debugging with GDB (stage 1)

```bash
# In one go: rebuild the firmware with -g -> start the GDB-wait server
vp/spike/debug.sh CA measure 3333 -- EXTRA_CFLAGS=-DPRELOAD

# From another terminal
riscv64-unknown-elf-gdb build/debug/CA_measure.elf \
    -ex 'target remote :3333' -ex 'break main' -ex 'continue'
```

It is a **separate executable** from the `verif_spike_run` family, so the
existing verification paths are unaffected.
Because step is `verif_spike_t::tick(1)`, the timing model keeps accumulating
even while debugging (`p/x $mcycle` = the model cycles, and `monitor cycles`
agrees).
For details, the verification table and the pitfalls, see
`chip_docs/verif/docs/debugging.md`.

## Differences from chipyard

| | chipyard | spike |
|---|---|---|
| Connection | a separate process (the simulator executable) | linked as a library (libriscv) |
| Where the host main lives | the chipyard tree (SimTSI.cc, external) | **this folder** (verif_spike_run.cc) |
| trigger_irq | unimplemented (needs a HarnessBinder) | sets state.mip directly |
| Who owns the loop | the simulator | our main |

## Caveats

The `sim_t` ctor signature differs per riscv-isa-sim version. Construction is
isolated in one place, `spike_factory.h::make_spike_sim`, and currently targets
the installation in <chipyard tree>/.conda-env/riscv-tools (the 2024+ master
line: a 12-argument ctor, `mmu->load<T>/store<T>`,
`mip->backdoor_write_with_mask`). For a different version, only that file needs
adjusting.

## Timing model phases

`timing_model.h` is Phase 2 (previous-instruction relationships + address-aware
memory latency + a D$ model).
The `VP_TIMING_PHASE=1` environment variable reverts to Phase 1 (a fixed penalty
per instruction class).
The penalty constants are calibrated against RTL (RV32RocketConfig, Verilator)
measurements - for the rationale and comparison tables see
`chip_docs/verif/docs/ca_execution_driven.md`.
