# Beauty pass — Act 2: SUBSTANCE (elements become their physical stuff)

**Authority:** `docs/visual-grammar.md` (LAW — two-level element rule, Laws 1–6, addendum
clauses 1–5) + THE ANIMATION/SOCKET work of act one (`docs/act1-plan.md`). Laws 4 & 5
(legibility, restraint) outrank spectacle everywhere. `REPLAY|` byte-identity gates every
integration step. **STYLE REGISTER = chibi-bold** (phase-one ballot).

## Doc-gap note (surfaced)
The brief references "addendum clauses 1–7 … and the style-reference line as modified by the
REGISTER slot," but `visual-grammar.md` currently ends at **clause 5 (TWIN TEST)** — there are no
clauses 6–7 and no explicit style-reference line. I proceed with the chibi-bold interpretation
defined below and record it here as the ratification sheet; if a canonical style line exists it
should be folded into the grammar.

## STYLE REGISTER — chibi-bold (interpretation)
Bold, saturated, high-contrast, **chunky** stylized substance with strong readable silhouettes —
matching the ratified chibi-proportioned wizard body. Punchy flipbook/stylized VFX over subtle or
photoreal looks. Per element: colours snap to the `MPC_Elements` palette primaries; the
characteristic motion is exaggerated (flame licks flare, water globs wobble, rocks tumble); and
silhouettes stay irregular-but-thick per **NO PERFECT PRIMITIVES** (noise/rotation/flicker, no clean
spheres/discs). Where a pack offers a "…Stylized" variant (Mega Magic), **prefer it** — it is already
the chibi-bold read. (Contrast: `genshin-elegant` would favour refined, flowing, delicate forms.)
Legibility (Law 4) and restraint (Law 5) cap the boldness: substance must stay distinguishable and
never occlude the arena or a status row.

## Harvest inventory (imported packs)
- **MegaMagicVFXBundle** — the workhorse. Element **blasts/explosions** (impact burst), **shields +
  walls** (shell surface / zone), **auras** (status), plus meteors/asteroids/rings. Per-element
  `N_*` Niagara systems, many with **Charged** and **Stylized** variants.
- **SlashTrail_SoftTofu** — per-element **trails** (`NS_SlashTrail_<elem>`), **hits**
  (`NS_Hit_<elem>`), and **auras** (`NS_AuraFX_<elem>`) for Fire, Ice, Water, Lightning, Wind,
  Nature, Sand, Dark, Mystic (+ stylistic Basic/CyberPunk/Matrix/Music/LightSaber/Scifi/Distortion).
- **Jayant_Chakradhari_Assets** — 35 **lowpoly pebble/rock** meshes + `MI_Pebble_*` materials
  (`Lowpoly_Pebbles/Mesh`). The earth substance body: hurled rocks, shards, rubble.
- **FreeNiagaraPack** — abstract sci-fi only (`NS_ActiveAtom`, `NS_Worm-Hole`, `NS_StarTrack_Medium`,
  `NS_GridFigure`, `NS_EyeColor`). **Low value** for element substance; usable only as *arcane*
  accents (geometric/synthetic). Not a primary source for any element.
- **Simple Noise Generators** — **skipped** (incompatible with this engine version, per human).
  Silhouette noise (NO PERFECT PRIMITIVES) will instead come from each pack's built-in noise modules
  + the element materials, not a dedicated noise pack.

## Per-element substance plan (sockets → raw sources → chibi-bold shaping)
Sockets from act one: **projectile head · impact burst · trail · shell surface · zone fill · heal motes**.

| element | projectile head | impact burst | trail | shell / wall | zone fill | heal motes | chibi-bold shaping |
|---|---|---|---|---|---|---|---|
| **fire** | flame ball (Meteor core / FlameBlast) | `N_FlameBlast` / `N_MagmaExplosion`, `NS_Hit_Fire` | `NS_SlashTrail_Fire` | `N_FlameShield`/`N_FlameWall`, `RingOfFlames` | `RingOfFlames` / FlameWall ground | rising embers/sparks | fat orange→deep-red licks, exaggerated flare, additive glow |
| **water** | coherent glob (Aqua material) | Tsunami splash, `NS_Hit_Water`/`NS_Hit_Ice` | `NS_SlashTrail_Water` | `N_AquaShield`/`N_AquaWall` | Aqua puddle / Tsunami | blue droplets rising | chunky cyan→deep-blue glob, thick foam rim, wobble |
| **earth** | **Jayant lowpoly rock** mesh | `N_EarthExplosion` + rock shards, `NS_Hit_Sand` | `NS_SlashTrail_Sand` + tumbling pebbles | Magma/rock ring (`N_MagmaWall`) | rubble (pebbles) + dust | rising dust/pebbles | flat-shaded chunky rocks, ochre→slate, bold dust puffs |
| **air** | wind swirl/ribbon (WindShield mat) | `NS_Hit_Wind`, WindWall gust; `N_LightningBlast` (shock) | `NS_SlashTrail_Wind` | `N_WindShield`/`N_WindWall` (prefer Stylized) | swirling wind decal | rising ribbons | pale-cyan→white bold streaks/ribbons, quick, translucent |
| **light** | light bolt (`N_LightBlast` core) | `N_LightBlast`(Charged), glints | `NS_SlashTrail_LightSaber` | `N_EnergyShield` (light-tinted, Stylized) | light pool | **golden rays/glints rising** (heal element) | warm-white→gold rays, clean bloom |
| **shadow** | dark orb (Curse/Blackhole core) | `N_BlackholeExplosion`/`N_ChaosExplosion`, `NS_Hit_Dark` | `NS_SlashTrail_Dark`, `TrailsOfDeath` | `N_CurseShield`/`N_CurseWall` (Stylized) | smoke pool | dark motes (rare heal) | violet→near-black soft tendrils/smoke, matte |
| **nature** *(waits for refresh)* | thorn/leaf orb (Poison green) | `N_PoisonExplosion`, `NS_Hit_Nature` | `NS_SlashTrail_Nature` | `N_PoisonShield`/`N_PoisonWall` | `N_NatureAura` ground | **green motes (regen)** | bold leaf-green→moss, chunky leaves/spores/thorns |
| **arcane** *(waits for refresh; neutral fallback)* | rune orb (Magical/Energy core) | `N_MagicalExplosion`/`N_EnergyBlast`, `NS_Hit_Mystic` | `NS_SlashTrail_Mystic` | `N_ArcaneShield`/`N_ArcaneWall`, `MagicalBarrier` | `GalacticField` | energy motes | magenta→violet crisp geometric runes/glyphs, synthetic |

