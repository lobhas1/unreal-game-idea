# Beauty pass — Act 1, Phase 1: skeletal casters + cast archetypes

**Authority:** `docs/visual-grammar.md` (LAW) + THE ANIMATION LAW (this phase): animation is
performance *under* event-given cues. The cast event and its effect event are the clock; a cast
animation stretches/trims its play-rate to fit exactly the data-given window; animation never
delays, reorders, or drives an event. `REPLAY|` testimony stays byte-identical — G1 gates every
substep. Frozen G3 build is `m2-frozen-v2` (never touched); work happens on `master`.

## A0 — scene hygiene (finding)

Investigated the reported "template water plane submerging ground decals." **There is no water
body/ocean actor in the level.** Full actor inventory (via MCP `find_actors`): `SkyAtmosphere`,
`SM_SkySphere` (the single `StaticMeshActor` — sky backdrop, scale 400, ±1.6M bounds; this is the
"blue" seen in overhead captures), `SkyLight`, `VolumetricCloud`, `DirectionalLight`,
`ExponentialHeightFog`, `Landscape` (+ 126 `LandscapeStreamingProxy` + HLOD instances), a default
`BuoyancyManager` (no water bodies attached), `PlayerStart`, and our `ReplayPlayer`. No
`WaterBodyOcean`/`WaterZone`.

