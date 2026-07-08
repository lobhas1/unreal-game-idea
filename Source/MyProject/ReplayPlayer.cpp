// Copyright enaDyne. Phase D milestone 1 renderer.

#include "ReplayPlayer.h"

#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Dedicated log category so the gate can grep "REPLAY|" cleanly.
DEFINE_LOG_CATEGORY_STATIC(LogReplay, Log, All);

namespace
{
	constexpr float kLabelSeconds = 1.2f;   // wall-clock lifetime of a floating number
	constexpr float kHeadOffsetZ  = 130.f;  // uu above capsule origin for labels
}

AReplayPlayer::AReplayPlayer()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AReplayPlayer::BeginPlay()
{
	Super::BeginPlay();

	CubeMesh     = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	bLoaded = LoadReplay();
	if (!bLoaded)
	{
		UE_LOG(LogReplay, Error, TEXT("ReplayPlayer: failed to load '%s' - nothing to render."), *ReplayPath);
		return;
	}

	UE_LOG(LogReplay, Display, TEXT("ReplayPlayer: loaded '%s' (%d events, %d entities, duration=%f, winner=%s, endReason=%s)"),
		*ReplayPath, Events.Num(), Entities.Num(), DurationSeconds, *WinnerName, *EndReason);

	BuildScaffold();
}

bool AReplayPlayer::LoadReplay()
{
	const FString FullPath = FPaths::Combine(FPaths::ProjectContentDir(), ReplayPath);

	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *FullPath))
	{
		UE_LOG(LogReplay, Error, TEXT("ReplayPlayer: cannot read file at %s"), *FullPath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogReplay, Error, TEXT("ReplayPlayer: JSON parse failed for %s"), *FullPath);
		return false;
	}

	const TSharedPtr<FJsonObject>* Header = nullptr;
	if (!Root->TryGetObjectField(TEXT("header"), Header) || !Header)
	{
		UE_LOG(LogReplay, Error, TEXT("ReplayPlayer: replay has no header"));
		return false;
	}

	(*Header)->TryGetStringField(TEXT("winner"), WinnerName);
	(*Header)->TryGetStringField(TEXT("endReason"), EndReason);
	(*Header)->TryGetNumberField(TEXT("durationSeconds"), DurationSeconds);

	// Entities
	const TArray<TSharedPtr<FJsonValue>>* EntityArr = nullptr;
	if ((*Header)->TryGetArrayField(TEXT("entities"), EntityArr) && EntityArr)
	{
		for (const TSharedPtr<FJsonValue>& V : *EntityArr)
		{
			const TSharedPtr<FJsonObject> Obj = V->AsObject();
			if (!Obj.IsValid()) { continue; }

			FReplayEntity E;
			Obj->TryGetNumberField(TEXT("id"), E.Id);
			Obj->TryGetStringField(TEXT("name"), E.Name);
			Obj->TryGetNumberField(TEXT("maxHp"), E.MaxHp);

			const TSharedPtr<FJsonObject>* Spawn = nullptr;
			if (Obj->TryGetObjectField(TEXT("spawn"), Spawn) && Spawn)
			{
				E.Spawn.X = (*Spawn)->GetNumberField(TEXT("x"));
				E.Spawn.Y = (*Spawn)->GetNumberField(TEXT("y"));
				E.Spawn.Z = (*Spawn)->GetNumberField(TEXT("z"));
			}
			E.CurrentSim = E.Spawn;
			E.TargetSim  = E.Spawn;
			Entities.Add(E);
		}
	}

	// Events (already ordered by t in file; we preserve file order verbatim).
	const TArray<TSharedPtr<FJsonValue>>* EventArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("events"), EventArr) || !EventArr)
	{
		UE_LOG(LogReplay, Error, TEXT("ReplayPlayer: replay has no events"));
		return false;
	}

	SimMin = FVector::ZeroVector;
	SimMax = FVector::ZeroVector;
	auto Grow = [this](const FVector& P)
	{
		SimMin = SimMin.ComponentMin(P);
		SimMax = SimMax.ComponentMax(P);
	};
	for (const FReplayEntity& E : Entities) { Grow(E.Spawn); }

	for (const TSharedPtr<FJsonValue>& V : *EventArr)
	{
		const TSharedPtr<FJsonObject> Obj = V->AsObject();
		if (!Obj.IsValid()) { continue; }

		FReplayEvent Ev;
		Obj->TryGetNumberField(TEXT("t"), Ev.T);
		Obj->TryGetStringField(TEXT("type"), Ev.Type);
		// The canonical projection line is authored by the C# authority and
		// stored per-event. We echo it verbatim; we never reconstruct it.
		Obj->TryGetStringField(TEXT("canonical"), Ev.Canonical);
		const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("payload"), PayloadPtr) && PayloadPtr)
		{
			Ev.Payload = *PayloadPtr;
		}

		// grow floor extents from any positional payload
		if (Ev.Payload.IsValid())
		{
			for (const TCHAR* Key : { TEXT("from"), TEXT("to"), TEXT("center") })
			{
				const TSharedPtr<FJsonObject>* Pos = nullptr;
				if (Ev.Payload->TryGetObjectField(Key, Pos) && Pos)
				{
					Grow(FVector((*Pos)->GetNumberField(TEXT("x")),
					             (*Pos)->GetNumberField(TEXT("y")),
					             (*Pos)->GetNumberField(TEXT("z"))));
				}
			}
			double ZoneSize = 0.0;
			if (Ev.Payload->TryGetNumberField(TEXT("size"), ZoneSize))
			{
				const TSharedPtr<FJsonObject>* C = nullptr;
				if (Ev.Payload->TryGetObjectField(TEXT("center"), C) && C)
				{
					const FVector Ctr((*C)->GetNumberField(TEXT("x")),
					                  (*C)->GetNumberField(TEXT("y")),
					                  (*C)->GetNumberField(TEXT("z")));
					Grow(Ctr + FVector(ZoneSize, ZoneSize, 0));
					Grow(Ctr - FVector(ZoneSize, ZoneSize, 0));
				}
			}
		}

		Events.Add(MoveTemp(Ev));
	}

	return Events.Num() > 0;
}

