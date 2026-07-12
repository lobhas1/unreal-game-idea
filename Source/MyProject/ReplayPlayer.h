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