So decals are **not** submerged by water. The zone-decal mechanism is confirmed working — the fire
groundAoE showcase **blaze** renders a clear circular orange translucent fill
(`docs/screenshots/spawnZone-groundAoE-blaze-v2.png`). Smoke's decal is simply hard to see
(shadow-violet at ~0.35 alpha over the light checker floor, and its large 1000 uu zone extends past
the arena's `ReplayFloor` edge into open sky in places). Per human ruling, A0's water-removal is
skipped (nothing to remove); blaze stands as the decal evidence.

## A — inventory

**Skeletal meshes:** `Content/AnimStarterPack/UE4_Mannequin/Mesh/SK_Mannequin` (skeleton
`UE4_Mannequin_Skeleton`, physics asset `SK_Mannequin_PhysicsAsset`). It is the only skeletal mesh
available, so it is the **working mannequin** for phase 1.

**Animations (UE4 Animation Starter Pack — a shooter set):** Idle_Rifle_Hip (+Break1/2), Idle_Pistol,
Fire_Rifle_Hip, Fire_Shotgun_Hip, Reload_Rifle_Hip, Reload_Pistol, Reload_Shotgun_Hip, Jog_*_Rifle,
Sprint_Fwd_Rifle, Jump_From_Stand, Jump_From_Jog, Death_1/2/3 (+Ironsights/Prone), Hit_React_1..4,
Equip_Rifle/Pistol_Standing, Aim_Space_Hip, plus crouch/prone variants. There are **no bespoke
spellcasting anims**, so the four cast archetypes are mapped onto the closest shooter motions;
a bespoke caster set is a later phase.

## Step-A amendment — Wizard for Battle: PBR rig evaluation (RATIFIED as primary body)

Pack: `Content/BattleWizardPBR`. Evaluated for the four criteria:
- **Skeleton sanity — PASS.** `WizardSM` is a genuine SkeletalMesh, and the pack ships
  `Meshes/UE4_Mannequin_Skeleton` — the wizard is rigged to the **same UE4 mannequin skeleton**
  as SK_Mannequin. So the existing machinery works unchanged: the `hand_r`/`head`/`spine_03`/
  `foot_r` bones all exist, `SocketWorld`/`HeadWorld` resolve, and single-node `PlayAnimation`
  drives it directly.
- **Scale — standard.** Shares the mannequin skeleton (≈180 cm); confirmed at build (spawned
  actor bounds ≈ humanoid, feet on floor).
- **Animations slice into archetypes — PASS.** Rich cast set: `Attack01`, `Attack02Start/Maintain`,
  `Attack03Start/Maintain`, `Attack04`, `Defend*`, `Idle01/02/03`, `Die`, `GetHit`, `Jump*`,
  `PotionDrink`. Each archetype maps to a real clip (below); play-rate fits it to the window.
- **Tint — better than the mannequin.** Body material is `PBRMaskTintMatInst` (mask-tint) with
  three color params `OuterClothes` / `InnerCloth` / `Hair`; the two sides tint the robe warm (A)
  vs cool (B).

**Verdict: rig is sound → Step B builds on the wizard (both sides = wizard, tinted apart); the
mannequin is not used as the fighter body.** (SK_Mannequin/ASP remain in Content, unused by the
casters.)

## Archetype → animation mapping (verb + delivery → wizard montage)

| archetype | triggers (verb / delivery) | exact wizard clip(s) | structure / fit |
|---|---|---|---|
| **THROW** | targeted damage, travelling `projectile` (window ≥ `kHitscanGap`) | `Attack01Anim` (single) | staff-point forward cast; play-rate = len × speed / window so the release lands on the effect event; projectile head spawns at `hand_r`. |
| **SLAM** | `groundAoE` / `spawnZone` (`GroundAoE` delivery) | **composite:** `JumpUpAttackAnim` → `JumpAirAttackAnim` → `JumpEndAnim` | leap-slam reads chibi-bold — a runtime `UAnimComposite` concatenates the three on one timeline; whole composite play-rate-fit to cast→`ZoneSpawned`; foot dust at the feet, decal at the event centre (Law 2). Single-clip fallback `Attack04Anim` if a clip fails to load. |
| **CHANNEL** | `heal`, `shield`, self-buffs (`Self` delivery) | **composite:** `Attack02StartAnim` + looped `Attack02MaintainAnim` | play Start once, then loop Maintain `round((window − startLen)/maintainLen)` times to fill the cast→effect window (looping is the lawful fit for LONG windows — composite length ≈ window ⇒ rate ≈ natural). SHORT windows get zero loops and the Start clip rate-stretches to fit. Fallback: Start alone. |
| **SNAP** | instant/hitscan `projectile` (window < `kHitscanGap`) or unresolved | `Attack03StartAnim` (single) | a quick attack start, play-rate pushed high to fit the near-instant window; from `hand_r`. |

Idle stance = `Idle01Anim`. Death keeps the grey-out (a `DieAnim` montage is a later refinement).
Play-rate law is unchanged (`len × speed / window`); for the composites `len` is the accumulated
composite timeline length. Composites are built at runtime in `PlayCastArchetype` via
`NewObject<UAnimComposite>` + `FAnimSegment`s (`SetAnimReference`/`LoopingCount`/`StartPos`) +
`SetCompositeLength`, played through single-node `PlayAnimation` — no authored montage assets.

**Play-rate law (all archetypes):** `PlayRate = AnimSequenceLength / max(window, epsilon)`, where
`window` is the event-given cast→effect (or cast→zone-spawn) gap in sim seconds. The montage is
sampled against the shared `SimTime` clock only; it emits nothing and gates nothing. If juice
hit-stop freezes the clock, the animation holds with it (it reads `SimTime`). REPLAY| unaffected.

## Socket / origin plan (Step D)

UE4 mannequin bones used as attach points: `hand_r` (throws + projectile head, the M2-D swappable
head attaches here), `head` (name label + status sigils + damage-number origin), a foot/`root`
projection (slam telegraph at the feet), `spine_03`/`hand_r`+`hand_l` (channel emission at
chest/hands). Bone names verified against the skeleton during Step B/D.

## Body swap plan (Step B)

Both combatant capsules become `SK_Mannequin` skeletal-mesh actors, tinted per side via a dynamic
material instance (A = warm, B = cool) so they stay distinguishable. Name labels, status sigils, and
damage-number origins re-anchor from the old cylinder-top offset to the `head` bone. Death grey-out
(the existing killed-path material tint + slump) stays, optionally with a Death_* montage.
**G1 re-check on frost-ember (1x and 4x) after the swap — byte-identical required.**

## Inspection camera mode (Step C revision)

Formalized the front-view need as an **INSPECTION CAMERA MODE**: `CameraMode`, an `EReplayCameraMode`
`UPROPERTY` on the ReplayPlayer with three values —

- **`LawTracking`** *(default)* — the canonical three-quarter tracking view. **This is the committed
  level default**, so a plain Play is always the ratified experience.
- **`FrontInspection`** — a head-on animation view: looks across the matchup along the perpendicular
  to the fighter-separation axis, low pitch, closer than the law camera, so a cast pose is legible.
  This is the mode used to capture the Step-E archetype screenshots.
- **`OverheadDebug`** — the retired straight-down debug view (replaces the old `bDebugOverheadCamera`).

The human flips `CameraMode` freely in-editor for animation work (`Tick` retargets the player view
live when it changes); the placed actor stays on `LawTracking` and no override is saved. Presentation-
only: the mode selects a camera and nothing else — the event loop and the `REPLAY|` echo are untouched,
so G1 byte-identity holds in every mode.

## Gates

Every substep ends with a G1 re-check (frost-ember; full G1 = frost-ember AND warden at Step E),
1x and 4x, byte-diffed vs `docs/references/` — presentation must move zero bytes. G1b spot-check and
four mid-animation archetype screenshots at Step E into `docs/screenshots/act1/`.

## Results log (Step E)

**Body ratification (decided):** the human ratified the **BattleWizardPBR mage** as the primary
body. Rig evaluated sound — `WizardSM`, `Idle01Anim`, and the `Attack*` clips all share
`/Game/BattleWizardPBR/Meshes/UE4_Mannequin_Skeleton`, so the mannequin-era machinery (sockets,
single-node playback) applies unchanged; actor bounds ≈ 127×175 uu (standard humanoid, feet on
floor). Render confirmed in live PIE by the human.

- **B — body swap** (commit `f021797`): both combatants are `WizardSM` skeletal casters, feet on
  floor, looping `Idle01Anim`; robe tinted per side via `PBRMaskTintMat` `OuterClothes`/`InnerCloth`
  (A warm / B cool). Labels/sigils/damage-numbers anchor to the `head` bone; death grey-out drains
  the mask tint to grey.
- **C — cast archetypes** (`f021797`): `PlayCastArchetype` maps verb+delivery → THROW `Attack01Anim`,
  SLAM `Attack04Anim`, CHANNEL `Attack02StartAnim`, SNAP `Attack03StartAnim`; play-rate = clip
  length × speed / window, fit to the event-given cast→effect window per THE ANIMATION LAW.
- **D — socket origins** (`f021797`): projectiles leave `hand_r`, self/channel emit from `spine_03`,
  slam dust at `foot_r`, heal/shield anchor to the chest.
- **C archetype-selection correction** (Live-Coded body tweak, source committed): the corpus models
  nearly every cast with a **~0 cast→effect window**, so the original window-first rule (SNAP when
  window < 0.05) collapsed almost everything to SNAP — SLAM and CHANNEL never fired, only THROW
  (frost-ember's 3 windowed casts) + SNAP. Fixed `PlayCastArchetype` to select **delivery-first**:
  GroundAoE→SLAM, Self→CHANNEL, windowed Projectile→THROW, instant/hitscan or unresolved→SNAP; the
  window still drives play-rate. Now all four differentiate: SLAM on groundAoE showcases, CHANNEL on
  self showcases, THROW on frost-ember, SNAP on instant projectiles.
- **G1 (final/patched build):** frost-ember **1× and 4×** and warden **1× and 4×** all
  **BYTE-IDENTICAL** to `docs/references/` (verbatim `cmp`, no output). The archetype fix is
  presentation-only (event loop + `REPLAY|` echo untouched), verified byte-identical both before and
  after the Live-Coding patch.
- **G1b:** frost-ember juice-**OFF** rendered **BYTE-IDENTICAL** to the reference (== juice-on == ref).
- **Screenshots (`docs/screenshots/act1/`):** four archetypes, one each, captured via a PowerShell
  screen-grab of the live editor viewport (`slam-blaze.png`, `channel-droplet.png`,
  `throw-frost-ember.png`, `snap-ember.png`). Note: MCP `CaptureViewport` cannot capture PIE-spawned
  skeletal actors — it renders an editor gizmo (spoked-wheel proxy), not the skinned wizard — so the
  screen-grab of the real game view was used and each was verified as a genuine editor render (both
  wizards visible, tinted apart, mid-cast with element FX + the browser label).

**(Prior build STOP)** — the above (delivery-first single-clip archetypes) gated green: both
fixtures byte-identical, four screenshots filed from the default camera.

### Step-C revision (leap-slam + looped channel + inspection camera) — pending rebuild + re-verify

Per new Step-C guidance, the archetypes were deepened and the front-view formalized (source changed,
**not yet gate-verified on this build**):

- **SLAM → leap-slam composite** (`JumpUpAttackAnim` → `JumpAirAttackAnim` → `JumpEndAnim`) and
  **CHANNEL → Start + looped Maintain** (`Attack02StartAnim` + N×`Attack02MaintainAnim`, N sized to
  the window; short windows rate-stretch), both built at runtime as `UAnimComposite`s and played via
  single-node `PlayAnimation`. THROW (`Attack01Anim`) and SNAP (`Attack03StartAnim`) unchanged. Exact
  clips now named in the archetype table above. Composite APIs verified against the UE 5.8 headers
  (`FAnimSegment::SetAnimReference`, `SetCompositeLength`, `UAnimationAsset::SetSkeleton`).
- **INSPECTION CAMERA MODE** — `CameraMode` enum property (see section above); replaces
  `bDebugOverheadCamera`.
- **Structural change** (new `UENUM`, new `UPROPERTY`, new members) ⇒ took the **full-rebuild path**
  with the editor closed (Live Coding cannot reinstance it). `MyProjectEditor` rebuilt clean
  (`ReplayPlayer.cpp` + `Module.MyProject.gen.cpp` compiled, `UnrealEditor-MyProject.dll` linked,
  Result: Succeeded).

**Re-verification on the rebuilt binary (this step-E stop — full G1 per the cadence amendment):**

- **Full G1 — PASS.** frost-ember **1× and 4×** and warden **1× and 4×** all **BYTE-IDENTICAL** to
  `docs/references/` (verbatim `cmp`, no output; frost 116 lines, warden 96 lines). The leap-slam
  composite, looped channel, and the camera-mode property moved zero bytes in the `REPLAY|` log.
- **G1b — PASS.** frost-ember juice-**OFF** byte-identical to the reference (116 lines).
- **Four archetype screenshots — refreshed** from the `FrontInspection` camera into
  `docs/screenshots/act1/` (`slam-blaze`, `channel-droplet`, `throw-frost-ember`, `snap-ember`),
  captured via the editor-viewport screen grab (`capture.ps1`; the mode was set on the PIE actor via
  MCP `set_properties` with `CameraMode:"FrontInspection"`, then reset). Each verified by eye: both
  wizards visible head-on, robe-tinted apart (warm vs cool), feet on floor, mid-cast with element FX /
  status sigils / shield shell and the browser label — legible now, unlike the prior tracking-cam set.
  *Note:* a wall-clock capture offset doesn't always land the most dramatic pose frame (e.g. the slam
  leap apex); and a small dark placeholder sphere sits near the warm caster in every frame — a
  pre-existing cosmetic element, not introduced by this change (G1 confirms logic is unchanged).

**Cadence amendment recorded:** per lettered step run **fast-G1** = frost-ember @ 4× only; **full G1**
(frost-ember AND warden, 1× and 4×) only at **step-E stops and before tags**. This stop ran full G1.

**STOP** — Step-C revision gate-green on the rebuilt binary. (Apparatus follow-up: the in-engine `G1`
console command lands as its own task + commit.)

## PAUSED AT STEP C — current archetype status + warden misfit (filed)

Act one is **paused at step C** (not finished). Two runtime bugs were found and fixed after the stop
(commit `8c740d5`, full/fast-G1 byte-identical), then this status was filed. Steps C and D must be
**finished** — including the remap below — before act two's step C (arena integration) may run.

### Playback fixes that landed (commit `8c740d5`)
- **Casts were invisible.** The corpus models nearly every cast with a ~0 cast→effect window (see the
  standing gotcha), so the old play-rate `Len·speed / max(Window, 0.05)` clamped every cast to the 12×
  max — each clip finished in ~50 ms and froze on its last pose. Confirmed by a temporary single-node
  diagnostic (`rate=12.00`, `playing=0`, frozen `pos`). Fix: floor the window at the clip length
  (`W = max(Window, Len)`) ⇒ a ~0-window cast plays at **natural speed**; resume the looping idle once a
  non-looping cast ends so the wizard breathes between casts. Post-fix diagnostic: `rate=1.00`, idle
  looping, positions advancing.
- **Showcase camera framed the distant dummy too**, pulling the view high/far. Fix: a single-caster
  replay focuses the tracking + front cameras tight on the caster (blaze showcase camera→caster distance
  872 uu, was ~2000); two-caster fights keep the ratified both-fighters framing.

### Per-archetype status
| archetype | delivery trigger | clips wired | reads correctly? |
|---|---|---|---|
| **THROW** | `Projectile`, window ≥ `kHitscanGap` | `Attack01Anim` (single) | wired + plays at natural speed. Offensive staff-point cast — **reads plausibly** (used by the frost-ember windowed casts). |
| **SLAM** | `GroundAoE` (`ZoneSpawned`) | leap-slam composite `JumpUpAttackAnim`→`JumpAirAttackAnim`→`JumpEndAnim` | wired + composite plays (confirmed via single-node diag). A leap onto a ground zone — **reads plausibly** as a ground-AoE slam; leap-apex framing still to be visually confirmed. |
| **CHANNEL** | `Self` (effect resolves on the caster) | `Attack02StartAnim` + N×`Attack02MaintainAnim` loop | wired + loops to fill the window. **Reads WRONG for shield/ward casts** — it is an *attack* channel pose. See misfit below. |
| **SNAP** | `Projectile` window < `kHitscanGap`, or unresolved | `Attack03StartAnim` (single) | wired + plays. Quick offensive flick — **reads plausibly**. |

Idle = `Idle01Anim` (looping, resumes between casts). Death = grey-out (no death montage yet).

### Warden misfit — PRECISE
Fixture `warden-vs-duelist-t2-seed1`. Caster 1 is `a-defensive-earth-warden`.
- **`stone-aegis`** (casts at t=0.0 and t=9.0): its resolving effect is **`ShieldGranted` on the caster
  itself** (+ `StatModified` self) ⇒ `ClassifyDeliveries` marks it **`Self`** ⇒ archetype **CHANNEL** ⇒
  it plays the **`Attack02Start` + `Attack02Maintain`** loop. So a **defensive stone aegis reads as an
  aggressive attack-channel** (staff thrust/hold) — the misfit. This is the clearest wrong read in the
  corpus.
- **Remap (evaluated, recommended):** route **shield/ward casts** (delivery `Self` whose resolving
  effect is `ShieldGranted`) to **`DefendStartAnim` + `DefendMaintainAnim`** — the wizard's defend clips,
  which read as a raised-guard/ward and fit a shield grant. Both clips exist on the shared skeleton
  (`Content/BattleWizardPBR/Animations/DefendStartAnim`, `DefendMaintainAnim`, plus `DefendHitAnim`).
  This needs the CHANNEL branch to distinguish shield-grant Self-casts (→ Defend) from heal/buff
  Self-casts (→ Attack02 channel, or a heal-specific clip later).
- **Secondary note (not the shield remap):** the duelist's **`wind-step`** (self `Displaced`/`StatModified`)
  also classifies `Self` ⇒ CHANNEL ⇒ `Attack02` loop. A hit-and-run dash reading as an attack-channel is
  also off, but `DefendMaintain` is *not* its fix — a dash/dodge/`Jump*` mapping would be, which is a
  separate step-C/D refinement out of scope for the shield remap.

### What remains for steps C & D (before any act-two integration)
1. Implement the shield/ward → Defend remap (split the CHANNEL branch by resolving-effect kind).
2. Reconsider self-mobility (`wind-step`) mapping (dash vs channel).
3. Step D socket-origin pass on the wizard rig (throws from `hand_r`, slam feet, channel chest/hands),
   visually confirmed; and visually confirm the SLAM leap-apex and THROW/SNAP reads.
4. Re-run the gate at the appropriate cadence; full G1 + screenshots at the step-E stop.

## Step C RESUMED — verb-first cast animation (implemented)

Cast intent is now discovered from the resolving effects (`ClassifyDeliveries` second pass → `FReplayEvent::CastVerb`,
priority **shield > heal > self-mobility**, else Generic) and **overrides** the delivery archetype in
`PlayCastArchetype` — because a targeted heal is `Projectile` delivery yet must read as a heal, a shield
as a guard. Generic verbs still use the delivery archetype (SLAM/CHANNEL/THROW/SNAP).

**Per-verb clip table** (candidates evaluated in-editor via anim thumbnails — `CaptureAssetImage`):

| verb | trigger (event stream) | clip | thumbnail read | verdict |
|---|---|---|---|---|
| **shield / ward** | `ShieldGranted` in cast group | `DefendStartAnim` + looped `DefendMaintainAnim` (window-sized composite) | braced stance, orb/guard raised | **wired** — fixes the warden `stone-aegis` misfit (was the aggressive Attack02 channel). |
| **self-heal** | `Healed`, target == caster | `PotionDrinkAnim` | hand raised to mouth (drinking) | **wired** — reads as self-heal. |
| **targeted-heal** | `Healed`, target ≠ caster | `InteractAnim` | two-handed forward bestow gesture | **wired** — non-aggressive, reads as an acceptable bestow (soft but not wrong). |
| **mobility** | `Displaced`, subject == caster (no shield/heal) | `BattleRunForwardAnim` | mid-stride forward lean | **wired** — fixes `wind-step` (was Attack02); the body already lerps to the displaced position, so a run pose reads as a dash/step. |
| generic Self buff | else (Self delivery) | `Attack02Start` + `Attack02Maintain` | attack wind-up | kept as the generic channel fallback. |

Heal fallback: softened `Attack02` only if a heal clip fails to load (never a wrong-but-loud read). No
verb needed a **Mixamo retarget flag** — every chosen clip read acceptably for its verb. Thumbnails saved
under `scratchpad` during eval; `Attack02Start` confirmed *aggressive* (correctly rejected for shield/heal).

Presentation-only (the REPLAY| log is untouched — `PlayCastArchetype` reads no state, emits nothing).
Structural change (new `EReplayCastVerb`, new `FReplayEvent` field) → full rebuild. **fast-G1 (frost 4×)
after this step: BYTE-IDENTICAL (116 lines) — verb-first change is log-safe.**

## Step D — socket-origin pass (done)

Audited every effect/cast FX emission origin so each anchors to a **body bone** (via `SocketWorld`/
`HeadWorld`) on the wizard rig, not a raw sim position + fixed Z. Already-correct (from the earlier D):
throws leave `hand_r` → target `spine_03`; self/channel burst `spine_03`; slam foot dust `foot_r` (decal
stays at the event-given zone centre, Law 2); heal `spine_03`; shield shell `spine_03`. **Fixed this pass**
(were raw `SimToWorld+Z`):

| effect | was | now |
|---|---|---|
| `DamageDealt` impact | raw +100 | target **`spine_03`** (chest hit) |
| `StatusApplied` dock burst | raw +150 | target **head** (`HeadWorld`) — where the sigil docks |
| `StatusRemoved` shatter | raw +150 | target **head** (`HeadWorld`) |
| `StatModified` aura | raw +100 | target **`spine_03`** |
| regen mote (Tick) | raw +100 | target **`spine_03`** (rises from the chest) |

All are body-only expression changes (no new members/signatures) ⇒ **Live-Coding-safe** (no editor-close
rebuild). Presentation-only; the REPLAY| log is untouched. fast-G1 (frost 4×) after Live Coding:
**BYTE-IDENTICAL.**

## Step E — full gate + archetype screenshots (DONE; STOP for audit)

On the C+D build (verb-first cast animation + socket-origin pass; C via full rebuild, D via Live Coding):

- **Full G1 — PASS.** frost-ember **1× and 4×** and warden **1× and 4×** all **BYTE-IDENTICAL** to
  `docs/references/` (116/116/96/96 lines, verbatim `cmp`).
- **G1b — PASS.** frost-ember juice-**OFF** byte-identical to the reference (116 lines).
- **Four archetype screenshots in the law camera + FrontInspection supplements** (8 total) in
  `docs/screenshots/act1/`, on the 61-corpus (old showcase-named shots removed as superseded):

| archetype | fixture | law shot | front shot |
|---|---|---|---|
| **THROW** | `frost-ember-seed1` (windowed projectile) | `throw-law.png` | `throw-front.png` |
| **SLAM** | `blaze-2e5c00bf` (fire groundAoE) | `slam-law.png` | `slam-front.png` |
| **CHANNEL** | `glimmer` (self modifyStat = generic channel) | `channel-law.png` | `channel-front.png` |
| **SNAP** | `bramble` (instant targetUnit damage) | `snap-law.png` | `snap-front.png` |

  Captured via editor-viewport screen-grab (`CopyFromScreen`) with the human foregrounding the editor;
  **MCP `CaptureViewport` re-confirmed to render PIE skeletal actors as a gizmo**, so it cannot be used
  for these. Each grab verified by eye as a genuine editor render (skinned wizards, correct camera, browser
  label; throw shows frost fire-burst + ember frost column/sigils/shield). Fixtures picked verb-first-aware
  (a groundAoE with a shield verb now routes to Defend, not SLAM). Law camera focuses the caster for
  single-caster showcases (blaze/glimmer/bramble) and frames both for the frost-ember fight (ratified).
  *Notes:* the slam/channel/snap poses land near-idle (brief cast anims vs a wall-clock capture offset —
  timing, not a defect); every frame carries an **editor-only** "multiple directional lights competing"
  overlay warning (arena `DirectionalLight` + spawned `ReplaySun`) — cosmetic, not in-render.

**STOP for audit.** Act one C→E complete on the wizard rig: verb-first cast animation (shield/heal/mobility
overrides), socket-origin pass, full G1 + G1b green, archetype screenshots filed. Next (after audit): the
act-two **B-completion pass** (silhouette + motion refinement + nature/arcane, step-A guard) → greyscale
twin ballot → integration.

## Audit answers + RELEASE-MARKER table (STOP — human fills the release seconds)

**(a) The hard-edged blue "water-like" plane** is the engine **`SM_SkySphere`** (scale 400, material
`M_SimpleSkyDome`) — the arena's sky backdrop, read as a hard blue expanse past the finite `ReplayFloor`
edge. It is **not water**; A0's "no water body" stands (A0 correctly named the sky sphere). No water actor
exists in the level.

**(b) The grey ball** appears even in the **projectile-less** SLAM (blaze) and CHANNEL (glimmer) stills, so
it is **not** the cast projectile — it is a **stray grey-checker placeholder near the arena origin** (worth
removing). The real projectiles ARE event-driven: they spawn at the caster's `hand_r` and lerp to the
**target's chest** (`ToW` from the event's caster/target), i.e. toward the opponent; the "away" read is the
head caught at spawn on the caster's off-side, not an inverted direction. (Both flagged for a quick verify.)

