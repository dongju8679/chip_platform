# [TODO] apply_backend.sh - add a CLEAN option (apply later)

Status: design complete, not applied. Apply as described below when needed.
Not urgent - the current incremental approach works well enough.

## What gets added
Two modes for the gen-collateral handling:
- CLEAN=0 (default, current behaviour): overwrite only the changed files for an
  incremental build (fast)
- CLEAN=1: delete gen-collateral entirely -> regenerate everything from csrc
  (reliable, slow)

## Why an environment variable
- The positional arguments ($1=CY, $2=CONFIG) are untouched -> existing commands
  keep working 100% unchanged
- The default CLEAN=0 means anyone unaware of it gets the usual behaviour, and
  only those who know set CLEAN=1
- No parsing conflicts

## Usage (once applied)
    # normal (identical to today)
    backend/chipyard/apply_backend.sh $CY RV32CosimConfig
    # a clean full rebuild
    CLEAN=1 backend/chipyard/apply_backend.sh $CY RV32CosimConfig

## How to apply - replace the step 4 (gen-collateral propagation) block with the following

Existing (around lines 57-69):
    echo "== 4) propagate to gen-collateral (if already built) =="
    if [ -d "$GEN/gen-collateral" ]; then
      cp "$CSRC/SimTSI.cc"          "$GEN/gen-collateral/"
      cp "$CSRC/verif_host.h"       "$GEN/gen-collateral/"
      cp "$CSRC/verif_primitives.h" "$GEN/gen-collateral/"
      touch "$GEN/gen-collateral/SimTSI.cc"
      rm -f "$GEN/chipyard.harness.TestHarness.$CONFIG/SimTSI.o"
      rm -f "$GEN/chipyard.harness.TestHarness.$CONFIG/VTestDriver__ALL.a"
      rm -f "$CY/sims/verilator/simulator-chipyard.harness-$CONFIG"
      echo "  overwrote gen-collateral + deleted SimTSI.o/binary (picked up on rebuild)"
    else
      echo "  no gen-collateral = first build -> the csrc patch is picked up automatically (nothing to do)"
    fi

After replacement:
    # Decide the CLEAN mode (default 0 = incremental). Validate the value.
    CLEAN="${CLEAN:-0}"
    case "$CLEAN" in 0|1) ;; *) echo "[!] CLEAN must be 0/1 (got: $CLEAN) -> treating as 0"; CLEAN=0;; esac

    if [ ! -d "$GEN/gen-collateral" ]; then
      echo "== 4) no gen-collateral = first build -> the csrc patch is picked up automatically (nothing to do) =="
    elif [ "$CLEAN" = "1" ]; then
      echo "== 4) [CLEAN] deleting gen-collateral entirely -> regenerating from csrc (reliable, slow) =="
      # Safety: never delete when the path is empty or the root
      if [ -n "$GEN" ] && [ "$GEN" != "/" ] && [ -d "$GEN/gen-collateral" ]; then
        rm -rf "$GEN/gen-collateral"
        rm -f  "$CY/sims/verilator/simulator-chipyard.harness-$CONFIG"
        echo "  gen-collateral deleted -> it will be regenerated from csrc on make"
      else
        echo "  [!] path validation failed - skipping the delete: GEN=$GEN"
      fi
    else
      echo "== 4) [incremental] overwriting only the changed files (fast) =="
      cp "$CSRC/SimTSI.cc"          "$GEN/gen-collateral/"
      cp "$CSRC/verif_host.h"       "$GEN/gen-collateral/"
      cp "$CSRC/verif_primitives.h" "$GEN/gen-collateral/"
      touch "$GEN/gen-collateral/SimTSI.cc"
      rm -f "$GEN/chipyard.harness.TestHarness.$CONFIG/SimTSI.o"
      rm -f "$GEN/chipyard.harness.TestHarness.$CONFIG/VTestDriver__ALL.a"
      rm -f "$CY/sims/verilator/simulator-chipyard.harness-$CONFIG"
      echo "  overwrote gen-collateral + deleted SimTSI.o/binary (picked up on rebuild)"
    fi

## Also add to the header comment (so the usage is visible)
    #   CLEAN=1 apply_backend.sh <CHIPYARD_PATH> [CONFIG]   # regenerate gen-collateral entirely

## Core principles
- Steps 1, 2 and 3 (copying csrc + patching SimTSI) are mandatory in both modes
  (gen-collateral is derived from csrc)
- Nothing changes for existing users (without CLEAN, behaviour is identical)
