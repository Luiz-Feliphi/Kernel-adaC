# tests/

* `harness/compiletest.py` — the test runner (`make test`); `harness/emu.py` runs the generated `.exe` in the
  Unicorn CPU emulator with a simulated KERNEL32.
* `ui/run-pass/` — programs that must compile and print the expected `.out` (stdin from `.in`).
* `ui/compile-fail/` — programs that must be rejected; `// error-pattern: TEXT` lines name the expected diagnostics.
* `ui/compile-pass/` — programs that must compile.
* `ui/examples/` — expected output of the programs in `examples/`.
* `library/` — one test per standard-library module.

`python3 tests/harness/compiletest.py [--bless] [filter]` — `--bless` records the output of tests that have no `.out` yet.
