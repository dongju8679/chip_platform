#!/usr/bin/env python3
"""ca_compare3.py - three-way CA_measure comparison.

  [1] spike functional   spike --log-commits          no cycle model
  [2] spike + model      vp/spike/verif_spike_run     timing_model.h prediction
  [3] RTL                chipyard verilator           real hardware

The same ELF, the same measurement code, the same result table at 0x80010100.
Instruction counts must be identical everywhere; only the cycle columns differ.
Column [2] is what the timing model predicts, so [2] vs [3] is the accuracy of
the model and [1] vs [3] is the size of the gap the model exists to close.

Usage:
  # [1] functional spike (commit-log trace)
  "$RISCV/bin/spike" --isa=rv32imac_zicsr_zifencei -m0x80000000:0x10000000 \
        --log-commits -l build/CA_measure.elf > build/logs/CA_measure.spike.log 2>&1

  # [2] spike + timing model     (ci/run_regress.sh fast produces this)
  ./vp/spike/verif_spike_run build/CA_measure.elf +verif=CA_measure \
        > build/logs/CA_measure.model.log 2>&1

  # [3] RTL                      (verif/ip/S5740/CA/measure/run.sh produces this)
  CHIPYARD=$CY CONFIG=$CONFIG ./verif/ip/S5740/CA/measure/run.sh

  python3 ca_compare3.py build/logs/CA_measure.spike.log \
                        build/ci-logs/spike_CA_measure.log \
                        build/logs/CA_measure.log

Any of [1] or [2] may be omitted with '-' to fall back to a two-way view.
Both "[verif]" and "[verif_spike]" table prefixes are accepted, so logs from
either the chipyard host or verif_spike_run can be passed as-is.

This supersedes spike_compare.py, which handled only [1] vs [3].
"""
import re
import sys

TABLE = 0x80010100
MAGIC = 0xCA5EC0DE
ENTRY0 = TABLE + 16
STRIDE = 32


def parse_commitlog(path):
    """Rebuild the result table from the store trace of spike --log-commits."""
    mem = {}
    with open(path, errors="replace") as f:
        for line in f:
            for m in re.finditer(r"mem (0x[0-9a-f]+) (0x[0-9a-f]+)", line):
                mem[int(m.group(1), 16)] = int(m.group(2), 16)

    def word(a):
        return mem.get(a, 0)

    def name(a):
        s = ""
        for i in range(8):
            c = mem.get(a + i, 0)
            if c == 0:
                break
            s += chr(c)
        return s

    if word(TABLE) != MAGIC:
        sys.exit(f"[!] {path}: CA table magic not found "
                 f"(is this a --log-commits trace? did the run finish?)")
    out = {}
    for i in range(word(TABLE + 4)):
        b = ENTRY0 + i * STRIDE
        out[name(b)] = (word(b + 8), word(b + 16))   # (cyc, ins)
    return out


def parse_verif(path):
    """Read the '[verif] <BLOCK> <cyc_raw> <ins_raw> ...' table.

    Both the chipyard host and verif_spike_run print through the same Vseq,
    so this one parser serves [2] and [3] alike.
    """
    out = {}
    order = []
    # The chipyard host prints "[verif]"; verif_spike_run prints
    # "[verif_spike]". Same Vseq, same columns - accept either.
    pat = re.compile(r"^\[verif(?:_spike)?\] ([A-Z0-9_]+)\s+(\d+)\s+(\d+)\s")
    with open(path, errors="replace") as f:
        for line in f:
            m = pat.match(line)
            if m:
                name = m.group(1)
                if name not in out:
                    order.append(name)
                out[name] = (int(m.group(2)), int(m.group(3)))
    if not out:
        sys.exit(f"[!] {path}: no '[verif] <BLOCK> ...' rows found "
                 f"(did the run finish? wrong log?)")
    return out, order


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    p1, p2, p3 = sys.argv[1:4]

    func = parse_commitlog(p1) if p1 != "-" else {}
    model, order_m = parse_verif(p2) if p2 != "-" else ({}, [])
    rtl, order_r = parse_verif(p3)
    order = order_r or order_m

    print()
    print("  [1] spike functional   (no cycle model)")
    print("  [2] spike + model      (timing_model.h)")
    print("  [3] RTL                (real hardware)")
    print()
    print(f"  {'block':<9} {'ins[1]':>8} {'ins[2]':>8} {'ins[3]':>8} {'ins==':>6}"
          f" | {'cyc[1]':>8} {'cyc[2]':>8} {'cyc[3]':>8}"
          f" | {'[2]/[3]':>8} {'[3]/[1]':>8}")
    print("  " + "-" * 104)

    ins_all_same = True
    worst_name, worst_err = None, 0.0

    for name in order:
        c3, i3 = rtl.get(name, (0, 0))
        c2, i2 = model.get(name, (0, 0))
        c1, i1 = func.get(name, (0, 0))

        present = [x for x in (i1, i2, i3) if x]
        same = len(set(present)) <= 1
        ins_all_same &= same

        # model accuracy: how far [2] is from [3]
        acc = (c2 / c3) if c3 else 0.0
        gap = (c3 / c1) if c1 else 0.0
        if c3 and c2:
            err = abs(acc - 1.0)
            if err > worst_err:
                worst_err, worst_name = err, name

        print(f"  {name:<9} {i1:>8} {i2:>8} {i3:>8} {'YES' if same else 'NO':>6}"
              f" | {c1:>8} {c2:>8} {c3:>8}"
              f" | {acc:>7.2f}x {gap:>7.2f}x")

    print("  " + "-" * 104)
    print()
    print(f"  instruction counts identical in every block : "
          f"{'YES' if ins_all_same else 'NO  <-- investigate'}")
    if worst_name:
        print(f"  largest model deviation                    : "
              f"{worst_name}  ({worst_err * 100:.1f}% off RTL)")
    print()
    print("  [2]/[3] near 1.00x means the timing model matches the hardware.")
    print("  [3]/[1] is the gap the model exists to close (no cache model in [1]).")
    print()


if __name__ == "__main__":
    main()
