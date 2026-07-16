# G1 — the REPLAY| byte-identity gate

G1 is the renderer's core gate: the `REPLAY| ` testimony a playback emits must be **byte-identical**
to the recorded truth in `docs/references/`. The C# simulation is the sole authority; the renderer
computes no combat logic, so its log must never diverge from the reference — presentation work
(animation, camera, juice, FX) must move **zero bytes**.

## Cadence

- **fast-G1** — frost-ember at **4× only**. Run per lettered step during a phase, as the quick check
  that a presentation change didn't disturb the log.
- **full G1** — frost-ember **and** warden, **1× and 4×** (four runs). Run only at **step-E stops and
  before tags**. G1b (juice on vs off byte-identical) rides along at full-G1 stops.

References currently pinned: `frost-ember-seed1` (116 lines), `warden-vs-duelist-t2-seed1` (96 lines),
`shadow-duel-t3-seed1`. References are LF, single trailing newline (see the committed `.gitattributes`).

## Instruments

### In-engine: the `G1` console command (standard instrument)

Run inside PIE (the renderer only ticks/emits there). Open the PIE console and type:

```
G1 <fixture> <speed>
```

e.g. `G1 frost-ember-seed1 4`. The command finds the `ReplayPlayer`, restarts playback on that fixture
at that speed, captures the renderer's own emitted canonical lines, and at fight end **byte-diffs** them
against `docs/references/<fixture>.reference.txt`, logging one of:

- `G1 PASS: <fixture> @ <speed>x - N REPLAY| lines BYTE-IDENTICAL to reference`
- `G1 FAIL: <fixture> @ <speed>x - first divergence at line K …` (with the reference vs rendered pair)
- `G1 FAIL: <fixture> - cannot read reference …` / `could not load fixture …` (setup failures)

Instrument-only: it reads the same lines `EmitCanonical` already logs and never alters them or the event
order, so it cannot change what G1 measures. Implemented in `ReplayPlayer.cpp` (`StartGate`/`FinishGate`
+ the `FAutoConsoleCommandWithWorldAndArgs` registration).

A new `G1` preempts an in-flight gated run (via the replay reload), so it emits **no** verdict for the
interrupted run — there are no spurious passes.

### External: the scratchpad harness (fallback / automation)

`gate_all.py` (full G1) and `g1b.py` drive PIE over MCP, extract each run's `REPLAY| ` segment from the
log, and `cmp` it against `docs/references/`. Kept as the automatable cross-check; the in-engine `G1`
command is the standard hand instrument.
