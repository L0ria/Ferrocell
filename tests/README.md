`tests/` holds the host-side test harness (no Arduino, no hardware).

## Unit (host) — `tests/unit/*.cpp` (M2 #3, M3 #4, M5 #6)

Pure-core unit tests, each a self-contained binary with its own `main()`:

- `unit/core_test.cpp` — `Color` + `ColorEngine` (M2)
- `unit/settings_test.cpp` — `Settings` defaults + normalize/clamping (M3)
- `unit/api_test.cpp` — `Json` + `Api` (the JSON API contract, M5)

`run_tests.sh` builds them with the system `g++ -std=c++17` and runs them;
exits non-zero on any failure (CI-friendly). This is the command the
`core-tests` CI job (job 1 of 4) runs.

```bash
tests/run_tests.sh          # build + run the pure-core unit tests
```

## UI contract (Node) — `tests/ui/` (M6, issue #7)

- `ui/harness.mjs` — dependency-free Node harness (Node >= 18; CI uses Node
  22) that loads `ferrocell/data/ui.js` and asserts the JS side of the web-UI
  contract: endpoints, payload shapes, the color-wheel hue/saturation
  mapping, HSV<->RGB, and the presets. It also compiles + runs
  `ui/cpp_probe.cpp` (the pure core) and asserts the JS helpers agree with
  the C++ core (the JS<->C++ cross-check). Skips the cross-check with a
  warning if `g++` is unavailable.
- `ui/cpp_probe.cpp` — the tiny pure-core probe the harness compiles
  (`Color` / `Settings` / `Api` only; no Arduino).
- `ui/package.json` — metadata only (`type: module`, `npm test`); zero
  dependencies, so CI runs `node tests/ui/harness.mjs` directly.

```bash
node tests/ui/harness.mjs   # run the web-UI JS-contract harness
```

This is the command the `ui-contract` CI job (job 2 of 4) runs.