### Release markers (clip durations measured via `SequenceLength`; release seconds are my best guesses)
| verb-intent | archetype | clip asset | duration (s) | best-guess release/impact (s) |
|---|---|---|---|---|
| generic — windowed projectile | **THROW** | `Attack01Anim` | 1.000 | ~0.65 (staff-point forward) |
| generic — groundAoE | **SLAM** | `JumpUpAttackAnim`→`JumpAirAttackAnim`→`JumpEndAnim` (composite) | 2.500 (0.833×3) | ~1.90 (foot-plant, into `JumpEnd`) |
| generic — self | **CHANNEL** | `Attack02StartAnim` (+`Attack02MaintainAnim` loop) | 0.667 + 0.333/loop | ~0.50 (end of Start) |
| generic — instant/unresolved projectile | **SNAP** | `Attack03StartAnim` | 1.333 | ~0.15 (quick flick at start) |
| shield / ward | **WARD** | `DefendStartAnim` (+`DefendMaintainAnim` loop) | 0.333 + 0.333/loop | ~0.30 (guard raised, end of Start) |
| self-heal | **HEAL(self)** | `PotionDrinkAnim` | 2.667 | ~1.30 (vial to mouth) |
| targeted-heal | **HEAL(target)** | `InteractAnim` | 1.667 | ~1.00 (bestow gesture) |
| self-mobility | **DASH** | `BattleRunForwardAnim` | 0.800 | ~0.10 (push-off at start) |
| idle (base) | — | `Idle01Anim` | ~1.0 (loop) | n/a |

