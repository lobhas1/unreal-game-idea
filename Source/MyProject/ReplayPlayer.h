// Copyright enaDyne. Phase D milestone 1 renderer.
//
// AReplayPlayer plays back a recorded fight replay like a video player.
//
// THE LAW OF THIS PHASE: the C# simulation is the single authority on what
// happens in a fight. This actor implements NO combat logic - no damage math,
// no cooldowns, no re-simulation, not one formula. It reads recorded events and
// renders them. Every rendered event's canonical projection line is echoed
// verbatim to the Output Log, prefixed "REPLAY| ". That log is the renderer's
// testimony and the entire point of milestone 1.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dom/JsonObject.h"
#include "ReplayPlayer.generated.h"

class UStaticMesh;
class AStaticMeshActor;
class ACameraActor;
class UNiagaraSystem;
class UNiagaraComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;

/** Delivery trajectory of a cast, discovered from the event stream (Law 2), not
 *  from the manifest: an effect on another entity => Projectile; on the caster =>
 *  Self; a ZoneSpawned in the cast group => GroundAoE. */
enum class EReplayDelivery : uint8 { None, Projectile, Self, GroundAoE };

/** One recorded event. Non-reflected: holds the raw payload for rendering and,
 *  crucially, the canonical projection string that the replay file already
 *  provides. We NEVER re-serialize the payload into a projection line - we echo
 *  the file's canonical field, so the log cannot diverge from the C# authority. */
struct FReplayEvent
{
	double T = 0.0;
	FString Type;
	FString Canonical;
	TSharedPtr<FJsonObject> Payload;

	// Delivery resolution (populated by ClassifyDeliveries for CastStarted events only,
	// from the following events up to the next cast - purely descriptive, no combat logic).
	EReplayDelivery Delivery = EReplayDelivery::None;
	int32 CasterId = 0;
	int32 EffectTargetId = 0;
	double EffectT = 0.0;          // time of the resolving effect event (== T for hitscan)
	FString EffectElement;         // element carried by the resolving effect, if any
	FVector ZoneCenterSim = FVector::ZeroVector; // GroundAoE telegraph center (sim space)
	double ZoneRadiusSim = 0.0;
};

/** One combatant, resolved from the replay header. */
struct FReplayEntity
{
	int32 Id = 0;
	FString Name;
	double MaxHp = 0.0;
	FVector Spawn = FVector::ZeroVector; // sim coordinates (unscaled)
	TWeakObjectPtr<AStaticMeshActor> Capsule;
	FVector CurrentSim = FVector::ZeroVector; // current sim-space position
	FVector TargetSim = FVector::ZeroVector;  // displace target (sim space)
	bool bDead = false;
};

/** An active zone, drawn as a flat translucent cylinder until its expiry event. */
struct FActiveZone
{
	int32 Id = 0;
	FVector CenterSim = FVector::ZeroVector;
	double RadiusSim = 0.0;
	bool bActive = false;
};

/** A persistent shield shell. No shield-end event exists in the corpus, so the
 *  shell's lifetime derives from the grant event's own `duration` timing (Law 2
 *  timings clause), tracked on the shared SimTime clock - never a band. */
struct FActiveShell
{
	TWeakObjectPtr<AStaticMeshActor> Mesh;
	double ExpireSim = 0.0;
};

/** A travelling projectile: the renderer lerps the component from FromW to ToW over
 *  [StartSim, EndSim] on the shared clock (cast-event -> effect-event, Law 2). */
struct FActiveProjectile
{
	TWeakObjectPtr<UNiagaraComponent> Comp;
	FVector FromW = FVector::ZeroVector;
	FVector ToW = FVector::ZeroVector;
	double StartSim = 0.0;
	double EndSim = 0.0;
};

/** An active status sigil docked over an entity: persists from StatusApplied until
 *  StatusRemoved (which shatters it). Rendered as a tinted text glyph (Step E). */
struct FActiveStatus
{
	int32 EntityId = 0;
	FString Status;
	double NextMoteSim = 0.0; // regen: next SimTime to emit a rising mote
};

/** A groundAoE ground decal: spawned faint as a telegraph at the cast, filled to
 *  full opacity at the ZoneSpawned event, removed at ZoneExpired (Law 2 - the
 *  events drive it). ZoneId is -1 until the spawn event adopts the telegraph. */
