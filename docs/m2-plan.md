# M2 Phase 3 — Implementation Plan (visual grammar realization)

**Authority:** `docs/visual-grammar.md` (LAW, ratified 2026-07-12). Where this plan and
the law differ, the law wins. This document maps **every token present in the corpus**
(26 showcases + 3 M1 fights) to a concrete Unreal asset or renderer parameter, and
records the deviations the corpus forces.

**Invariant that outranks everything:** the C# sim is the sole combat authority. The
renderer computes no fight logic. `AReplayPlayer` echoes each event's `canonical`
field verbatim as `REPLAY| …`; that testimony is byte-frozen by G1/G1b and must not
change for any visual, juice, speed, or camera reason.

---

## 1. Derivation architecture (Law 1: no per-spell authored effect)

The renderer is a **lookup pipeline**, authored once:

```
event JSON token  ──▶  grammar mapping (data tables in C++/data-asset)  ──▶
      (element, verb, delivery, band, status)         concrete asset + parameters
                                                  ├─ MPC_Element params (colors)
                                                  ├─ Niagara system (verb archetype) + user params
                                                  ├─ decal / trail (delivery)
                                                  └─ sigil mesh/material (status)
```

Nothing keys off a spell **id** or **name**. Every visual is selected by
`(verb, element, delivery, band)` tuples read from the event and the spell header.
A new spell with the same tokens renders identically with zero new authoring.

**Module deps to add** (`MyProject.Build.cs`, currently Core/CoreUObject/Engine/Json/
JsonUtilities): add `Niagara` and `NiagaraCore` (spawn/parameterize systems from C++),
and `UMG`+`Slate`+`SlateCore` if damage numbers move from DrawDebugString to UMG. Decals
use Engine. No gameplay/combat module is ever added — the design ban stands.

**Asset root:** `/Game/M2/` with subfolders `Palettes/`, `Niagara/`, `Materials/`,
`Decals/`, `Sigils/`. Unreal prefix conventions apply (`MPC_`, `NS_`, `M_`, `MI_`,
`NSG_`); the `YYMMDD_` document convention does **not** apply to engine assets.

---

## 2. Elements → palette (Step B). Two-level element law (Law 3)

**Case normalization (deviation D1):** the grammar and manifest name elements in
lowercase (`fire`); event payloads capitalize them (`Fire`, `Arcane`). The renderer
lowercases before lookup. Both forms map to one palette.

Eight palettes as a `MaterialParameterCollection` **MPC_Elements** — one primary+secondary
`LinearColor` pair each (the grammar's named colors realized as concrete sRGB; presented
for ratification, values chosen to match the named hues):

| element | primary (name → sRGB) | secondary (name → sRGB) | motif | feel |
|---|---|---|---|---|
| fire   | ember orange `#FF7A1A` | deep red `#8C1A0D`   | rising sparks, licks | additive, hot glow |
| water  | deep blue `#123A8C`    | cyan `#2FC7E0`       | droplets, arcs | refractive, soft |
| earth  | ochre `#B8863B`        | slate `#5A6470`      | shards, dust | opaque, heavy |
| air    | pale cyan `#BFEFFF`    | white `#FFFFFF`      | streaks, ribbons | translucent, quick |
| light  | warm white `#FFF3D6`   | gold `#FFC64A`       | rays, glints | bloom, clean |
| shadow | violet `#6A3AA0`       | near-black `#120A1F` | smoke, tendrils | darkening, matte |
| arcane | magenta `#E040FB`      | violet `#7B2FBF`     | geometric runes | crisp, synthetic |
| nature | leaf green `#4F9E3A`   | moss `#6B7B3A`       | leaves, spores | organic, drifting |

**Two-level resolution** (exactly Law 3), computed per clause-effect:
1. **Concept palette** = spell header element (manifest `element` / replay header). This is
   the spell's base palette.
2. **Clause tint** = the event payload's own element (`DamageDealt.element`,
   `StatModified.statElement`, …) when present → tint that clause's effect with it.
3. **Absent** → inherit the concept palette.
4. **Bare spell** (no concept element): palette of the highest-share clause's element;
   none anywhere → **arcane** (arcane-neutral).

Niagara reads the resolved `PrimaryColor`/`SecondaryColor` as user params (systems don't
read the MPC directly for per-instance tint; the C++ renderer resolves the pair and pushes
it per spawn). Materials that want a static element tint read MPC_Elements.

---

## 3. Verbs → Niagara archetypes (Step C). One system per archetype

Event types in the corpus (count): DamageDealt 210, StatusApplied 130, ZoneTicked 94,
CastStarted 81, StatModified 79, StatusRemoved 58, Healed 45, Displaced 19,
ShieldGranted 18, ZoneSpawned 9, ZoneExpired 8, CastFailed 4.

