# chipyard changes (what apply_backend.sh applies)

apply_backend.sh touches exactly **one directory (the testchipip csrc)** in the
chipyard tree. Everything else in chipyard (the core, RTL, build system, Configs)
stays pristine.

## Where the changes are
```
chipyard_v110/generators/testchipip/src/main/resources/testchipip/csrc/
|-- verif_host.h          [added]    the verification host (verif_tsi_t: the PLIC/CLINT co-sim logic)
|-- verif_primitives.h    [added]    the seam interface (BP definitions, the master_* abstraction)
|-- SimTSI.cc             [modified] pristine + #include "verif_host.h" + a (+verif=) branch
`-- SimTSI.cc.orig        [backup]   the pristine SimTSI.cc (for restoring)
```

## What was modified in SimTSI.cc (exactly 2 places versus pristine)
1. one added line, `#include "verif_host.h"` (after testchip_tsi.h)
2. a +verif branch where the tsi is constructed:
   - with a `+verif=` argument -> `new verif_tsi_t` (the verification host)
   - without it -> `new testchip_tsi_t` (pristine chipyard behaviour)

## Build artifacts (automatic; not modifications of the original)
```
sims/verilator/generated-src/.../gen-collateral/
|-- SimTSI.cc, verif_host.h, verif_primitives.h  (copied from csrc, managed by make)
```

## Config
- No Config is injected. The chipyard default RV32RocketConfig is used.
  (RV32RocketConfig = WithRV32 ++ WithNBigCores(1) ++ AbstractConfig)

## Restoring the pristine state
```bash
CSRC=$CY/generators/testchipip/src/main/resources/testchipip/csrc
cp $CSRC/SimTSI.cc.orig $CSRC/SimTSI.cc        # restore the pristine SimTSI
rm $CSRC/verif_host.h $CSRC/verif_primitives.h # remove our files
# -> pristine chipyard
```

## Applying it on a company machine
```bash
verif/backend/chipyard/apply_backend.sh <CHIPYARD_PATH> RV32RocketConfig
cd <CHIPYARD_PATH>/sims/verilator && make CONFIG=RV32RocketConfig
```
The company chipyard starts from a pristine state, so a genuinely pristine copy
gets backed up to SimTSI.cc.orig.
(If it has already been modified, as on a local machine, apply_backend's pristine
check prevents an incorrect backup.)
