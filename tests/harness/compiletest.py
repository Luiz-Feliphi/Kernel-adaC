#!/usr/bin/env python3
"""
compiletest.py - the adaC test harness (modelled on rustc's compiletest).

Test kinds
  run-pass      tests/ui/run-pass/*.adac, tests/library/*.adac and examples/*.adac
                compile, run the .exe in the emulator, compare stdout with NAME.out
                (stdin comes from NAME.in; the exit code defaults to 0)
  compile-fail  tests/ui/compile-fail/*.adac   must be rejected; every line
                `// error-pattern: TEXT` must appear in the compiler output
  compile-pass  tests/ui/compile-pass/*.adac   must compile (warnings allowed)

Directives (comment lines at the top of a test file)
  // exit-code: N          expected exit code of the program (default 0)
  // error-pattern: TEXT   text expected in the diagnostics (compile-fail)

Usage: python3 tests/harness/compiletest.py [--bless] [filter]
  --bless   record the current output as the expectation for tests that have none
"""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
TESTS = os.path.dirname(HERE)
ROOT = os.path.dirname(TESTS)
sys.path.insert(0, HERE)
from emu import run_pe

EXE = "adac.exe" if os.name == "nt" else "adac"
ADAC = os.path.join(ROOT, EXE)
OUT = os.path.join(TESTS, "out")
os.makedirs(OUT, exist_ok=True)

# (source dir, expectation dir, extra files, ignored names)
RUN_PASS_ROOTS = [
    (os.path.join(TESTS, "ui", "run-pass"), os.path.join(TESTS, "ui", "run-pass"), [], []),
    (os.path.join(TESTS, "library"), os.path.join(TESTS, "library"), [], []),
    (os.path.join(ROOT, "examples"), os.path.join(TESTS, "ui", "examples"), [os.path.join("modules", "main.adac")],
     ["test_original_english.adac"]),
]

def compile_file(path, exe):
    return subprocess.run([ADAC, path, "-o", exe, "--no-color"], capture_output=True, text=True)

def directives(path, key):
    vals = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            if not line.startswith("//"): break
            if line.startswith("// " + key + ":"): vals.append(line.split(":", 1)[1].strip())
    return vals

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    bless = "--bless" in sys.argv
    flt = args[0] if args else ""
    ok = fail = 0

    # ---- run-pass
    for src_dir, exp_dir, extra, ignored in RUN_PASS_ROOTS:
        names = sorted(f for f in os.listdir(src_dir) if f.endswith(".adac") and f not in ignored) + extra
        for rel in names:
            if flt not in rel: continue
            src = os.path.join(src_dir, rel)
            base = os.path.basename(rel)[:-5]
            exe = os.path.join(OUT, base + ".exe")
            c = compile_file(src, exe)
            if c.returncode != 0:
                print(f"FAIL run-pass {rel}: did not compile\n{c.stderr}"); fail += 1; continue
            in_path = os.path.join(exp_dir, base + ".in")
            stdin = open(in_path, "rb").read() if os.path.exists(in_path) else b""
            code = int((directives(src, "exit-code") or ["0"])[0])
            r = run_pe(exe, stdin)
            got = r.stdout.decode("utf-8", "replace")
            out_path = os.path.join(exp_dir, base + ".out")
            if not os.path.exists(out_path):
                if bless:
                    open(out_path, "w", encoding="utf-8").write(got)
                    print(f"BLESS {rel}: recorded {len(got)} bytes - review it!")
                else:
                    print(f"FAIL run-pass {rel}: no expected output (run with --bless)"); fail += 1
                continue
            want = open(out_path, encoding="utf-8").read()
            if r.error or r.exit_code != code or got != want:
                print(f"FAIL run-pass {rel}: exit={r.exit_code} error={r.error}\n--- got\n{got}\n--- expected\n{want}")
                fail += 1
            else:
                print(f"ok   run-pass {rel}  (live heap at exit: {r.heap_live} B, {r.heap_allocs} allocations, {r.heap_frees} frees)")
                ok += 1

    # ---- compile-fail / compile-pass
    for kind in ("compile-fail", "compile-pass"):
        d = os.path.join(TESTS, "ui", kind)
        for f in sorted(os.listdir(d)):
            if not f.endswith(".adac") or flt not in f: continue
            path = os.path.join(d, f)
            c = compile_file(path, os.path.join(OUT, f + ".exe"))
            if kind == "compile-pass":
                if c.returncode != 0: print(f"FAIL {kind} {f}\n{c.stderr}"); fail += 1
                else: print(f"ok   {kind} {f}"); ok += 1
                continue
            pats = directives(path, "error-pattern")
            missing = [p for p in pats if p not in c.stderr]
            if c.returncode == 0 or missing or not pats:
                print(f"FAIL {kind} {f}: expected {pats} (missing {missing})\n{c.stderr}"); fail += 1
            else:
                print(f"ok   {kind} {f}"); ok += 1

    # ---- driver: reading the program from stdin (used by editor integrations)
    if flt in "driver-stdin":
        r = subprocess.run([ADAC, "--check", "--no-color", "--stdin-name", "virtual/prog.adac"],
                           input='void main() {\n    int x = "hi";\n}\n', capture_output=True, text=True)
        good = subprocess.run([ADAC, "--check", "--no-color", "--stdin-name", "virtual/prog.adac"],
                              input='void main() { println(1); }\n', capture_output=True, text=True)
        if r.returncode != 0 and "virtual/prog.adac:2:13: error:" in r.stderr and good.returncode == 0:
            print("ok   driver --stdin-name"); ok += 1
        else:
            print(f"FAIL driver --stdin-name\n{r.stderr}\n{good.stderr}"); fail += 1

    print(f"\n{ok} ok, {fail} failure(s)")
    sys.exit(1 if fail else 0)

main()