| grammar verb | event(s) | Niagara/asset | key user params | signature |
|---|---|---|---|---|
| damage | DamageDealt | `NS_Damage` | Primary/Secondary, Scale(share), Flash, CrunchTier | impact burst + brief flash at target |
| heal | Healed | `NS_Heal` | colors, Scale(effective) | motes rise & converge, soft completion pulse |
| shield | ShieldGranted | `NS_Shield` (+`M_Shield`) | colors, bElemental, ElementSigil | shell; ripple at hit; gentle expiry pop; elemental=tinted+sigil, omni=neutral white |
| applyStatus | StatusApplied | `NS_StatusDock` + sigil (§6) | Sigil, Tint | sigil rises & docks over head; aura tint while active |
| (status removal) | StatusRemoved | `NS_SigilShatter` | Sigil | sigil shatters |
| modifyStat | StatModified | `NS_StatArrows` | bUp, StatGlyph | arrow stream up-gold/down-violet + faint ring; glyph from §6 style |
| displace | Displaced | `NS_DisplaceTrail` + `NS_DustBurst` | Mode, From, To | motion trail on body; dust at origin & landing |
| spawnZone | ZoneSpawned/Ticked/Expired | `NS_ZoneEdge` + decal (§4) | colors, Radius, ShapeCircle | edge pulses each tick; expiry contracts & fades |
| summon/transform/dispel | (no corpus) | `NS_Summon`,`NS_Transform`,`NS_Dispel` | — | spec'd, built minimal per grammar; not exercised by G2 |

Restraint (Law 5): a per-spell simultaneous-emitter cap (constant `kMaxConcurrentEmitters`)
enforced in the renderer; effects scale with bands but never occlude the arena or a status row.
Validation order for Step C: build `NS_Damage` first and validate against the **frost-ember**
fixture (frost-bolt/fireball/ice-lance/scorch damage + Burn/Chill/Freeze status), before the rest.

---

## 4. Deliveries → trajectory & telegraph (Step D)

DeliveryType tokens (manifest): `self` 11, `groundAoE` 10, `targetUnit` 3, `projectile` 2.

**Deviation D2:** the grammar's delivery table names three cases (targetUnit/self/groundAoE)
and describes targetUnit *as* "projectile … interpolated cast-event → effect-event". The
manifest uses a distinct fourth token `projectile` (ember, cinder-rock). Resolution:
`targetUnit` and `projectile` both map to the **projectile trajectory** (interpolate from the
CastStarted position (caster) to the effect event position (target); gap ≈ 0 → hitscan streak).
They are treated as synonyms for delivery; noted here so the mapping is explicit, not silent.

| delivery | trajectory | telegraph | data source |
|---|---|---|---|
| targetUnit / projectile | projectile with element trail, CastStarted→effect-event interp; gap≈0 → hitscan streak | muzzle flash at caster | positions from entities/events (Law 2); `speed:fast` band → higher velocity |
| self | radial burst from caster | brief windup from castTime | caster position |
| groundAoE | telegraph decal at cast, **fills** at ZoneSpawned | decal appears at CastStarted, alpha ramps to spawn | `ZoneSpawned.center/size/shape` — shape=`Circle` (only value in corpus), radius from `size` |

Decal: `M_ZoneDecal` element-tinted, low-alpha fill; radius from event `size` (sim radius,
Law 2) with band `size` tiers (large/medium/small) only as cosmetic fallback.

---

## 5. Bands → cosmetic intensity (cosmetic-only; events outrank, Law 2)

Band universe in corpus: castTime{quick,normal}, cooldown{medium,long,short},
cost{moderate,high,low,trivial}, duration{medium,short,long}, interval{medium,slow},
multiplier{major,minor}, size{large,medium,small}, speed{fast}.

| band | maps to | note |
|---|---|---|
| share (per event) | effect scale ≈ `0.6 + 0.8×share` | primary intensity driver |
| size | decal/AoE scale tiers matched to sim radii | event radius outranks |
| multiplier (=amplify) | detonation flourish: minor→spark ring, major→shockwave, extreme→white-flash | **D3:** corpus has only minor/major; extreme built but unexercised |
| castTime | windup length from the cast event's own timing | quick/normal |
| interval | pulse cadence — driven by actual ZoneTicked events, not the band | band is a hint only |
| speed | projectile velocity | only `fast` in corpus |
| duration | **NEVER** from bands — ends on the removal event | Law 2 hard rule |
| cost, cooldown | **no visual** (gameplay economy, not look) | intentionally unmapped |

---

## 6. Statuses → sigil + body effect (Step E)

Grammar status table (18, full vocabulary regardless of corpus): burn, chill, freeze, slow,
root, stun, blind, stealth, haste, silence, fear, poison, bleed, shock, weaken, invulnerable,
unstoppable, disarm. All 18 get a sigil this milestone (simple geometric/text-glyph acceptable;
legibility over beauty).

**Deviation D4 (must-fix for G2):** the corpus contains two statuses **not** in the grammar
table — `Vulnerable` (25) and `Regen` (1). G2 forbids placeholder/missing-asset renders across
all showcases, so both need real sigils. Proposed, consistent with the table's idiom:
`Vulnerable` = downward-cracked-shield glyph, victim-red tint; `Regen` = green plus/heart
glyph with slow rising motes (heal-adjacent). Flagged for ratification; built unless overruled.

