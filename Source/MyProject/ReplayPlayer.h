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

private:
	// --- loading ---
	bool LoadReplay();
	void ClassifyDeliveries();   // resolve each CastStarted's delivery from the event stream
	void BuildScaffold();
	FVector SimToWorld(const FVector& Sim) const;

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
	void SpawnZoneDecal(const FVector& CenterSim, double RadiusSim, float Opacity, int32 ZoneId);

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
};
