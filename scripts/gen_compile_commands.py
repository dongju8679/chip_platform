#!/usr/bin/env python3
"""
gen_compile_commands.py - build a JSON compilation database for chip_platform.

Why this exists
---------------
Include paths in this project are per-test: verif/ip/<TARGET>/<F>/<SF>/Inc is
put on the command line by Implementation/Makefile.VERIF, and the source list
itself changes with TRACK / USE_FREERTOS / USE_BENCH / HAL module selection.
A hand-written includePath in c_cpp_properties.json goes stale the moment a
test is added, so the command lines are extracted from make instead.

How
---
`bear` is not required (and is not installed on the reference machine). Instead
each test combination is run through

    make -f Implementation/Makefile.VERIF ... --always-make -n

which prints the recipe without executing it, so nothing is written under
build/ and no compiler has to exist for the extraction itself.

Makefile.VERIF links every source in ONE gcc invocation (there are no per-file
.o rules apart from the two dhrystone ones), so that single command line is
split back into one compile entry per .c file. Link-only flags (-T/-o/-n/
--specs/-u/-Wl,...) are dropped and `-c -o <obj>` is substituted.

All test combinations found under verif/ip/<TARGET>/ are merged into one
compile_commands.json, so opening any test folder is covered. Two extra sweeps
are added so that shared code which no single test happens to pull in is also
covered:
  * every Platform/Common/Src module forced on via HAL_MODULES / HAL_ASM_MODULES
  * TRACK=shim (hal_shim.c is not part of the TRACK=baremetal source list)

Usage
-----
    scripts/gen_compile_commands.py                  # -> ./compile_commands.json
    scripts/gen_compile_commands.py -o /tmp/cdb.json
    RISCV_TOOLCHAIN=/path/to/rv32-gcc scripts/gen_compile_commands.py
    scripts/gen_compile_commands.py --verbose

Re-run it after adding a test folder, or after switching toolchains (each entry
records the compiler by absolute path).

The toolchain is a fallback rather than a pin - an exported RISCV_TOOLCHAIN
wins, otherwise DEFAULT_TOOLCHAIN below applies. Same precedence as the
"build: current F/SF (release)" task in .vscode/tasks.json; see the comment on
DEFAULT_TOOLCHAIN.
"""

import argparse
import json
import os
import shlex
import subprocess
import sys

# ---------------------------------------------------------------------------
# repo layout
# ---------------------------------------------------------------------------
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                      # chip_platform top level
MAKEFILE = "Implementation/Makefile.VERIF"
TARGET = os.environ.get("TARGET", "S5740")
VERIF_DIR = os.path.join(ROOT, "verif", "ip", TARGET)
COMMON_SRC = os.path.join(ROOT, "Platform", "Common", "Src")

# -- rv32 firmware compiler: fallback, not a pin -----------------------------
#   Same precedence as the "build: current F/SF (release)" task in
#   .vscode/tasks.json, which is the shell equivalent:
#
#       TC=${RISCV_TOOLCHAIN:-$HOME/_project/rv32-multilib}
#
#   1. an exported RISCV_TOOLCHAIN always wins - verbatim, even if it turns
#      out to be broken (see pick_toolchain: it warns instead of quietly
#      substituting some other compiler, because a database built against a
#      different compiler than the one make uses is worse than a loud one)
#   2. otherwise DEFAULT_TOOLCHAIN
#   3. otherwise the remaining known prefixes, as a convenience probe
#
#   Empty counts as unset, matching bash ${VAR:-...} rather than ${VAR-...}.
#   That distinction matters here: Makefile.VERIF uses `RISCV_TOOLCHAIN ?=`,
#   and a *defined but empty* value still beats ?=, which would silently make
#   RISCV_PREFIX "/bin/riscv32-unknown-elf-". run_make_dry() scrubs such a
#   value out of the child environment for the same reason.
#
#   Keep DEFAULT_TOOLCHAIN in sync with the fallback in .vscode/tasks.json.
DEFAULT_TOOLCHAIN = os.path.expanduser("~/_project/rv32-multilib")
OTHER_TOOLCHAINS = [
    os.path.expanduser("~/opt/riscv/toolchain/rv32gcc-13.2.0-fast_interrupt"),
    os.path.expanduser("~/opt/riscv/toolchain/rv32gcc-13.2.0-9a28c80"),
    # the Makefile's own default: the dscloud site path, absent elsewhere
    "/share/opt/toolchain/gcc13.2_fastinterrupt",
]
CC_NAME = "riscv32-unknown-elf-gcc"

# Per-test make arguments, mirroring ci/run_regress.sh:build_args().
# Anything not listed here builds with the plain TRACK=baremetal flag set.
EXTRA_ARGS = {
    "FREERTOS_preempt": ["USE_FREERTOS=1"],
    "BENCH_coremark":   ["USE_BENCH=coremark", "BENCH_ITERATIONS=2"],
    "BENCH_dhrystone":  ["USE_BENCH=dhrystone"],
}

