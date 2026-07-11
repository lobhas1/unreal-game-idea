# ReplayPlayer Renderer — Milestone 1 Gate

**Date:** 2026-07-11
**Component:** `AReplayPlayer` (`Source/MyProject/ReplayPlayer.{h,cpp}`)
**Verdict: PASS** — the renderer replays recorded fights faithfully, its canonical
testimony is invariant to playback speed, and it matches the C# authority
byte-for-byte on all three fixtures.

## The law under test

The C# simulation is the single authority on what happens in a fight. The renderer
implements no combat logic: it reads recorded events and echoes each event's
canonical projection line verbatim to the Output Log, prefixed `REPLAY| `. That log
is the renderer's testimony. Milestone 1 gates on two claims:

1. **Speed changes pacing only** — never event order, never log content.
2. **The rendered testimony equals the C# reference projection, byte-for-byte.**

## Method

Playback was driven live in the Unreal Editor (Play-In-Editor) via the
`AReplayPlayer` actor placed in `/Game/Untitled`. Each `REPLAY| ` line was collected
from the session Output Log, then reduced to a `*.rendered.txt` with
`scripts/extract-rendered.py` (strip through the `REPLAY| ` marker; LF endings). Each
rendered file was compared to its reference in `docs/references/` with `cmp`
(exact bytes, including the trailing newline).

## Phase 2 step 4 — speed invariance (frost-ember-seed1)

Full playback of the same replay at two speeds, back to back:

| Run | `SpeedMultiplier` | Wall-clock | Sim duration | REPLAY lines |
|-----|-------------------|-----------|--------------|--------------|
| Normal | 1.0 | 37.4 s | 37.6 s | 116 |
| Fast   | 4.0 |  9.4 s | 37.6 s | 116 |

- **Logs identical:** the 116 canonical lines from the 1× run match the 4× run
  line-for-line (`diff` empty). Pacing scaled ~4× (37.4 s → 9.4 s); content did not move.
- **No errors:** `LogReplay` emitted 0 Error and 0 Warning lines across both runs.
  (The only `failed to load` string in the session is an unrelated `LogAudio`
  startup message, not the renderer.)

**Verdict: PASS.**

## Phase 3 — byte-for-byte gate (all three fixtures)

Rendered from real PIE playback, diffed against the C# references:

| Replay | Events / lines | Winner (endReason) | `cmp` vs reference |
|--------|----------------|--------------------|--------------------|
| frost-ember-seed1        | 116 | ember (death)                                  | **PASS** |
| warden-vs-duelist-t2-seed1 | 96 | a-defensive-earth-warden-1 (death)           | **PASS** |
| shadow-duel-t3-seed1     | 113 | a-shadow-assassin-built-on-setup-and-burst-1 (death) | **PASS** |

**Gate: GREEN (3/3 byte-for-byte).**

Artifacts:
- Rendered output: `docs/rendered/*.rendered.txt`
- Reference authority: `docs/references/*.reference.txt`
- Extractor: `scripts/extract-rendered.py`

## Note on line endings

The byte-for-byte gate is line-ending sensitive. `extract-rendered.py` emits LF, so
the reference fixtures must also be LF. A `.gitattributes` pins `docs/references`,
`docs/expected-projections`, `docs/rendered`, and `scripts/*.py` to `eol=lf` so a
Windows checkout (`core.autocrlf=true`) cannot rewrite them to CRLF and break the
gate on line endings alone.