Sigil implementation: one shared `NS_StatusDock` + a `Sigil` texture/glyph parameter per
status (materials `MI_Sigil_<name>` from a master `M_Sigil`), docked above the head; aura tint
via a body MID param while active; `NS_SigilShatter` on StatusRemoved. Body effects per table
(ember drip, frost rim, ice sheath, dragging trail, etc.) as small Niagara riders keyed by the
same status name — built where cheap, text-glyph fallback where not, never a blank placeholder.

StatModified `statKind` tokens: CritChance, Evasion, MoveSpeed, Armor, DamageOut. Direction from
`amountPct` sign (up gold / down violet). **D5:** `statElement` is never populated in the corpus →
stat mods are element-neutral (direction color only), consistent with the grammar's up/down rule.

Displaced `mode` tokens: Push 12, Dash 5, Teleport 2. **D6:** grammar says "push vs pull read by
trail direction"; corpus has Push/Dash/Teleport (no explicit pull). Mapping: Push→trail away from
caster + landing dust; Dash→self-motion trail along from→to; Teleport→blink (fade-out at `from`,
pop-in at `to`, no continuous trail). Read from `from`/`to`/`mode` (Law 2).

---

## 7. Verbless events

`oom` → blue fizzle + slump; `death` → grey-out, fall, element-tinted wisp; `CastFailed`
(reason `stunned` only in corpus) → fizzle puff + red X sigil; `fightEnd` → winner banner (as M1,
already implemented). Death grey-out already exists in M1 `RenderEventVisual` (killed=true path);
extend with the element-tinted wisp.

---

## 8. Law 6 — Juice is data (Step F)

Derives from event data; modulates **only** the shared playback clock and the camera — never an
individual event, never the log.

- **Hit-stop:** on DamageDealt, `fraction = amount / victim.maxHp`. Pause the shared playback
  clock 40 ms at fraction ≥ 0.25, 90 ms at ≥ 0.50, none below. Implemented as a freeze on the
  same `SimTime` accumulator that drives event firing — because the clock stops, event order and
  the REPLAY| log are unchanged (this is what G1b proves).
- **Screen shake:** amplitude ∝ fraction, capped at a readable max (`kShakeCap`); non-damage
  events never shake.
- **Crunch:** three impact-sound tiers keyed to the same fractions (<0.25 / ≥0.25 / ≥0.50).
- **Toggle:** `UPROPERTY bool bJuiceEnabled` (default true). Off ⇒ no hit-stop, no shake, no
  crunch. G1b requires on-vs-off REPLAY| logs byte-identical.

**Critical for G1/G1b:** hit-stop must pause the clock, not skip or reorder events. The Tick loop
already fires events by `T <= SimTime` in index order; freezing `SimTime` for the hit-stop window
preserves both order and content at 1x and 4x.

---

## 9. Camera law + damage numbers (Step G)

- **Camera:** three-quarter angle, pitched down ~55°, framing both fighters with margin, smoothly
  tracking their midpoint, no cuts. Canonical everywhere. Replaces the M1 straight-down overhead,
  which moves behind `UPROPERTY bool bDebugOverheadCamera` (default false).
- **Damage numbers:** floating, **element-tinted** (resolved clause element), **scaled by amount**,
  always facing the camera. Extends the M1 `DrawDebugString` `-N` label with tint+scale (or UMG).

---

## 10. Gates (Step H) → docs/renderer-gate-milestone-2.md

- **G1:** 3 M1 fights, full visuals + juice **on**, 1x and 4x; REPLAY| logs byte-identical to
  `docs/references/` via `scripts/extract-rendered.py` (reused).
- **G1b:** juice on vs off → byte-identical logs.
- **G2:** play all 26 showcases; **zero** placeholder/missing-asset renders; coverage table
  generated from `manifest.json` cross-checked against implemented assets; one screenshot per verb
  family and per delivery family into `docs/screenshots/`.
- Any failure is filed verbatim in the gate doc; nothing is fixed silently; execution stops.

---

## 11. Deviations & flagged findings (surfaced, not silently resolved)

- **D1 element case:** payloads capitalize (`Fire`), grammar lowercases — normalized in lookup.
- **D2 delivery synonym:** manifest `projectile` ≡ grammar `targetUnit` projectile trajectory.
- **D3 amplify.extreme:** built but no corpus material (only minor/major present).
- **D4 statuses off-table:** `Vulnerable`, `Regen` in corpus but absent from grammar table;
  sigils proposed above; **needs ratification**, built unless overruled (G2 would otherwise fail).
- **D5 statElement empty:** stat mods render element-neutral (direction color only).
- **D6 displace modes:** Push/Dash/Teleport mapped; grammar's "pull" has no corpus instance.
- **D7 nature element:** in the grammar palette but absent from all corpus tokens; palette built
  for completeness, unexercised by G2.
- **D8 arcane as concept:** appears only as payload element / neutral fallback, never as a manifest
  concept element; covered by the bare-spell rule.

## 12. Build order (commit after each lettered step)

A plan (this doc) → B palettes/MPC → C verb Niagara (validate frost-ember) → D deliveries →
E status sigils → F Law 6 juice → G camera + numbers → H gates. Each step ends with a commit whose
message states its contents.
