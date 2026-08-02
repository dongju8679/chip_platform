# Configuration (config) - yaml-driven verification assembly

> This directory explains the **configuration concept** of chip_platform_v2.
> The yaml under `examples/` is currently a **design illustration**; the code
> (the Makefile) does not read it yet (values stay hardcoded). It will be
> activated when a second backend or product line is added.
> For now it is reference material showing "the configuration structure we
> intend".

---

## The concept in one sentence

> **Verification is a combination of settings. Declare in yaml what to verify
> (TARGET), with which engine (BACKEND) and in which configuration (CONFIG), and
> the platform assembles and runs that combination.**

The verification subject, engine and scope are swapped by changing settings, not
by editing test code.

---

## Why yaml - what it expresses

Parameters are scattered, and each has a different owner.

| Information | Owner | Reason |
|---|---|---|
| Paths | the directory structure | self-evident from the `targets/S5740/suite/<F>/<SF>/` convention |
| Test list | `S5740.list` | the SDK's original catalog (unmodified) |
| Test addresses | the test's `.h` | the SDK's original agreed values (unmodified) |
| **Metadata, valid combinations, matrix** | **yaml** | what the three above cannot express |

The unique value of yaml is the **"valid combination"**. "Which BACKENDs can
verify this TARGET" cannot be expressed by directories or by the list - only
yaml can declare it.

> yaml is "the single source of metadata", not "the source of everything".
> Paths, the test list and addresses each have a different owner; yaml deals
> only with combinations and metadata on top of them.
> This is what keeps the SDK port unmodified (yaml never overrides the tests).

---

## The three yaml files

### 1. `target.yaml` - the metadata of one product line + its valid backends

```yaml
name: S5740
isa:  rv32imac_zicsr_zifencei     # moved here from the Makefile hardcoding
abi:  ilp32
dut:
  default_config: RV32CosimConfig
supported_backends:                # only yaml can express this
  - { name: chipyard, verified: true,
      addr_override: { test: 0x80010004, break: 0x80010000 } }
  - { name: rt-dev,   verified: false }
  - { name: sv,       verified: false }
```

Defined here (owner): ISA, ABI, default_config, supported_backends.
Referenced (owned elsewhere): suite/ (the directory), S5740.list (the list) -
**not redefined here**.

### 2. `backend.yaml` - the character of one engine

```yaml
name:     chipyard
type:     rtl              # rtl | iss | tlm
accuracy: cycle-accurate
host_comm: tsi
capabilities:
  trigger_irq: false       # CI reads this and automatically skips host-stimulus tests
  cycle_count: true        # benchmarking is possible
```

CI reads `capabilities` and branches automatically according to the backend's
abilities. (For example, PLIC co-sim tests are not run on chipyard, where
`trigger_irq: false`.)

### 3. `matrix.yaml` - the combinations CI iterates over

```yaml
runs:
  - { target: S5740, backend: chipyard, tests: [CLINT/rtc],  expect: PASS }
  - { target: S5740, backend: rt-dev,   tests: [PLIC/enable], enabled: false }
```

It references target.yaml (valid combinations) and S5740.list (tests) to
enumerate "what to run".

---

## From configuration to artifacts (one case)

What the first run in `matrix.yaml` actually produces:

```
matrix: target=S5740, backend=chipyard, tests=[CLINT/rtc], expect=PASS
   |
   |- target.yaml -> ARCH=rv32imac.. ABI=ilp32  (firmware build flags)
   |- target.yaml -> addr_override 0x80010004   (the chipyard addresses)
   `- backend.yaml -> host_comm=tsi             (which adapter)
   |
   v  the assembled command
make TARGET=S5740 F=CLINT SF=rtc ARCH=.. CONFIG=RV32CosimConfig BACKEND=chipyard
   |
   v  the artifacts
dist/S5740-chipyard-RV32CosimConfig/
  |- build/CLINT_rtc.elf      (the instruction set determined by isa)
  |- result: PASS             (compared against expect -> the CI verdict)
  `- *.vcd                    (because waveform:true)
```

Each yaml value determines a specific part of the artifacts - tests -> firmware,
isa -> instruction set, backend -> engine, expect -> pass criterion.

---

## Phased adoption (roadmap)

| Stage | When | What |
|---|---|---|
| **Now** | one TARGET and one BACKEND | hardcoded in the Makefile. The yaml is an example (this folder). |
| **1** | cleaning up ISA/ABI | introduce `target.yaml`; the Makefile reads ISA/ABI |
| **2** | a second backend | activate `backend.yaml` (chipyard/spike/rt-dev) |
| **3** | building CI | activate `matrix.yaml`, automatic iteration |

Why the yaml is not wired into the code yet: with one TARGET and one BACKEND,
yaml's core value of "managing valid combinations" has nothing to show yet
(YAGNI). The design is complete; activation waits until it is needed.

---

## Related diagrams

Visual material for the configuration concept (outside this repository, in
`docs/` or provided separately):
- ownership split (who owns what)
- the relationship between the 3 yaml files
- tracing configuration through to artifacts
