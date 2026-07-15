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

## Archetype → animation mapping (verb + delivery → montage)

| archetype | triggers (verb / delivery) | ASP anim | why / window |
|---|---|---|---|
| **THROW** | targeted damage, `projectile`/`targetUnit` | `Fire_Rifle_Hip` | forward arm projection from `hand_r`; play-rate = len / (effect.t − cast.t) so the "release" lands on the effect event; projectile head spawns at `hand_r`. |
| **SLAM** | `groundAoE` / `spawnZone` | `Jump_From_Stand` | the leap-and-land reads as a ground impact; timed so the landing coincides with `ZoneSpawned`; telegraph/dust at the feet. (Approximation — ASP has no melee slam.) |
| **CHANNEL** | `heal`, `shield`, self-buffs (`self` delivery, `modifyStat`) | `Reload_Rifle_Hip` | sustained, two-handed, self-focused; stretched across the cast→effect window; motes/shell emit from chest/hands. |
| **SNAP** | near-zero cast windows (effect.t − cast.t < ~kHitscanGap) | `Fire_Shotgun_Hip` | a single quick burst, play-rate pushed high to fit the near-instant window; from `hand_r`. |

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

## Results log
(filled at Step E)
