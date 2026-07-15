# ReplayPlayer Renderer — Milestone 2 Gate

**Authority:** `docs/visual-grammar.md` v1.1 (+ banked non-binding addendum v2). This
document records the M2 gate results (G1 / G1b / G2), the coverage cross-check, the
documented limitations, and the verdict.

**Verdict summary:** **G1 PASS**, **G1b PASS**, **G2 coverage + zero-placeholder PASS**;
two G2 sub-requirements are **OPEN and disclosed** below (per-family screenshots; and a
family-covering subset was PIE-run rather than all 26 individually). Nothing was fixed
or papered over silently.

## Documented limitations (accepted for M2)

- **Shield shell lifetime is a duration approximation.** No shield-end event exists in the
  corpus — only `ShieldGranted`, carrying a `duration` timing. The shell despawns at
  `grant.t + duration` on the shared SimTime clock (Law 2 timings clause), not on a discrete
  expiry/consumption event. Empirically exact on the gate corpora. **Future fix:** track
  cumulative absorbed from damage events and pop on exhaustion.
- **Crunch audio is a deferred asset.** Law 6 crunch is implemented as three tiers keyed to
  the damage fraction, played through optional per-tier `USoundBase` properties. No concrete
  samples are assigned yet (null => silent); the tiering + trigger stand. Camera shake carries
  the felt impact meanwhile.
- **Shield element sigil** activates only for elemental shields; corpus shields carry no
  element (omni => neutral-white shell, per grammar). It lights up once a shield event or
  concept path supplies an element.

## Process note

- Commit `a37e78d` ("check whatsup") contains the M2-C1 slice. It was committed manually
  mid-step under a non-descriptive message; history is append-only here, so the message
  stands and the mishap is recorded per ruling.

## G1 — three M1 fights, full visuals AND juice on, byte-identical

Full protocol: visuals on, `bJuiceEnabled` true, 1x and 4x, extractor
(`scripts/extract-rendered.py`) output byte-diffed (`cmp`) against `docs/references/`.

| fixture | 1x vs reference | 4x vs reference | 1x ≡ 4x |
|---|---|---|---|
| frost-ember-seed1 | byte-identical | byte-identical | yes |
| warden-vs-duelist-t2-seed1 | byte-identical | byte-identical | yes |
| shadow-duel-t3-seed1 | byte-identical | byte-identical | yes |

0 `LogReplay` errors. **G1 PASS.** Hit-stop pauses the shared clock only, so event order
and testimony are invariant to juice and speed.

## G1b — juice on vs off byte-identical (frost-ember)

| run | vs reference |
|---|---|
| juice ON, 1x | byte-identical |
| juice ON, 4x | byte-identical |
| juice OFF, 1x | byte-identical |

`diff` juice-ON vs juice-OFF: empty. `diff` 1x vs 4x with juice: empty. **G1b PASS.**

## G2 — coverage, showcases, zero placeholders

### Concept-element tinting wired (prerequisite)
`LoadManifestElements` reads `Content/Replays/Showcases/manifest.json` (spell id -> element)
and each cast sets the concept level of the two-level law, so showcase spells wear their
concept palette. Committed in M2-H1.

### Coverage cross-check (manifest tokens vs implemented assets) — zero gaps
Elements used {light, fire, earth, water, air, shadow} ⊂ 8 implemented (MPC_Elements +
ReplayVisualGrammar). Verbs used {damage, heal, shield, applyStatus, modifyStat, displace,
spawnZone} all have Niagara/material assets. Deliveries {targetUnit, projectile, self,
groundAoE} all implemented. Corpus statuses {burn, chill, freeze, slow, blind, stealth,
vulnerable, regen, root, weaken, haste, bleed} ⊂ the 20-status glyph table. **No gaps.**

| spell | element | delivery | clause verbs |
|---|---|---|---|
| beacon | light | targetUnit | applyStatus, damage, modifyStat |
| blaze | fire | groundAoE | applyStatus, damage, displace, spawnZone |
| cairn | shadow | self | applyStatus, shield |
| cairn-2184858c | earth | self | modifyStat, shield |
| cinder-rock | earth | projectile | applyStatus, damage, shield |
| conflagration | fire | groundAoE | applyStatus, damage, displace, spawnZone |
| dewdrop-lens | water | self | heal, modifyStat |
| drift | air | self | displace, modifyStat |
| droplet | water | self | heal |
| dust-devil | air | groundAoE | applyStatus, displace, spawnZone |
| ember | fire | projectile | applyStatus, damage |
| glimmer | light | self | modifyStat |
| gust | air | targetUnit | displace |
| hearthstone | earth | self | applyStatus, damage, shield |
| lighthouse | light | groundAoE | applyStatus, damage, heal, modifyStat, spawnZone |
| mist | air | groundAoE | applyStatus, heal, spawnZone |
| mudstone-mirror | earth | self | modifyStat, shield |
| murk | shadow | groundAoE | applyStatus, displace, modifyStat |
| murk-7ff0b332 | shadow | groundAoE | applyStatus, modifyStat, spawnZone |
| pebble | earth | self | shield |
| penumbra | shadow | self | applyStatus, modifyStat |
| pyre-cairn | fire | groundAoE | applyStatus, damage, shield, spawnZone |
| riverstone | earth | self | applyStatus, shield |
| smoke | shadow | groundAoE | applyStatus, modifyStat, spawnZone |
| steam | air | groundAoE | applyStatus, damage |
| umbra | shadow | targetUnit | applyStatus |