**How to scrub a clip:** in the Content Browser open `Content/BattleWizardPBR/Animations/<Clip>` (double-click)
to launch the Animation Sequence editor; drag the timeline scrubber to the frame where the release/impact
reads (staff points forward · foot plants · vial reaches the mouth · guard fully raised), read the **time in
seconds** off the timeline, and enter it in the "release" column — flag any clip that should be swapped.

**STOP.** Awaiting the human's corrected release seconds (and any clip swaps) before wiring per-clip
`ReleaseMarker` alignment (fit the marker to the effect event; events are never delayed).

## E re-check — cast-window contract adopted (CastResolved); B-mode retired

The sim-side cast-time resync landed a `CastResolved` event per cast, giving **real cast windows**. Renderer
adapted (commits `d69822b` references, `699eb96` code):

- **References regenerated** — derived data, never hand-edited: each line is the C# authority's canonical
  projection, echoed verbatim (`LoadReplay` loads every event; `EmitCanonical` echoes `Event.Canonical`).
  Regenerated mechanically as `[e.canonical for e in events]`: frost-ember 116→141, warden 96→106,
  shadow 129 (deltas = the new `castResolved` lines). LF, single trailing newline.
- **CastResolved ingested** — `ClassifyDeliveries` takes the cast-window end from `CastResolved` (window =
  `CastStarted`→`CastResolved`), the effect anchor. Delivery/element/zone still read from the first resolving
  effect. Effect VFX already land at the resolve because the sim emits effects at `CastResolved.t` (verified:
  barrow cast 0.0 → resolve 0.8, effects at 0.8).
