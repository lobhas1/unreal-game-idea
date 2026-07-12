# ReplayPlayer Renderer — Milestone 2 Gate (in progress)

**Authority:** `docs/visual-grammar.md` v1.1. This document collects the M2 gate results
(G1 / G1b / G2) and documented limitations. It is completed in Step H; sections below are
filled as steps land.

## Documented limitations (accepted for M2)

- **Shield shell lifetime is a duration approximation.** The corpus carries no shield-end
  event — only `ShieldGranted`, which carries a `duration` timing (e.g. `dur=5`). The shell
  therefore despawns at `grant.t + duration` on the shared SimTime clock (Law 2 timings
  clause), not on a discrete expiry/consumption event. Accepted for M2 as empirically exact
  on all gate corpora. **Future fix:** track cumulative absorbed amount from damage events
  and pop the shell when absorption is exhausted, so consumption (not just timeout) ends it.

## Process note

- Commit `a37e78d` ("check whatsup") contains the M2-C1 slice (NS_Damage wired + G1-validated).
  It was committed manually mid-step under a non-descriptive message; history is append-only
  in this project, so the message stands and the mishap is recorded here per ruling.

## Incremental G1 validations during Step C (full protocol: visuals on, 1x and 4x, byte-diff
vs `docs/references/`, extractor reused)

| Step | Fixture | Paths exercised live | 1x | 4x | 1x≡4x |
|------|---------|----------------------|----|----|-------|
| C1  | frost-ember-seed1 | damage | identical | identical | yes |
| C4  | frost-ember-seed1 | damage, heal, applyStatus, shield | identical | identical | yes |
| C5  | frost-ember-seed1 | heal-converge, shield-shell | identical | identical | yes |
| C5b | warden-vs-duelist-t2-seed1 | shield×2, displace×2, zone(1 spawn+15 ticks), statModified×22 | identical | identical | yes |

All: 0 `LogReplay` errors. The formal G1 (three fights, juice on), G1b (juice on/off), and
G2 (26 showcases, coverage, screenshots) gates are run and recorded in Step H.