## Coverage verdict
**Every element family has viable raw material — no STOP required.** The old-26 corpus uses only
{light, fire, earth, water, air, shadow} as concept elements, so those are the cells act two will
*exercise* now. **nature** and **arcane** substance sets can be *built* (material above), but stay
unexercised until the **61-showcase refresh** lands and introduces nature/arcane concept spells;
until then they are prepared and wired but validated only by the arcane-neutral fallback path.
Blood/Chaos/CyberPunk/Matrix/Music/Scifi/Distortion sources are **not** element families and are
excluded (gore is out per the art direction).

## Twin-risk pre-note (for Step E)
Watch for read-alike pairs once integrated: **fire vs earth-magma** (both orange), **water vs
air-lightning** if lightning skews blue, **water-glob vs water-ice**, **shadow vs arcane** (both
violet-dark), **nature-poison vs nature-leaf** (both green). Palette-primary separation + distinct
motion (lick vs tumble vs ribbon vs tendril) is the intended differentiator; final twin judgment at E.

## Build order
A (this doc) → B substance sets on a test map (per element/pair, MPC + User params, silhouette
noise) → C integration via the two-level element law from the act-one sockets + full G1 → D coverage
+ per-element screenshots → E report + twin-risk note.

## Step B — substance-set prototyping (IN PROGRESS)

**Test map:** `/Game/TestMaps/SubstanceLab` (blank level created by the human; populated + captured via
MCP). Contents: a `WizardSM` **scale reference** at origin (~175 uu), a floor cube, a directional light
+ sky light, and each element's substance set laid out in a row (one actor per M2-D socket). Captured
with MCP `CaptureViewport` (renders editor-placed content correctly — no live-screen grabs). **This map
is a prototype sandbox only; nothing here integrates into the arena (act-two step C is gated on act-one
C & D).**

**Tinting finding:** the **MegaMagic** systems ship **per-element** (`N_FlameShield`, `N_AquaShield`, …)
and expose **no color User param** — so each element uses its OWN native system rather than re-tinting a
generic one. The **SlashTrail** systems DO expose a `Color` User var (tintable from the MPC), but also
ship per-element. Net: the MPC palette is the *reference/source of truth* for each element's primary/
secondary (`<Element>_Primary`/`_Secondary`); systems are selected per-element and already carry the
right hue, with the `Color` User var set from the MPC where a generic system is reused.

### fire (committed) — screenshot `docs/screenshots/act2/fire-substance-lab.png`
| socket | system | reads |
|---|---|---|
| projectile head | `NS_AuraFX_Fire` | orange flame aura — reads as fire; a touch wispy for a "ball of fire" (refine: chunkier core, e.g. small `N_FlameBlast` loop or `SM_Meteor`+flame). |
| impact burst | `N_FlameBlast` | bold bright yellow-orange burst — strong chibi-bold read. |
| trail | `NS_SlashTrail_Fire` | motion-dependent (static capture understates it); `Color` User var present. |
| shell surface | `N_FlameShield` | translucent sphere wrapped in orange flame bands — reads as a fire shell. Sphere is near-primitive; flame bands supply the required silhouette irregularity (NO PERFECT PRIMITIVES). |
| zone fill | `N_RingOfFlames` | ground ring of flames. |
| heal motes | — | N/A (fire is not a heal element); ember-rise placeholder only. |

Scale vs the wizard reads acceptable; a slight scale-up would push boldness. Silhouette noise comes from
each system's native modules (flicker/turbulence), satisfying the no-perfect-primitives clause. **Next:
water, then earth, air, light, shadow — committing per element or pair.**