# Flags that only mean something at link time. Keeping them would make cpptools
# pass them to the compiler when it probes for builtin defines / system
# includes, which at best is noise and at worst errors out.
LINK_ONLY_FLAGS = {"-n", "-nostartfiles", "-s", "-static", "-nostdlib"}
LINK_ONLY_WITH_VALUE = {"-o", "-T", "-u", "-Xlinker", "-l"}
# Options whose value is a separate argv entry and which DO matter to IntelliSense.
KEEP_WITH_VALUE = {"-I", "-D", "-U", "-include", "-isystem", "-idirafter", "-imacros"}


def log(msg):
    print(msg, file=sys.stderr)


def has_cc(prefix):
    return bool(prefix) and os.path.isfile(os.path.join(prefix, "bin", CC_NAME))


def pick_toolchain():
    """-> (prefix or None, how) following the ${RISCV_TOOLCHAIN:-default} rule."""
    env = os.environ.get("RISCV_TOOLCHAIN", "").strip()
    if env:
        # Honour it as given. If it is wrong the user gets told which path is
        # wrong; silently building the database with a different compiler than
        # `make` will use is the failure mode worth avoiding here.
        if not has_cc(env):
            log("[cdb] [!] RISCV_TOOLCHAIN=%s has no bin/%s" % (env, CC_NAME))
            log("[cdb]     Using it anyway - unset RISCV_TOOLCHAIN to fall back to")
            log("[cdb]     %s" % DEFAULT_TOOLCHAIN)
        return env, "RISCV_TOOLCHAIN"
    if has_cc(DEFAULT_TOOLCHAIN):
        return DEFAULT_TOOLCHAIN, "default"
    for cand in OTHER_TOOLCHAINS:
        if has_cc(cand):
            return cand, "probed"
    return None, "none"


def discover_tests():
    """Every verif/ip/<TARGET>/<F>/<SF> that has a Src/ directory.

    Src/ is allowed to be empty: BENCH/coremark and BENCH/dhrystone keep no
    firmware of their own, their bodies come from Middleware/benchmark via
    USE_BENCH=, and skipping them would leave that whole tree uncovered.
    """
    tests = []
    if not os.path.isdir(VERIF_DIR):
        return tests
    for f in sorted(os.listdir(VERIF_DIR)):
        fdir = os.path.join(VERIF_DIR, f)
        if not os.path.isdir(fdir):
            continue
        for sf in sorted(os.listdir(fdir)):
            if os.path.isdir(os.path.join(fdir, sf, "Src")):
                tests.append((f, sf))
    return tests


def hal_module_names():
    """Every Platform/Common/Src module, for the forced-coverage sweep."""
    if not os.path.isdir(COMMON_SRC):
        return [], []
    names = sorted(os.listdir(COMMON_SRC))
    c_mods = [n[:-2] for n in names if n.endswith(".c")]
    s_mods = [n[:-2] for n in names if n.endswith(".S")]
    return c_mods, s_mods


def run_make_dry(make_args, toolchain, verbose=False):
    """`make --always-make -n` -> recipe text (nothing is executed)."""
    env = dict(os.environ)
    if toolchain:
        env["RISCV_TOOLCHAIN"] = toolchain
    else:
        # Nothing found: let Makefile.VERIF apply its own ?= default. An
        # inherited empty RISCV_TOOLCHAIN would defeat that ?= and yield
        # RISCV_PREFIX="/bin/riscv32-unknown-elf-", so drop it entirely.
        env.pop("RISCV_TOOLCHAIN", None)
    cmd = ["make", "-f", MAKEFILE, "src_dir=.", "TRACK=baremetal",
           "EXTRA_CFLAGS=-DPRELOAD"] + make_args + ["--always-make", "-n"]
    if verbose:
        log("  $ " + " ".join(shlex.quote(c) for c in cmd))
    p = subprocess.run(cmd, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, universal_newlines=True)
    if p.returncode != 0:
        log("  [!] make -n failed (rc=%d): %s" % (p.returncode, p.stderr.strip().splitlines()[-1:]))
        return ""
    return p.stdout


def join_continuations(text):
    """make echoes the recipe with the original backslash-newlines intact."""
    out, buf = [], ""
    for line in text.splitlines():
        if line.endswith("\\"):
            buf += line[:-1] + " "
        else:
            out.append(buf + line)
            buf = ""
    if buf:
        out.append(buf)
    return out


def split_command(tokens):
    """One gcc invocation -> (compile flags, source files).

    Link-only flags are dropped; -c/-o are re-added by the caller per source.
    """
    flags, sources = [], []
    i, n = 1, len(tokens)          # tokens[0] is the compiler
    while i < n:
        t = tokens[i]
        if t in LINK_ONLY_WITH_VALUE:
            i += 2
            continue
        if t in LINK_ONLY_FLAGS or t.startswith("-Wl,") or t.startswith("--specs="):
            i += 1
            continue
        if t in KEEP_WITH_VALUE:
            if i + 1 < n:
                flags += [t, tokens[i + 1]]
                i += 2
            else:
                i += 1
            continue
        if t == "-c":              # the two dhrystone rules already have it
            i += 1
            continue
        if t.startswith("-"):
            flags.append(t)
            i += 1
            continue
        # non-option: a source file, an object, or a linker script
        if t.endswith(".c"):
            sources.append(t)
        # .S / .o / .ld / .elf are not IntelliSense translation units -> ignore
        i += 1
    return flags, sources