struct FActiveDecal
{
	TWeakObjectPtr<UDecalComponent> Decal;
	TWeakObjectPtr<UMaterialInstanceDynamic> MID;
	FVector CenterSim = FVector::ZeroVector;
	int32 ZoneId = -1;
};

UCLASS()
class MYPROJECT_API AReplayPlayer : public AActor
{
	GENERATED_BODY()

public:
	AReplayPlayer();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Project-relative path to the replay JSON. Default: the fixture replay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
	FString ReplayPath = TEXT("Replays/frost-ember-seed1.json");

	/** Sim-time to real-time multiplier. Changes pacing ONLY - never event
	 *  order, never log content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
	float SpeedMultiplier = 1.0f;

	/** Unreal units per sim unit. Cosmetic scaling for the scene only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
	float WorldScale = 100.0f;

	/** Law 6 juice master switch. When false: no hit-stop, no shake, no crunch. G1b
	 *  requires juice on vs off to yield byte-identical REPLAY| logs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
	bool bJuiceEnabled = true;

	/** Camera law: the three-quarter angled tracking view is canonical. Set true to fall
	 *  back to the retired straight-down overhead camera (debug only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
	bool bDebugOverheadCamera = false;

	/** G3 apparatus. When false, ALL on-screen TEXT is hidden - entity name labels,
	 *  floating damage/heal numbers, cast + status word labels, the winner banner, and
	 *  the showcase-browser label - leaving only the visuals and the tinted status
	 *  sigil markers. Presentation-only: never touches the REPLAY| log (G1 holds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay")
	bool bTextVisible = true;

	/** Optional crunch impact sounds, one per tier (light/heavy/max). Null => silent
	 *  (concrete samples are a deferred asset; the tiering + trigger stand regardless). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Juice")
	class USoundBase* CrunchTierLight = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Juice")
	class USoundBase* CrunchTierHeavy = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay|Juice")
	class USoundBase* CrunchTierMax = nullptr;

private:
	// --- loading ---
	bool LoadReplay();
	void ClassifyDeliveries();   // resolve each CastStarted's delivery from the event stream
	void LoadManifestElements(); // showcase spell id -> concept element (+ ordered list), from the manifest
	void BuildScaffold();
	FVector SimToWorld(const FVector& Sim) const;

	// --- showcase browser (Step 3b): cycle the manifest's showcases live during PIE ---
	// Presentation-only: swaps which replay plays and restarts its clock; the C# sim is
	// still the sole authority and the REPLAY| echo is unchanged.
	void SetupBrowserInput();
	void OnPrevShowcase();
	void OnNextShowcase();
	void OnReplayCurrent();
	void LoadAndBuild(const FString& NewReplayPath); // teardown + (re)load + rebuild + reset clock
	void TeardownScene();                            // destroy per-replay actors/FX, clear state
	void DrawLabel(const FVector& Loc, const FString& Text, const FColor& Color, float Scale = 1.f); // gated by bTextVisible

	TArray<FString> ShowcaseOrder;   // ordered showcase ids from the manifest (browser order)
	int32 CurrentShowcaseIndex = -1; // index into ShowcaseOrder, or -1 when the current replay isn't a showcase
	FString CurrentDisplayName;      // shown on screen as the browser label
	bool bScaffoldInit = false;      // one-time spawn of persistent cameras + light

	// --- playback ---
	void PlayReplayEvent(const FReplayEvent& Event);
	void RenderEventVisual(const FReplayEvent& Event);
	void EmitCanonical(const FReplayEvent& Event) const;
	FReplayEntity* FindEntity(int32 Id);
	void FinishFight();

	// --- header ---
	FString WinnerName;
	FString EndReason;
	double DurationSeconds = 0.0;

	// --- data (non-reflected plain C++ containers) ---
	TArray<FReplayEntity> Entities;
	TArray<FReplayEvent> Events;
	TArray<FActiveZone> Zones;

	// --- scene ---
	TWeakObjectPtr<AStaticMeshActor> Floor;
	TWeakObjectPtr<ACameraActor> OverheadCamera;
	TWeakObjectPtr<ACameraActor> TrackingCamera; // three-quarter canonical view
	void UpdateTrackingCamera(float DeltaSeconds); // frames both fighters at ~55 deg pitch
	static constexpr float kCamPitch = -55.0f;
	static constexpr float kCamYaw = -55.0f;
	UStaticMesh* CubeMesh = nullptr;
	UStaticMesh* CylinderMesh = nullptr;

	// --- M2 verb archetype effects (grammar-driven, no per-spell authoring) ---
	// Loaded by soft path in BeginPlay. The renderer resolves element colors via
	// ReplayVisualGrammar and spawns the archetype; it never alters the event loop
	// or the canonical log, so G1 byte-identity holds with visuals live.
	UNiagaraSystem* DamageFX = nullptr;
	UNiagaraSystem* HealFX = nullptr;
	UNiagaraSystem* ShieldFX = nullptr;
	UNiagaraSystem* StatusFX = nullptr;
	UNiagaraSystem* ModifyStatFX = nullptr;
	UNiagaraSystem* DisplaceFX = nullptr;
	UNiagaraSystem* ZoneFX = nullptr;

	// Shield shell (persistent translucent sphere, element-tinted MID). Tracked and
	// despawned on the shared clock per the grant event's duration.
	UStaticMesh* SphereMesh = nullptr;
	UMaterialInterface* ShieldMat = nullptr;
	TArray<FActiveShell> Shells;

	// Delivery (Step D): projectile with swappable head+trail; travelling projectiles
	// are lerped each Tick, hitscan casts draw an instant streak.
	UNiagaraSystem* ProjectileFX = nullptr;
	UNiagaraSystem* SelfBurstFX = nullptr;   // self-delivery radial burst at the caster
	TArray<FActiveProjectile> Projectiles;
	static constexpr double kHitscanGap = 0.05; // gap below this => hitscan streak, not travel

	// groundAoE telegraph decals (M_ZoneDecal). Telegraph on cast -> fill on ZoneSpawned
	// -> remove on ZoneExpired.
	UMaterialInterface* ZoneDecalMat = nullptr;
	TArray<FActiveDecal> Decals;
	void SpawnZoneDecal(const FVector& CenterSim, double RadiusSim, float Opacity, int32 ZoneId,
	                    const FString& ClauseElement); // element tint via two-level law (Step 3b patch)

	// Status sigils (Step E): persistent docked text glyphs, one per active status,
	// added on StatusApplied and shattered on StatusRemoved.
	TArray<FActiveStatus> ActiveStatuses;
	static constexpr double kRegenMoteInterval = 0.6; // sim-seconds between regen motes

	// Concept-element tinting (Step H / G2): a showcase spell wears its manifest concept
	// palette. CurrentConceptElement is set from the manifest on each cast and fed as the
	// concept level of the two-level law; empty for M1 fights (no manifest entry).
	TMap<FString, FString> SpellElement; // lowercased spell id -> lowercased element
	FString CurrentConceptElement;

	// Spawn a verb archetype at Loc. When ClauseElement is non-empty the burst is
	// element-tinted via the two-level law (User.Color); otherwise the system's
	// default tint stands. Cosmetic only - never touches event state or the log.
	void SpawnVerbFX(UNiagaraSystem* System, const FVector& Loc,
	                 const FString& ClauseElement, float Scale);

	// derived world extents (sim space) for floor sizing
	FVector SimMin = FVector::ZeroVector;
	FVector SimMax = FVector::ZeroVector;

	// --- clock ---
	double SimTime = 0.0;
	int32 NextEventIndex = 0;
	bool bLoaded = false;
	bool bFinished = false;

	// --- Law 6 juice (modulates ONLY the shared clock + camera; never events/log) ---
	// Hit-stop pauses the SimTime advance for a wall-clock window; because the clock
	// only pauses (never skips/reorders), every event still fires in order => the
	// REPLAY| log is invariant to juice (G1b) and to speed (1x/4x).
	double HitStopRemaining = 0.0;   // wall-clock seconds left frozen
	float ShakeAmp = 0.0f;           // current screen-shake amplitude (decays)
	FVector CameraBaseLoc = FVector::ZeroVector;
	void TriggerJuice(double DamageFraction); // hit-stop + shake + crunch from one hit

	static constexpr double kHitStop25 = 0.040; // >=25% max HP -> 40 ms
	static constexpr double kHitStop50 = 0.090; // >=50% max HP -> 90 ms
	static constexpr float  kShakeMaxAmp = 45.0f; // uu, readable cap
	static constexpr float  kShakeDecay = 6.0f;   // per second
};
