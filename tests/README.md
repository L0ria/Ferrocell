`tests/` holds the host-side test harness.

## M2 (issue #3) — pure-core unit tests

- `core_test.cpp` — self-contained unit tests of the pure core
  (`ferrocell::Color` + `ferrocell::ColorEngine`). No Arduino, no FastLED,
  no hardware.
- `run_tests.sh` — builds the tests with the system `g++ -std=c++17` and runs
  them; exits non-zero on any failure (CI-friendly). This is the command the
  `core-tests` CI job runs.

```bash
tests/run_tests.sh          # build + run the pure-core unit tests
```

## M6 (issue #7) — to be added

- the web-UI JS-contract harness (Node),
- the 3rd CI job,
- a unified runner tying the jobs together.
