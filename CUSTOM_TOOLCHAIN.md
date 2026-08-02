# Running the fast regression with a custom spike + custom rv32 gcc

The fast regression in `ci/run_regress.sh` uses **two** toolchain variables. Their
names look similar but their roles are completely different. Change only one and
the other silently keeps using the old one.

| Variable | Role | Who reads it |
|---|---|---|
| `RISCV` | the **spike engine** (libriscv/libfesvr/headers) + passing the safety guard | `ci/run_regress.sh`, `vp/spike/build.sh` |
| `RISCV_TOOLCHAIN` | the **rv32 firmware compiler** (`riscv32-unknown-elf-gcc`) | `Implementation/Makefile.VERIF` |

---

## 1. What each variable actually requires (measured)

### 1.1 `RISCV` - the spike engine

What `vp/spike/build.sh:12-27` looks for in this prefix:

```sh
if [ -n "$RISCV" ] && [ -d "$RISCV/include/riscv" ]; then     # <- this directory is the SPIKE_NATIVE switch
  FLAGS="-DSPIKE_NATIVE -I$RISCV/include"
  LIBS="-L$RISCV/lib -lriscv -lfesvr -Wl,-rpath,$RISCV/lib"
  # -lriscv_disasm is added only when libriscv_disasm.{so,a} exists (compatibility with older installs)
```

So exactly what is needed:

| Path | If missing |
|---|---|
| `$RISCV/include/riscv/` | `-DSPIKE_NATIVE` is off, so it **only syntax-checks and never builds the sim** (a silent failure) |
| `$RISCV/include/fesvr/` | includes such as `htif.h` fail |
| `$RISCV/lib/libriscv.so` | `-lriscv` link failure |
| `$RISCV/lib/libfesvr.a` | `-lfesvr` link failure |
| `$RISCV/lib/libriscv_disasm.*` | **optional** - recent installs where it is named `libdisasm.a` build fine without linking it (confirmed by measurement) |

What `ci/run_regress.sh` additionally requires:

```sh
# run_regress.sh:84-88  the safety guard
if [ ! -x "$RISCV/bin/riscv64-unknown-elf-gcc" ]; then exit 2; fi
export PATH="$RISCV/bin:$PATH"
export LD_LIBRARY_PATH="$RISCV/lib:$LD_LIBRARY_PATH"
```

**`riscv64-unknown-elf-gcc` is never executed even once in fast mode** (the rv32
firmware is compiled by the gcc on the `RISCV_TOOLCHAIN` side). Only its presence
is checked -> a single symlink is enough to pass.

The previously referenced value `$HOME/chipyard-scala/.conda-env/riscv-tools`
worked simply because the spike libraries and the riscv64 gcc **happened to live
in the same prefix**.
A custom spike install has no riscv64 gcc, so **a combined prefix is required.**

### 1.2 `RISCV_TOOLCHAIN` - the rv32 firmware compiler

`Implementation/Makefile.VERIF:32-37`:

```make
RISCV_TOOLCHAIN ?= $HOME/_project/rv32-multilib
RISCV_PREFIX    ?= $(RISCV_TOOLCHAIN)/bin/riscv32-unknown-elf-
CC      = $(RISCV_PREFIX)gcc
OBJDUMP = $(RISCV_PREFIX)objdump
NM      = $(RISCV_PREFIX)nm
SIZE    = $(RISCV_PREFIX)size
```

It uses `?=`, so the environment variable wins. All that is needed is
`$RISCV_TOOLCHAIN/bin/riscv32-unknown-elf-{gcc,objdump,nm,size}`, plus an
`ARCH=rv32imac_zicsr_zifencei ABI=ilp32` multilib with newlib.
There is nothing to combine, so **just pass the path through.**

---

## 2. `setup_custom_riscv.sh` - the reusable script

The conda originals are **only read and symlinked**. Nothing is copied or
modified.

```
custom spike install --+
  include/{riscv,fesvr,fdt,softfloat}
  lib/{libriscv.so,libfesvr.a,libsoftfloat.so,libdisasm.a,libcustomext.so}
  bin/{spike,spike-dasm,elf2hex,...}          |--> <out prefix>  (all symlinks)  = RISCV
conda riscv-tools -----+                      |
  bin/riscv64-unknown-elf-*  (for the guard, never executed) --+
custom rv32 toolchain ---------------------------> passed through   = RISCV_TOOLCHAIN
```

### Usage