def harvest(text, cc_path, entries, seen, verbose=False):
    """Pull compile entries out of one make -n transcript."""
    added = 0
    for line in join_continuations(text):
        line = line.strip()
        if not line or cc_path not in line:
            continue
        try:
            tokens = shlex.split(line)
        except ValueError:
            continue
        if not tokens or not tokens[0].endswith("gcc"):
            continue               # objdump / nm / size lines
        flags, sources = split_command(tokens)
        for src in sources:
            abs_src = os.path.normpath(os.path.join(ROOT, src))
            if abs_src in seen:
                continue
            seen.add(abs_src)
            stem = os.path.splitext(os.path.basename(abs_src))[0]
            entries.append({
                "directory": ROOT,
                "arguments": [tokens[0]] + flags + ["-c", "-o",
                                                    "build/.cdb/%s.o" % stem, src],
                "file": abs_src,
            })
            added += 1
            if verbose:
                log("      + %s" % os.path.relpath(abs_src, ROOT))
    return added


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--output", default=os.path.join(ROOT, "compile_commands.json"),
                    help="output path (default: <repo>/compile_commands.json)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    toolchain, how = pick_toolchain()
    if toolchain:
        log("[cdb] toolchain = %s  (from %s)" % (toolchain, how))
    else:
        log("[cdb] [!] no rv32 toolchain found (tried $RISCV_TOOLCHAIN, %s, and %d more)"
            % (DEFAULT_TOOLCHAIN, len(OTHER_TOOLCHAINS)))
        log("[cdb]     Falling back to the Makefile's own default. Export")
        log("[cdb]     RISCV_TOOLCHAIN if the compiler path in the output is wrong.")
    cc_path = os.path.join(toolchain, "bin", CC_NAME) if toolchain else CC_NAME

    tests = discover_tests()
    if not tests:
        log("[cdb] [!] no tests under %s" % VERIF_DIR)
        return 1
    log("[cdb] %d test combination(s) under verif/ip/%s" % (len(tests), TARGET))

    entries, seen = [], set()

    # A shared file (Platform/Common/Src/uart.c, ...) is claimed by whichever
    # test is processed first, so process the plain baremetal tests before the
    # BENCH / FREERTOS ones. Otherwise BENCH_coremark - first alphabetically -
    # would pin every shared file to USE_NEWLIB_STDIO=0 plus the whole
    # -fno-builtin-* set, and IntelliSense would show the benchmark's stdio
    # instead of newlib's for the entire Common tree.
    tests.sort(key=lambda t: ("%s_%s" % t) in EXTRA_ARGS)

    for f, sf in tests:
        name = "%s_%s" % (f, sf)
        margs = ["F=%s" % f, "SF=%s" % sf] + EXTRA_ARGS.get(name, [])
        text = run_make_dry(margs, toolchain, args.verbose)
        added = harvest(text, cc_path, entries, seen, args.verbose)
        log("[cdb]   %-22s %2d new file(s)" % (name, added))

    # sweep 1: force every shared Platform/Common/Src module into the link so
    #          modules no test opted into still get an entry.
    c_mods, s_mods = hal_module_names()
    if c_mods:
        f, sf = tests[0]
        text = run_make_dry(["F=%s" % f, "SF=%s" % sf,
                             "HAL_MODULES=%s" % " ".join(c_mods),
                             "HAL_ASM_MODULES=%s" % " ".join(s_mods)],
                            toolchain, args.verbose)
        log("[cdb]   %-22s %2d new file(s)" % ("(all HAL modules)",
                                               harvest(text, cc_path, entries, seen, args.verbose)))

    # sweep 2: TRACK=shim pulls in Platform/Chipset/TinyRocket/**.c (hal_shim.c,
    #          interrupt.c) which the baremetal track never compiles.
    f, sf = tests[0]
    env_shim = ["F=%s" % f, "SF=%s" % sf, "TRACK=shim"]
    cmd_text = run_make_dry(env_shim, toolchain, args.verbose)
    # run_make_dry hardcodes TRACK=baremetal first; the later TRACK=shim wins
    # because make takes the last command-line assignment.
    log("[cdb]   %-22s %2d new file(s)" % ("(TRACK=shim)",
                                           harvest(cmd_text, cc_path, entries, seen, args.verbose)))

    if not entries:
        log("[cdb] [!] nothing extracted - is make available and Makefile.VERIF intact?")
        return 1

    entries.sort(key=lambda e: e["file"])
    with open(args.output, "w") as fp:
        json.dump(entries, fp, indent=2)
        fp.write("\n")
    log("[cdb] wrote %s (%d entries)" % (args.output, len(entries)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
