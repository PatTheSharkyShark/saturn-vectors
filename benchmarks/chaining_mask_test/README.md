**Chaining Mask Test — Summary**

- **Purpose:** Validate whether mask-producing vector instructions write mask state that can be immediately consumed (chained) by subsequent masked vector operations.

- **Test harness:** C benchmark using RVV inline assembly; vectors `a`, `b`, `out` (length `N=16`). RVV configured with `vsetvli` for `e32,m1` and tests use `vmsne.vv`, `vmsgt.vx`, `vadd.vv`, `vsub.vv`, and masked stores/loads.

- **Tests implemented:**
  - **Test 1 — vmsne.vv -> vadd.vv (masked):** produce mask via `vmsne.vv v0, v8, v9` then `vadd.vv v16, v8, v9, v0.t`.
    - Expected: all lanes active (a != b), `out[i] = a[i] + b[i]` → 3, 6, 9, ... 48.
    - Simulator (user run): cycles: 140; out (truncated): 3 6 9 ... 48

  - **Test 2 — Partial mask (even/odd pattern):** craft `b` so mask is set only on odd indices and perform masked add.
    - Expected: only masked lanes updated; others remain unchanged/zero depending on tail/agreement policy.
    - Simulator (user run): cycles: 108; out (truncated): 3 104 9 108 ... 45 132

  - **Test 3 — Multi-stage chaining:** generate mask from a scalar compare (`vmsgt.vx v0, v8, 8`), then perform a masked `vadd.vv` followed by a masked `vsub.vv` (both using the same mask across the chain).
    - Expected: lanes where `a>8` updated; functional result for active lanes equals `b` (because (a+b)-a = b).
    - Simulator (user run): cycles: 130; out (truncated): 3 104 9 108 15 112 21 116 109 110 111 112 113 114 115 116

  - **Test 4 — Mask from computed vector result:** (added) compute `temp = a + b` then generate mask from `temp` (`vmsgt.vx v0, v10, TH`) and use it in a masked add that consumes `temp` as an operand. This verifies chaining when the mask depends on an earlier vector computation.
    - Status: code added to benchmark; simulation not yet run.
    - Expected: lanes where `(a+b) > TH` are updated (`out[i] = (a+b)+b`); other lanes unchanged.

  - **Test 5 — No-chaining control (store+reload):** (added) same computation as Test 4 but explicitly `vse32.v` the computed vector to memory and `vle32.v` it back before generating the mask and using it. This should prevent low-latency forwarding/chaining and acts as a control.
    - Status: code added to benchmark; simulation not yet run.
    - Expected: same functional results as Test 4, but any forwarding-based performance win should disappear (higher cycle count) and chaining-specific microbehavior is prevented.

- **Files:**
  - Benchmark source: [generators/saturn/benchmarks/chaining_mask_test/main.c](generators/saturn/benchmarks/chaining_mask_test/main.c#L1-L400)

- **How to build & run locally:**

```bash
cd generators/saturn/benchmarks
make
cd ../../sims/verilator
./run_sim.sh chaining_mask_test
```

- **Current status & next steps:**
  - Tests 1–3: built and run by user; simulator outputs above confirm masks produced by vector instructions are consumed by subsequent masked ops in the same sequence (i.e., chaining observed functionally).
  - Tests 4–5: added to the benchmark to further validate chaining vs. no-chaining control; need to compile and run to collect cycle counts and confirm micro-behavior differences.
  - Recommend running the build & simulation commands above and attaching the simulator stdout; I can then parse exact cycle counts and add a small results table to this file.

**Notes:**
- I did not modify RTL or generator logic — the benchmark probes existing mask write/read semantics and the sequencer/forwarding behavior implemented in the backend.
- The functional outputs for Tests 1–3 were provided from your simulator run; avoid treating cycle counts as definitive performance metrics until Tests 4–5 are run under the same invocations for comparison.
