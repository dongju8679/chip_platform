# verif/ - chip_platform verification (all tests)

The directory that gathers **all of chip_platform's verification** in one place.
Product firmware (App/) and verification (verif/) are kept separate.

```
verif/
  core/        core basic-operation verification (CPU bring-up)
  ip/          detailed peripheral IP verification (co-sim)
  framework/   verification infrastructure proof (the co-sim framework)
  include/     the verification seam (verif_primitives.h)
  backend/     verification backend adapters (chipyard / rt-dev)
```

---

## 1. core/ - core basic operation (bring-up)

"Does the TinyRocket core run correctly?" - verification of the CPU itself.

| Test | What it verifies |
|---|---|
| hello | the boot sequence (start.S -> main) + UART output |
| memory | memory read/write (TCM) |
| interrupt | the core trap mechanism (mtvec, MSIE) |

- BSP: `core/common/` (start.S, link.ld, hal.h - self-contained)
- Config: **RV32RocketConfig** (the same as co-sim - one unified Config)
- Execution: verilator standalone (not co-sim)
- Build: `Implementation/Makefile.TinyRocket` (TEST=hello|memory|interrupt)

```
cd Implementation
make -f Makefile.TinyRocket TEST=hello run-verilator
```

---

## 2. ip/ - detailed peripheral IP verification (co-sim)

"Does each IP block behave exactly right?" - host (Vseq) to DUT (firmware) co-sim.

```
ip/S5740/
  CLINT/rtc/         timer / RTC counter (an IP the core requires)
  PLIC/enable/       interrupt controller - line enable (8 sources)
  PLIC/latency/      interrupt latency measurement (PMU, CA)
```

Each test = `Inc/` (headers) + `Src/` (the firmware = DUT) + `Vseq/` (host
sequences, for reference).

- BSP: `Platform/Chipset/TinyRocket/` (through hal_shim)
- Config: **RV32RocketConfig** (TSI/DRAM, host accessible)
- Execution: co-sim of the firmware (DUT) and the host sequence
- Build: `Implementation/Makefile.VERIF` (F=PLIC SF=enable, etc.)

```
make -f Implementation/Makefile.VERIF src_dir=. F=PLIC SF=enable \
     CHIPYARD=$CY CONFIG=RV32RocketConfig
```

> **Note - Vseq/ is for reference**: the co-sim host sequences are currently
> implemented directly in the backend adapter (verif_host.h -> SimTSI.cc).
> `Vseq/*.cc` is reference code in the original rt_dev form and is not used by
> the current chipyard backend.
> (It will be used when the rt-dev backend is integrated.)

---

## 3. framework/ - verification infrastructure proof

Not IP verification, but tests that demonstrate **that co-sim itself works**.

| Test | Purpose |
|---|---|
| handshake | a host-to-DUT DRAM mailbox round trip (verifies the co-sim framework) |

- Used to confirm the co-sim infrastructure is alive before adding new IP
  verification.
- If handshake PASSes, the host_set/wait/check_bp round trip is healthy.

---

## 4. include/ - the verification seam

`verif_primitives.h` - the seam connecting the verification framework to the
backends.
It defines the BP enum (BP_TEST_begin/end etc.) and the common interface.
The backends (chipyard/rt-dev) implement this seam.

---

## 5. backend/ - verification backend adapters

```
backend/
  chipyard/   the chipyard (verilator RTL) adapter
    verif_host.h           the seam implementation (verif_tsi_t)
    apply_backend.sh       injects into the chipyard tree
    patch_simtsi.py        the SimTSI.cc patching tool
    SimTSI_verif_class.txt the class used to replace part of SimTSI.cc
    overlay/               (the chipyard default RV32RocketConfig is used; no overlay)
  rt-dev/     the company verification engine adapter (run.sh)
```

Co-sim connects the host and the DUT through a backend. On the chipyard backend,
verif_tsi_t in verif_host.h plays the host role.

---

## Verification status (chipyard_v110 / RV32RocketConfig)

| Category | Test | Result |
|---|---|---|
| core | hello / memory / interrupt | PASS (RV32RocketConfig) |
| ip | CLINT/rtc | working |
| ip | PLIC/enable | PASS (8 lines) |
| ip | PLIC/latency | PASS (42 cycles) |
| framework | handshake | PASS (0xABCD) |

-> RV32 co-sim fully demonstrated. See CHANGELOG 2.4.7 for details.