```bash
cd <chip_platform>

# combine using the default paths + rebuild the vp/spike drivers
./setup_custom_riscv.sh --rebuild-vp

# giving the paths explicitly
./setup_custom_riscv.sh \
  --spike     $HOME/opt/riscv/spike/current \
  --toolchain $HOME/opt/riscv/toolchain/rv32gcc-13.2.0-fast_interrupt \
  --gcc64     $HOME/chipyard-scala/.conda-env/riscv-tools \
  --out       $HOME/opt/riscv/merged/custom-verif \
  --rebuild-vp

# receive only the exports into the shell
eval "$(./setup_custom_riscv.sh --quiet --print-env)"
```

| Argument | Default | Meaning |
|---|---|---|
| `--spike` | `~/opt/riscv/spike/current` | custom spike install prefix |
| `--toolchain` | `~/opt/riscv/toolchain/rv32gcc-13.2.0-fast_interrupt` | custom rv32 gcc prefix |
| `--gcc64` | `~/chipyard-scala/.conda-env/riscv-tools` | the riscv64 gcc source for the safety guard (read-only) |
| `--out` | `~/opt/riscv/merged/custom-verif` | the combined prefix = `RISCV` |
| `--rebuild-vp [path]` | the directory containing this script | force-rebuild the vp/spike drivers |
| `--force` | - | overwrite an existing `--out` that has no marker |
| `--quiet` / `--print-env` | - | print the exports for `eval` |

### Safety mechanisms

- `--out` is **refused if it equals, or lies inside, `--spike`/`--toolchain`/`--gcc64`** -> the originals cannot be damaged
- `--out` is refused if it is inside `$HOME/chipyard{,-benchmark}` (the same guard as `run_regress.sh`)
- An existing `--out` is regenerated only when it carries the marker the script leaves (`.custom_riscv_prefix`); otherwise `--force` is required
- Self-check after creation: `bin/riscv64-unknown-elf-gcc` (the guard), `include/riscv`, `lib/libriscv.*`

### Why `--rebuild-vp` is needed

`run_regress.sh:141-144` **does not rebuild** `vp/spike/verif_spike_run` if it
already exists.
An existing binary still carries the `RUNPATH` of the old `RISCV`
(in this case `<chipyard tree>/.conda-env/riscv-tools/lib`),
so **changing `RISCV` alone still runs against the old libriscv.**
`--rebuild-vp` deletes the four drivers (`verif_spike_run`, `verif_ca_run`,
`verif_ca_run_dram`, `verif_spike_gdb`) and rebuilds them against the new
`RISCV`.

To confirm:
```
$ readelf -d vp/spike/verif_spike_run | grep RUNPATH
  RUNPATH  [$HOME/opt/riscv/merged/custom-verif/lib]
$ ldd vp/spike/verif_ca_run | grep riscv
  libriscv.so => $HOME/opt/riscv/merged/custom-verif/lib/libriscv.so
```

---

## 3. Verification result - fast 8/8 PASS (measured)

```bash
cd <chip_platform>
./setup_custom_riscv.sh --force --rebuild-vp

RISCV=$HOME/opt/riscv/merged/custom-verif \
RISCV_TOOLCHAIN=$HOME/opt/riscv/toolchain/rv32gcc-13.2.0-fast_interrupt \
CONFIG=RV32RocketConfig \
ci/run_regress.sh fast
```

```
============================================================================
 chip_platform regression  (mode=fast)   CFG=RV32RocketConfig
 RISCV=$HOME/opt/riscv/merged/custom-verif
============================================================================
 TEST                 BUILD  SPIKE          TIME      NOTE
----------------------------------------------------------------------------
 CLINT_sw_interrupt   ok     n/a            0.2s      needs host Vseq(MSIP) - RTL only
 EXCEPTION_traps      ok     n/a            0.3s      trap entry / mepc resume - RTL only
 PMP_violation        ok     n/a            0.4s      PMP violation trap (mcause 5/7) - RTL only
 CA_measure           ok     PASS           0.3s      CPI OK (LDUSE 1.497, DMISS 14.461)
 BENCH_dhrystone      ok     n/a            0.4s      needs HTIF console handling - RTL only
 BENCH_coremark       ok     n/a            0.8s      needs HTIF console handling - RTL only
 FREERTOS_preempt     ok     n/a            0.7s      CLINT timer + UART - RTL only
 UART_printf          ok     n/a            0.2s      UART0 MMIO not modelled - RTL only
----------------------------------------------------------------------------
 RESULT: PASS   (8/8 built, 1 spike-verified)      exit code 0
============================================================================
```