FVector AReplayPlayer::SimToWorld(const FVector& Sim) const
{
	return Sim * WorldScale;
}

void AReplayPlayer::BuildScaffold()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	// --- Floor sized to the sim's coordinate range (with margin) ---
	const FVector CenterSim = (SimMin + SimMax) * 0.5f;
	FVector SpanSim = (SimMax - SimMin);
	SpanSim.X = FMath::Max(SpanSim.X, 4.0);
	SpanSim.Y = FMath::Max(SpanSim.Y, 4.0);
	const float MarginUU = 200.f;

	if (CubeMesh)
	{
		FActorSpawnParameters P;
		AStaticMeshActor* F = World->SpawnActor<AStaticMeshActor>(
			SimToWorld(CenterSim) - FVector(0, 0, 10.f), FRotator::ZeroRotator, P);
		if (F)
		{
			F->SetMobility(EComponentMobility::Movable);
			F->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
			// Cube basic shape is 100uu per unit scale.
			const float SX = (SpanSim.X * WorldScale + 2 * MarginUU) / 100.f;
			const float SY = (SpanSim.Y * WorldScale + 2 * MarginUU) / 100.f;
			F->SetActorScale3D(FVector(FMath::Max(SX, 1.f), FMath::Max(SY, 1.f), 0.1f));
			F->SetActorLabel(TEXT("ReplayFloor"));
			Floor = F;
		}
	}

	// --- Two capsule actors (cylinder mesh pillars) with name labels ---
	for (FReplayEntity& E : Entities)
	{
		if (!CylinderMesh) { break; }
		FActorSpawnParameters P;
		const FVector Loc = SimToWorld(E.Spawn) + FVector(0, 0, 100.f);
		AStaticMeshActor* Cap = World->SpawnActor<AStaticMeshActor>(Loc, FRotator::ZeroRotator, P);
		if (!Cap) { continue; }
		Cap->SetMobility(EComponentMobility::Movable);
		Cap->GetStaticMeshComponent()->SetStaticMesh(CylinderMesh);
		Cap->SetActorScale3D(FVector(0.6f, 0.6f, 2.0f));
		Cap->SetActorLabel(*FString::Printf(TEXT("Capsule_%s"), *E.Name));
		E.Capsule = Cap;

		// Floating name label that follows the capsule (attached via base actor).
		DrawDebugString(World, FVector(0, 0, kHeadOffsetZ), E.Name, Cap, FColor::White, -1.f, true);
	}

	// --- Overhead camera ---
	{
		const FVector Above = SimToWorld(CenterSim) + FVector(0, 0, FMath::Max(SpanSim.X, SpanSim.Y) * WorldScale + 900.f);
		ACameraActor* Cam = World->SpawnActor<ACameraActor>(Above, FRotator(-90.f, 0.f, 0.f));
		if (Cam)
		{
			Cam->SetActorLabel(TEXT("ReplayOverheadCamera"));
			OverheadCamera = Cam;
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				PC->SetViewTarget(Cam);
			}
		}
	}

	// --- One directional light ---
	if (AActor* Light = World->SpawnActor<AActor>(ADirectionalLight::StaticClass(),
			FVector(0, 0, 1000.f), FRotator(-60.f, 30.f, 0.f)))
	{
		Light->SetActorLabel(TEXT("ReplaySun"));
	}
}

void AReplayPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLoaded) { return; }

	UWorld* World = GetWorld();

	// Advance the sim clock. SpeedMultiplier changes pacing ONLY; the event
	// firing loop below is index-ordered, so order and log content are invariant.
	SimTime += (double)DeltaSeconds * (double)FMath::Max(SpeedMultiplier, 0.f);

	while (NextEventIndex < Events.Num() && Events[NextEventIndex].T <= SimTime)
	{
		ProcessEvent(Events[NextEventIndex]);
		++NextEventIndex;
	}

	// Interpolate capsule positions toward displace targets.
	for (FReplayEntity& E : Entities)
	{
		if (!E.Capsule.IsValid()) { continue; }
		E.CurrentSim = FMath::VInterpTo(E.CurrentSim, E.TargetSim, DeltaSeconds, 8.f);
		FVector Loc = SimToWorld(E.CurrentSim) + FVector(0, 0, 100.f);
		E.Capsule->SetActorLocation(Loc);
	}

	// Redraw active zones as flat translucent cylinders (one frame each tick).
	if (World)
	{
		for (const FActiveZone& Z : Zones)
		{
			if (!Z.bActive) { continue; }
			const FVector C = SimToWorld(Z.CenterSim);
			const float R = Z.RadiusSim * WorldScale;
			DrawDebugCylinder(World, C, C + FVector(0, 0, 15.f), R, 32,
				FColor(30, 120, 255, 64), false, -1.f, 0, 2.f);
		}
	}

	if (NextEventIndex >= Events.Num() && !bFinished)
	{
		FinishFight();
	}
}

void AReplayPlayer::ProcessEvent(const FReplayEvent& Event)
{
	// THE LOG comes first and unconditionally: every consumed event emits its
	// canonical projection line. Visuals are secondary and never gate the log.
	EmitCanonical(Event);
	RenderEventVisual(Event);
}

void AReplayPlayer::EmitCanonical(const FReplayEvent& Event) const
{
	// Verbatim echo of the file's canonical field. No formatting, no math.
	UE_LOG(LogReplay, Display, TEXT("REPLAY| %s"), *Event.Canonical);
}

FReplayEntity* AReplayPlayer::FindEntity(int32 Id)
{
	for (FReplayEntity& E : Entities)
	{
		if (E.Id == Id) { return &E; }
	}
	return nullptr;
}