- **B-mode retired entirely** — real windows make strict marker-alignment visible, so the showcase-scoped
  natural-speed + deferred-FX path is gone (`FDeferredFX`/queue/`BModeReleaseDelaySim`/`SpawnVerbFXNow`/Tick
  firing all removed). `PlayCastArchetype` fits the play-rate strictly everywhere; instants (~0 window)
  rate-stretch to the SNAP flick as designed.

**Full G1 — GREEN.** frost-ember + warden at 1× and 4×, byte-identical to the regenerated references (4/4).

**Corpus window census (61 showcases):** real windows on **43** spells — THROW 15, SLAM 8, HEAL 8 (of 10),
WARD 12 (of 14) — all ~0.8 s; **18** declared instants — SNAP 12, CHANNEL 1 (glimmer), DASH 1, +2 heal/+2 ward.
Consequence for the four generic archetype stills: **THROW + SLAM show real wind-ups**; **CHANNEL (glimmer,
the only CHANNEL cast) and SNAP are instant** (window 0 → ~12× flick, no wind-up by design).

### Marker re-captures — PENDING (editor must be foreground)
Reps + capture offset (= resolve time @1×): THROW `cinder-e43f6121` 0.80 · SLAM `murk-7ff0b332` 0.80 ·
WARD `scrying-pool-4ba61be1` 0.80 · HEAL `lighthouse-01d420e1` 0.80 · CHANNEL `glimmer` ~0.25 (instant) ·
SNAP `blaze-2e5c00bf` ~0.25 (instant). Six archetypes × (law + front) = 12 stills (human chose the full set).

**Blocked:** `capture.ps1` grabs the **foreground window**; a first run captured a non-editor foreground
(discarded on sight, nothing committed — privacy). MCP `CaptureViewport` can't substitute (PIE skeletal actor
renders as a gizmo proxy). The re-capture needs the **Unreal Editor viewport foregrounded** (ideally the PIE
viewport maximised). Held for that, then the body ballot.
