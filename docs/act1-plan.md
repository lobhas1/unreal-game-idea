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

| archetype | triggers (verb / delivery) | wizard anim | why / window |
|---|---|---|---|
| **THROW** | targeted damage, `projectile`/`targetUnit` | `Attack01Anim` | staff-point forward cast; play-rate = len / (effect.t − cast.t) so the release lands on the effect event; projectile head spawns at `hand_r`. |
| **SLAM** | `groundAoE` / `spawnZone` | `Attack04Anim` | grounded area attack; fit to cast→`ZoneSpawned`; foot dust at the feet, decal at the event centre (Law 2). |
| **CHANNEL** | `heal`, `shield`, self-buffs (`self` delivery) | `Attack02StartAnim` | the start of a maintained channel; stretched across the cast→effect window; emit from chest. |
| **SNAP** | near-zero cast windows (effect.t − cast.t < ~kHitscanGap) | `Attack03StartAnim` | a quick attack start, play-rate pushed high to fit the near-instant window; from `hand_r`. |

Idle stance = `Idle01Anim`. Death keeps the grey-out (a `DieAnim` montage is a later refinement).
Mapping is by clip name + role; refined after visual review at Step E. Start/Maintain montage
splits are collapsed to the Start clip for phase 1 (single-clip play-rate fit).

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

**STOP** — phase one complete on the wizard body; both fixtures gate green, four archetype
screenshots filed. Phase two (substance/beauty) Step B unlocks after this stop is audited.