`CA_measure`'s CPI (LDUSE 1.497 / DMISS 14.461) matches the reference values in
`ci/ca_baseline.txt` -> the custom spike produces **timing results identical to
the previous spike**.

### Evidence that the custom tools were actually used

```
$ grep -o '$HOME/opt/riscv/toolchain/[^ ]*gcc' build/ci-logs/build_CA_measure.log | head -1
$HOME/opt/riscv/toolchain/rv32gcc-13.2.0-fast_interrupt/bin/riscv32-unknown-elf-gcc

$ readelf -p .comment build/CA_measure.elf
  GCC: (gc891d8dc23e-dirty) 13.2.0        <- the custom fast_interrupt build
  GCC: (GNU) 13.2.0

$ ldd vp/spike/verif_ca_run | grep riscv
  libriscv.so => $HOME/opt/riscv/merged/custom-verif/lib/libriscv.so
                 -> $HOME/opt/riscv/spike/chipyard-v1.13-9c190a07/lib/libriscv.so
```

### Confirming the conda originals are undamaged

```
$ ls -ld ~/chipyard-scala/.conda-env/riscv-tools{,/bin,/lib}
drwxrwxr-x ... Jul 26 22:27 .../riscv-tools
drwxr-xr-x ... Jul 26 22:27 .../riscv-tools/bin
drwxr-xr-x ... Jul 26 22:27 .../riscv-tools/lib
```
No mtime change since the work date (Jul 29). The combined prefix is entirely
symlinks:
```
$ ls -l ~/opt/riscv/merged/custom-verif/bin/riscv64-unknown-elf-gcc
... -> $HOME/chipyard-scala/.conda-env/riscv-tools/bin/riscv64-unknown-elf-gcc
```

---

## 4. The custom components used here

| Item | Path | Version |
|---|---|---|
| custom spike | `~/opt/riscv/spike/chipyard-v1.13-9c190a07` (symlinked as `spike/current`) | Spike 1.1.1-dev, chipyard 1.13 pin `9c190a07` |
| custom rv32 gcc | `~/opt/riscv/toolchain/rv32gcc-13.2.0-fast_interrupt` | GCC 13.2.0 (`gc891d8dc23e-dirty`) |
| combined prefix (`RISCV`) | `~/opt/riscv/merged/custom-verif` | symlinks only |

### When the custom spike has to be rebuilt

From the bundle (`$HOME/_project/riscv-isa-sim-9c190a07.bundle`,
chipyard 1.13 pin):

```bash
git clone $HOME/_project/riscv-isa-sim-9c190a07.bundle riscv-isa-sim
cd riscv-isa-sim && git checkout 9c190a07
mkdir build && cd build
../configure --prefix=$HOME/opt/riscv/spike/<tag>     # --prefix is required: include/riscv is installed here
make -j"$(nproc)" && make install
```

What `make install` produces - this is everything `RISCV` requires:
```
<prefix>/include/{riscv,fesvr,fdt,softfloat}/
<prefix>/lib/{libriscv.so,libfesvr.a,libsoftfloat.so,libdisasm.a,libcustomext.so}
<prefix>/bin/{spike,spike-dasm,elf2hex,spike-log-parser,xspike,termios-xspike}
```
After building, recombine with
`./setup_custom_riscv.sh --spike <prefix> --rebuild-vp`.

---

## 5. Common pitfalls

1. **Changing `RISCV` but not `RISCV_TOOLCHAIN`** leaves the firmware compiled
   with the `Makefile.VERIF` default
   (`$HOME/_project/rv32-multilib`). Both must be passed.
2. **Not deleting the `vp/spike` drivers** keeps them running against the old
   libriscv (RUNPATH). Use `--rebuild-vp`.
3. **`CONFIG` is accepted only as an environment variable.**
   `ci/run_regress.sh fast RV32RocketConfig` gives `exit 2`.
4. **Passing a prefix without `include/riscv` as `RISCV`** does not produce a
   link error; it falls through to the "offline compile check" path and no sim is
   built. The script blocks this up front.
5. The `RISCV` default in `run_regress.sh`,
   `$HOME/chipyard-merge2/.conda-env/riscv-tools`, does not exist on
   this machine -> always specify it explicitly.
