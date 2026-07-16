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
each system's native modules (flicker/turbulence), satisfying the no-perfect-primitives clause.

**Gallery layout (ratified):** persistent gallery — each element kept in its own row (Y offset 600 apart:
fire 0, water 600, earth 1200, air 1800, light 2400, shadow 3000) on the shared floor, so the final map
shows all six for the act-2-E twin-risk comparison. Per-element screenshots are the committed record.

### water (committed) — `docs/screenshots/act2/water-substance-lab.png`
| socket | system | reads |
|---|---|---|
| projectile head | `NS_AuraFX_Water` | cyan water aura (subtle static; a chunkier glob would push diegetic-substance). |
| impact burst | `NS_Hit_water` | splash burst. |
| trail | `NS_SlashTrail_Water` | motion-dependent; `Color` User var present. |
| shell surface | `N_AquaShield` | **strong read** — translucent cyan sphere with flowing water caustics on its surface (diegetic: water shaped into a shell); flame-free, wobble/caustic motion supplies silhouette irregularity. |
| zone fill | `N_Tsunami` | splashy water zone. |
| heal motes | — | rising blue droplets (placeholder; water not a heal element here). |

Native cyan hue matches `MPC.Water_Primary/_Secondary`. Refine: chunkier projectile-head glob.

**Capture note:** burst/one-shot systems (explosions, asteroids) finish before a static capture, so
`cap_elem` **reactivates** each element's components (re-assigns the system) immediately before
`CaptureViewport`. Looping systems (shells) render either way. The persistent gallery is a reference;
the per-element screenshots (captured with reactivation) are the record.

### earth (committed) — `docs/screenshots/act2/earth-substance-lab.png`
| socket | system | reads |
|---|---|---|
| projectile head | `N_MagicalAsteroid` | dark rock/asteroid orb (reads as a hurled solid; slightly purple — refine toward a browner Jayant lowpoly rock mesh for true diegetic earth). |
| impact burst | `N_EarthExplosion` | **strong** — chunky radial burst of brown rock debris; the definitive chibi-bold earth read. |
| trail | `NS_SlashTrail_Sand_` | sandy trail (motion-dependent). |
| shell surface | `N_MagmaShield` | dark rocky sphere veined with glowing lava cracks — reads as rock/magma shell (there is no `N_EarthShield`; magma is the closest earth-family shell). |
| zone fill | `N_MagmaWall` | magma/rock wall. |
| heal motes | — | rising dust/pebbles (placeholder). |

Chunky flat-shaded rock silhouettes satisfy NO PERFECT PRIMITIVES natively.

### air (committed) — `docs/screenshots/act2/air-substance-lab.png`
| socket | system | reads |
|---|---|---|
| projectile head | `NS_AuraFX_Wind` | pale wind swirl (faint). |
| impact burst | `NS_Hit_Wind` | wind gust. |
| trail | `NS_SlashTrail_Wind` | wind streak (motion-dependent). |
| shell surface | `N_WindShieldStylized` | pale translucent sphere with swirling wind streaks (Stylized variant preferred per chibi-bold) — reads as a wind shell. |
| zone fill | `N_WindWall` | wind gust wall. |
| heal motes | — | rising ribbons (placeholder). |

**Air is the faintest read** — translucent pale-cyan wind washes out against the white lab floor; it will
read markedly better on the dark arena floor, and/or with a bolder `MPC.Air` tint at integration. Motion
(the exaggerated swirl) is where air reads; static frames understate it.

### light (committed) — `docs/screenshots/act2/light-substance-lab.png`
| socket | system | reads |
|---|---|---|
| projectile head | `NS_AuraFX_Lightsaber` | light bolt aura. |
| impact burst | `N_LightBlast` | **strong + correct** — bold bright-white burst; the true light read. |
| trail | `NS_SlashTrail_LightSaber` | blue-white saber trail; has a `Color` User var (tintable to gold). |
| shell surface | `N_EnergyShieldStylized` | **hue MISMATCH** — renders **magenta** (Energy family's native tint), reads as arcane, not warm-white/gold light. |
| zone fill | `N_EnergyWallStylized` | same magenta mismatch. |
| heal motes | golden rays | light IS the heal element — golden rays/glints rising is the intended heal-mote; not yet placed. |

**FINDING (light hue gap):** MegaMagic shields/walls expose **no color User param**, so the Energy shell/
wall cannot be recoloured to gold via user params — only `N_LightBlast` (white) reads as light out of the
box. Fixes for integration/refinement: (a) author a light-tinted **material instance** of the Energy
shield/wall material (gold), or (b) source a white/gold shield elsewhere. The SlashTrail trail's `Color`
User var CAN be set to `MPC.Light` (gold). Logged as the first real per-element gap.

### shadow (committed) — `docs/screenshots/act2/shadow-substance-lab.png`
| socket | system | reads |
|---|---|---|
| projectile head | `NS_AuraFX_Dark` | dark smoke orb. |
| impact burst | `N_BlackholeExplosion` | dark implosion/burst. |
| trail | `NS_SlashTrail_Dark` | dark smoke trail; `Color` User var present. |
| shell surface | `N_CurseShieldStylized` | **strong** — dark violet smoky translucent sphere; matte tendrils (diegetic shadow). |
| zone fill | `N_CurseWall` | purple-veined smoke wall. |
| heal motes | — | dark motes (rare; placeholder). |

Reads as matte violet/near-black smoke. **Twin-risk observed:** shadow (violet) sits close to the light
shell's (mistaken) magenta — fixing light to warm-white/gold (above) also separates it from shadow/arcane.

## Step B — DONE (six exercised elements prototyped)

All six corpus-exercised elements are prototyped in the `/Game/TestMaps/SubstanceLab` gallery, one row each,
committed per element with a screenshot. Summary:

- **Strong reads:** fire (blast + flame shell), water (aqua shell), earth (rock-debris burst + magma
  shell), shadow (violet smoke shell). **Correct-but-partial:** light (white blast correct; shell magenta).
  **Faint:** air (translucent wind, washes out on the white floor).
- **Findings that shape integration (act-two step C):**
  1. MegaMagic ships **per-element** systems with **no color User param** → each element uses its own
     native system; the **MPC palette is the source of truth**, not a live tint, for those. Where a generic
     system is reused, the **SlashTrail `Color` User var** is the tint hook.
  2. **light hue gap** — Energy shell/wall render magenta; needs a gold **material instance** (or a
     white/gold shield asset) to read as light. Highest-priority refinement.
  3. **air** needs the dark arena floor and/or a bolder tint to read; it's motion-forward.
  4. **earth projectile head** → prefer a browner Jayant lowpoly rock over the purplish `MagicalAsteroid`.
  5. **burst systems** need reactivation to capture (baked into `cap_elem`); in the live arena the M2-D
     socket layer will trigger them on the event, so this is a capture-only concern.
- **NO PERFECT PRIMITIVES:** shells are spheres but each carries native surface motion (flame bands, water
  caustics, lava cracks, wind swirls, energy hex, violet smoke) supplying silhouette irregularity.
- **nature / arcane** substance sets remain **unexercised** (no corpus concept spells yet) — built only when
  the 61-showcase refresh lands, per the coverage verdict above.

**GATE (unchanged):** act-two **step C (integration into the live arena) does NOT start** — it requires act
one's steps **C and D finished first** (the shield/ward → Defend remap, self-mobility mapping, and the
socket-origin pass). We return to the animations before anything integrates. Animation law + fast-G1
cadence remain in force. Step B stands as a prototyped, documented sandbox.