### Zero placeholder / missing-asset
The renderer has no placeholder fallback; every FX/material is loaded by soft path in
BeginPlay. Across the **entire** M2 validation session (all fights + showcases): **0
`LogReplay` errors, 0 warnings, 0 failed-to-load**. Combined with the zero-gap coverage
above, no showcase can produce a missing-asset render. **PASS.**

### Live showcase playthrough — all 26 individually, 4x, zero errors (ruling-mandated re-run, 2026-07-15)
Per the G2 ruling, every one of the 26 showcases was PIE-run **individually at 4x** and played
through to completion. Method: MCP-driven PIE — for each showcase the placed `ReplayPlayer`
had its `ReplayPath` set to `Replays/Showcases/<name>.replay.json` and `SpeedMultiplier` set
to 4, PIE was started (`PlayMode_InViewPort`), the run was held until the
`ReplayPlayer: fight end` line confirmed every event had fired, then PIE was stopped with a
~3s idle boundary before the next (the batch was recorded to one video with seekable
boundaries). For each showcase the rendered REPLAY| line count was extracted from
`Saved/Logs/MyProject.log`; **it equals the showcase's event count in every case**, which is
the proof that the full event stream fired. **Zero `LogReplay` errors or warnings** in every
run's log segment. The composition-stress cases ran clean: **lighthouse** (5 verb families in
one cast, 76 lines), **murk-7ff0b332** (64), **conflagration** (43), **blaze** (41).

| showcase | duration (s) | REPLAY lines | errors |
|---|---|---|---|
| beacon | 6.4 | 5 | 0 |
| blaze | 6.4 | 41 | 0 |
| cairn | 6.4 | 4 | 0 |
| cairn-2184858c | 6.4 | 3 | 0 |
| cinder-rock | 6.4 | 15 | 0 |
| conflagration | 6.8 | 43 | 0 |
| dewdrop-lens | 6.4 | 4 | 0 |
| drift | 3.5 | 3 | 0 |
| droplet | 1.0 | 2 | 0 |
| dust-devil | 6.4 | 20 | 0 |
| ember | 6.0 | 14 | 0 |
| glimmer | 6.0 | 2 | 0 |
| gust | 1.0 | 2 | 0 |
| hearthstone | 11.8 | 5 | 0 |
| lighthouse | 17.3 | 76 | 0 |
| mist | 6.0 | 13 | 0 |
| mudstone-mirror | 11.8 | 4 | 0 |
| murk | 6.0 | 5 | 0 |
| murk-7ff0b332 | 17.3 | 64 | 0 |
| pebble | 6.0 | 2 | 0 |
| penumbra | 6.4 | 4 | 0 |
| pyre-cairn | 11.8 | 30 | 0 |
| riverstone | 11.8 | 24 | 0 |
| smoke | 8.4 | 34 | 0 |
| steam | 3.9 | 8 | 0 |
| umbra | 6.0 | 3 | 0 |

All 26 showcases: **26/26 played through, 430 REPLAY| lines total, 0 errors.**

### Screenshots — one per verb & delivery family (CAPTURED 2026-07-15)
Eight stills committed to `docs/screenshots/`, one per verb & delivery family. Captured
autonomously (tooling blocker resolved): a PowerShell `Graphics.CopyFromScreen` of the
editor during PIE, timed to each family's defining event (first DamageDealt, first Healed,
first ShieldGranted, StatusApplied, StatModified, Displaced, ZoneSpawned+~2s, targetUnit hit)
plus a small bloom lead, each verified non-uniform (stdev ~63, ~120-130 distinct colours).

| family | representative | file |
|---|---|---|
| damage / projectile | ember | `damage-projectile-ember.png` |
| heal / self | droplet | `heal-self-droplet.png` |
| shield | cinder-rock | `shield-cinder-rock.png` |
| applyStatus | umbra | `applyStatus-umbra.png` |
| modifyStat | glimmer | `modifyStat-glimmer.png` |
| displace | gust | `displace-gust.png` |
| spawnZone / groundAoE | smoke | `spawnZone-groundAoE-smoke.png` |
| targetUnit | beacon | `targetUnit-beacon.png` |

## Verdict

G1 (three fights, juice on, 1x/4x) **PASS** — byte-identical to `docs/references/` at 1x and 4x,
re-verified on a clean-rebuilt binary and again with the Step-3b browser/toggle build active.
G1b (juice on/off) **PASS**. G2: concept-element tinting wired; coverage zero-gap and zero
placeholder/missing-asset **PASS**; **all 26 showcases** PIE-run individually at 4x, played
through fully, **0 errors** (table above); per-family screenshots **captured** (table above).
**Both previously-open G2 sub-requirements are now closed.** **G2 PASS.**

G3 remains a separate registered protocol (fresh tester); its apparatus — the showcase browser
and the `bTextVisible` toggle on `AReplayPlayer` — ships in this build (commit `M2-3b`,
presentation-only, frost-ember G1 re-checked byte-identical). The default map fix
(`GameDefaultMap`/`EditorStartupMap` -> `/Game/Untitled`) is committed so a plain Play delivers
the ratified experience.