void AReplayPlayer::RenderEventVisual(const FReplayEvent& Event)
{
	UWorld* World = GetWorld();
	if (!World || !Event.Payload.IsValid()) { return; }

	const TSharedPtr<FJsonObject>& P = Event.Payload;
	auto HeadOf = [this](int32 Id) -> FVector
	{
		if (FReplayEntity* E = FindEntity(Id))
		{
			return SimToWorld(E->CurrentSim) + FVector(0, 0, 100.f + kHeadOffsetZ);
		}
		return FVector::ZeroVector;
	};
	auto Float = [World](const FVector& Loc, const FString& Text, const FColor& Color)
	{
		DrawDebugString(World, Loc, Text, nullptr, Color, kLabelSeconds, true);
	};

	if (Event.Type == TEXT("DamageDealt"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		double Amt = 0; P->TryGetNumberField(TEXT("amount"), Amt);
		bool bKilled = false; P->TryGetBoolField(TEXT("killed"), bKilled);
		Float(HeadOf(Target), FString::Printf(TEXT("-%d"), FMath::RoundToInt(Amt)), FColor::Red);
		if (bKilled)
		{
			if (FReplayEntity* E = FindEntity(Target))
			{
				E->bDead = true;
				if (E->Capsule.IsValid())
				{
					if (UMaterialInstanceDynamic* MID =
						E->Capsule->GetStaticMeshComponent()->CreateDynamicMaterialInstance(0))
					{
						MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.3f, 0.3f, 0.3f, 1.f));
						MID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.3f, 0.3f, 0.3f, 1.f));
					}
					E->Capsule->SetActorScale3D(FVector(0.6f, 0.6f, 0.5f)); // slump
				}
			}
		}
	}
	else if (Event.Type == TEXT("Healed"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		double Eff = 0; P->TryGetNumberField(TEXT("effective"), Eff);
		Float(HeadOf(Target), FString::Printf(TEXT("+%d"), FMath::RoundToInt(Eff)), FColor::Green);
	}
	else if (Event.Type == TEXT("StatusApplied"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		FString Status; P->TryGetStringField(TEXT("status"), Status);
		Float(HeadOf(Target) + FVector(0, 0, 20.f), Status, FColor::Cyan);
	}
	else if (Event.Type == TEXT("ShieldGranted"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		double Amt = 0; P->TryGetNumberField(TEXT("amount"), Amt);
		Float(HeadOf(Target), FString::Printf(TEXT("shield %d"), FMath::RoundToInt(Amt)), FColor(120, 180, 255));
	}
	else if (Event.Type == TEXT("StatModified"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		FString Kind; P->TryGetStringField(TEXT("statKind"), Kind);
		Float(HeadOf(Target) + FVector(0, 0, 20.f), Kind, FColor::Yellow);
	}
	else if (Event.Type == TEXT("CastStarted"))
	{
		int32 Caster = 0; P->TryGetNumberField(TEXT("caster"), Caster);
		FString Spell; P->TryGetStringField(TEXT("spell"), Spell);
		Float(HeadOf(Caster) + FVector(0, 0, 40.f), Spell, FColor::White);
	}
	else if (Event.Type == TEXT("CastFailed"))
	{
		int32 Caster = 0; P->TryGetNumberField(TEXT("caster"), Caster);
		FString Spell; P->TryGetStringField(TEXT("spell"), Spell);
		FString Reason; P->TryGetStringField(TEXT("reason"), Reason);
		Float(HeadOf(Caster) + FVector(0, 0, 40.f),
			FString::Printf(TEXT("%s (%s)"), *Spell, *Reason), FColor::Orange);
	}
	else if (Event.Type == TEXT("Displaced"))
	{
		int32 Subject = 0; P->TryGetNumberField(TEXT("subject"), Subject);
		const TSharedPtr<FJsonObject>* To = nullptr;
		if (P->TryGetObjectField(TEXT("to"), To) && To)
		{
			if (FReplayEntity* E = FindEntity(Subject))
			{
				E->TargetSim = FVector((*To)->GetNumberField(TEXT("x")),
				                       (*To)->GetNumberField(TEXT("y")),
				                       (*To)->GetNumberField(TEXT("z")));
			}
		}
	}
	else if (Event.Type == TEXT("ZoneSpawned"))
	{
		FActiveZone Z;
		P->TryGetNumberField(TEXT("id"), Z.Id);
		P->TryGetNumberField(TEXT("size"), Z.RadiusSim);
		const TSharedPtr<FJsonObject>* C = nullptr;
		if (P->TryGetObjectField(TEXT("center"), C) && C)
		{
			Z.CenterSim = FVector((*C)->GetNumberField(TEXT("x")),
			                      (*C)->GetNumberField(TEXT("y")),
			                      (*C)->GetNumberField(TEXT("z")));
		}
		Z.bActive = true;
		Zones.Add(Z);
	}
	else if (Event.Type == TEXT("ZoneExpired"))
	{
		int32 Id = 0; P->TryGetNumberField(TEXT("id"), Id);
		for (FActiveZone& Z : Zones)
		{
			if (Z.Id == Id) { Z.bActive = false; }
		}
	}
	// ZoneTicked, StatusRemoved: no dedicated visual (still logged canonically).
}

void AReplayPlayer::FinishFight()
{
	bFinished = true;
	UE_LOG(LogReplay, Display, TEXT("ReplayPlayer: fight end - winner=%s reason=%s"), *WinnerName, *EndReason);

	if (UWorld* World = GetWorld())
	{
		const FVector Center = SimToWorld((SimMin + SimMax) * 0.5f) + FVector(0, 0, 400.f);
		DrawDebugString(World, Center, FString::Printf(TEXT("WINNER: %s"), *WinnerName),
			nullptr, FColor::Yellow, 8.f, true, 2.f);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow,
			FString::Printf(TEXT("WINNER: %s (%s)"), *WinnerName, *EndReason));
	}
}
