// Copyright enaDyne. Phase D milestone 1 renderer.

#include "ReplayPlayer.h"

#include "ReplayVisualGrammar.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/DecalComponent.h"
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

	// M2 verb archetypes (element-tinted via User.Color / scale from share). Loaded by
	// soft path; grammar maps event type -> archetype, no per-spell authoring.
	DamageFX     = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_Damage.NS_Damage"));
	HealFX       = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_Heal.NS_Heal"));
	ShieldFX     = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_Shield.NS_Shield"));
	StatusFX     = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_Status.NS_Status"));
	ModifyStatFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_ModifyStat.NS_ModifyStat"));
	DisplaceFX   = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_Displace.NS_Displace"));
	ZoneFX       = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_Zone.NS_Zone"));
	ProjectileFX = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_Projectile.NS_Projectile"));
	SelfBurstFX  = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/M2/Niagara/NS_SelfBurst.NS_SelfBurst"));
	ZoneDecalMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/M2/Materials/M_ZoneDecal.M_ZoneDecal"));

	// Shield shell mesh + material (element-tinted via a dynamic instance per grant).
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ShieldMat  = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/M2/Materials/M_ShieldShell.M_ShieldShell"));

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

	ClassifyDeliveries();
	return Events.Num() > 0;
}

void AReplayPlayer::ClassifyDeliveries()
{
	// For each CastStarted, scan the events up to the next CastStarted (its "cast
	// group") and classify delivery from the data (Law 2): a ZoneSpawned => GroundAoE;
	// else the first effect on another entity => Projectile, on the caster => Self.
	// This reads the event stream only; it invents nothing and drives no state.
	auto NumField = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K, int32 Def) -> int32
	{
		int32 V = Def; if (O.IsValid()) { O->TryGetNumberField(K, V); } return V;
	};

	for (int32 i = 0; i < Events.Num(); ++i)
	{
		if (Events[i].Type != TEXT("CastStarted")) { continue; }
		const int32 Caster = NumField(Events[i].Payload, TEXT("caster"), 0);
		Events[i].CasterId = Caster;

		for (int32 j = i + 1; j < Events.Num() && Events[j].Type != TEXT("CastStarted"); ++j)
		{
			const FString& T = Events[j].Type;
			const TSharedPtr<FJsonObject>& JP = Events[j].Payload;
			if (T == TEXT("ZoneSpawned"))
			{
				Events[i].Delivery = EReplayDelivery::GroundAoE;
				Events[i].EffectT = Events[j].T;
				if (JP.IsValid()) { JP->TryGetNumberField(TEXT("size"), Events[i].ZoneRadiusSim); }
				const TSharedPtr<FJsonObject>* C = nullptr;
				if (JP.IsValid() && JP->TryGetObjectField(TEXT("center"), C) && C)
				{
					Events[i].ZoneCenterSim = FVector((*C)->GetNumberField(TEXT("x")),
					                                   (*C)->GetNumberField(TEXT("y")),
					                                   (*C)->GetNumberField(TEXT("z")));
				}
				break;
			}
			if (T == TEXT("DamageDealt") || T == TEXT("StatusApplied") || T == TEXT("Healed") ||
			    T == TEXT("ShieldGranted") || T == TEXT("StatModified") || T == TEXT("Displaced"))
			{
				const int32 Tgt = (T == TEXT("Displaced"))
					? NumField(JP, TEXT("subject"), -1)
					: NumField(JP, TEXT("target"), -1);
				Events[i].EffectTargetId = Tgt;
				Events[i].EffectT = Events[j].T;
				if (JP.IsValid()) { JP->TryGetStringField(TEXT("element"), Events[i].EffectElement); }
				Events[i].Delivery = (Tgt == Caster) ? EReplayDelivery::Self : EReplayDelivery::Projectile;
				break;
			}
		}
	}
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
		PlayReplayEvent(Events[NextEventIndex]);
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

	// Advance travelling projectiles along their cast->effect path on the shared clock
	// (speed- and hit-stop-consistent, since it reads SimTime and never advances it).
	for (int32 i = Projectiles.Num() - 1; i >= 0; --i)
	{
		FActiveProjectile& PR = Projectiles[i];
		if (!PR.Comp.IsValid()) { Projectiles.RemoveAt(i); continue; }
		const double Span = PR.EndSim - PR.StartSim;
		const double A = (Span > SMALL_NUMBER) ? (SimTime - PR.StartSim) / Span : 1.0;
		if (A >= 1.0)
		{
			PR.Comp->DestroyComponent();
			Projectiles.RemoveAt(i);
		}
		else
		{
			PR.Comp->SetWorldLocation(FMath::Lerp(PR.FromW, PR.ToW, (float)FMath::Clamp(A, 0.0, 1.0)));
		}
	}

	// Despawn shield shells whose event-carried duration has elapsed on the shared
	// clock (respects speed and, later, hit-stop). A gentle pop marks the expiry.
	for (int32 i = Shells.Num() - 1; i >= 0; --i)
	{
		if (SimTime >= Shells[i].ExpireSim)
		{
			if (Shells[i].Mesh.IsValid())
			{
				SpawnVerbFX(ShieldFX, Shells[i].Mesh->GetActorLocation(), FString(), 1.0f);
				Shells[i].Mesh->Destroy();
			}
			Shells.RemoveAt(i);
		}
	}

	if (NextEventIndex >= Events.Num() && !bFinished)
	{
		FinishFight();
	}
}

void AReplayPlayer::PlayReplayEvent(const FReplayEvent& Event)
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

void AReplayPlayer::SpawnVerbFX(UNiagaraSystem* System, const FVector& Loc,
                                const FString& ClauseElement, float Scale)
{
	UWorld* World = GetWorld();
	if (!System || !World) { return; }
	if (UNiagaraComponent* Comp =
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Loc))
	{
		if (!ClauseElement.IsEmpty())
		{
			// Two-level element law: the clause (event) element tints. Concept and
			// highest-share are not carried by M1 fixture events, so pass empty and
			// let Law 3's fallbacks apply.
			const FElementPalette Pal = ReplayGrammar::Resolve(FString(), ClauseElement, FString());
			Comp->SetVariableLinearColor(TEXT("User.Color"), Pal.Primary);
		}
		Comp->SetVariableFloat(TEXT("User.Scale"), Scale);
		Comp->SetWorldScale3D(FVector(Scale));
	}
}

void AReplayPlayer::SpawnZoneDecal(const FVector& CenterSim, double RadiusSim, float Opacity, int32 ZoneId)
{
	UWorld* World = GetWorld();
	if (!World || !ZoneDecalMat) { return; }
	const float R = FMath::Max((float)RadiusSim * WorldScale, 60.f);
	// Project straight down onto the floor (pitch -90). DecalSize is a half-extent box.
	UDecalComponent* Dec = UGameplayStatics::SpawnDecalAtLocation(
		World, ZoneDecalMat, FVector(256.f, R, R), SimToWorld(CenterSim), FRotator(-90.f, 0.f, 0.f), 0.f);
	if (!Dec) { return; }
	FActiveDecal AD;
	AD.Decal = Dec;
	AD.MID = Dec->CreateDynamicMaterialInstance();
	if (AD.MID.IsValid()) { AD.MID->SetScalarParameterValue(TEXT("Opacity"), Opacity); }
	AD.CenterSim = CenterSim;
	AD.ZoneId = ZoneId;
	Decals.Add(AD);
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

		// M2 damage archetype: element-tinted impact burst at the target, scaled by
		// damage share of the victim's max HP.
		if (FReplayEntity* E = FindEntity(Target))
		{
			FString Elem; P->TryGetStringField(TEXT("element"), Elem);
			const double Frac = (E->MaxHp > 0.0) ? (Amt / E->MaxHp) : 0.0;
			const float S = FMath::Clamp(0.6f + 0.8f * (float)Frac, 0.4f, 3.0f);
			SpawnVerbFX(DamageFX, SimToWorld(E->CurrentSim) + FVector(0, 0, 100.f), Elem, S);
		}
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
		// Heal archetype: motes rise/converge on the target (default restorative tint;
		// no clause element carried by Healed events).
		if (FReplayEntity* E = FindEntity(Target))
		{
			SpawnVerbFX(HealFX, SimToWorld(E->CurrentSim) + FVector(0, 0, 100.f), FString(), 1.0f);
		}
	}
	else if (Event.Type == TEXT("StatusApplied"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		FString Status; P->TryGetStringField(TEXT("status"), Status);
		Float(HeadOf(Target) + FVector(0, 0, 20.f), Status, FColor::Cyan);
		// applyStatus archetype: sigil dock burst over the head (sigil glyph itself
		// is Step E; this is the rise/dock motion).
		if (FReplayEntity* E = FindEntity(Target))
		{
			SpawnVerbFX(StatusFX, SimToWorld(E->CurrentSim) + FVector(0, 0, 150.f), FString(), 1.0f);
		}
	}
	else if (Event.Type == TEXT("ShieldGranted"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		double Amt = 0; P->TryGetNumberField(TEXT("amount"), Amt);
		Float(HeadOf(Target), FString::Printf(TEXT("shield %d"), FMath::RoundToInt(Amt)), FColor(120, 180, 255));
		// shield archetype: a PERSISTENT translucent shell around the body, not a burst.
		// No shield-end event exists in the corpus, so the shell lives for the grant
		// event's own `duration` (Law 2 timings), tracked on SimTime and popped on expiry.
		double Dur = 0; P->TryGetNumberField(TEXT("duration"), Dur);
		if (FReplayEntity* E = FindEntity(Target))
		{
			if (SphereMesh && ShieldMat && World)
			{
				const FVector Loc = SimToWorld(E->CurrentSim) + FVector(0, 0, 100.f);
				if (AStaticMeshActor* Shell = World->SpawnActor<AStaticMeshActor>(Loc, FRotator::ZeroRotator))
				{
					Shell->SetMobility(EComponentMobility::Movable);
					UStaticMeshComponent* SMC = Shell->GetStaticMeshComponent();
					SMC->SetStaticMesh(SphereMesh);
					SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Shell->SetActorScale3D(FVector(1.8f));
					if (UMaterialInstanceDynamic* MID = SMC->CreateDynamicMaterialInstance(0, ShieldMat))
					{
						// Omni shields are neutral white; elemental tint arrives with the
						// concept element in G2. ShieldGranted carries no element (Law 3 inherit).
						MID->SetVectorParameterValue(TEXT("TintColor"), FLinearColor(0.6f, 0.8f, 1.0f));
					}
					if (E->Capsule.IsValid())
					{
						Shell->AttachToActor(E->Capsule.Get(), FAttachmentTransformRules::KeepWorldTransform);
					}
					FActiveShell AS; AS.Mesh = Shell; AS.ExpireSim = Event.T + Dur;
					Shells.Add(AS);
				}
			}
		}
	}
	else if (Event.Type == TEXT("StatModified"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		FString Kind; P->TryGetStringField(TEXT("statKind"), Kind);
		Float(HeadOf(Target) + FVector(0, 0, 20.f), Kind, FColor::Yellow);
		// modifyStat archetype: arrow stream + ring (direction glyph refined in Step E).
		if (FReplayEntity* E = FindEntity(Target))
		{
			SpawnVerbFX(ModifyStatFX, SimToWorld(E->CurrentSim) + FVector(0, 0, 100.f), FString(), 1.0f);
		}
	}
	else if (Event.Type == TEXT("CastStarted"))
	{
		int32 Caster = 0; P->TryGetNumberField(TEXT("caster"), Caster);
		FString Spell; P->TryGetStringField(TEXT("spell"), Spell);
		Float(HeadOf(Caster) + FVector(0, 0, 40.f), Spell, FColor::White);

		// Delivery trajectory (Step D), resolved from the event stream by
		// ClassifyDeliveries. Element tint comes from the resolving effect's element.
		if (Event.Delivery == EReplayDelivery::Projectile)
		{
			FReplayEntity* CE = FindEntity(Event.CasterId);
			FReplayEntity* TE = FindEntity(Event.EffectTargetId);
			if (CE && TE)
			{
				const FVector FromW = SimToWorld(CE->CurrentSim) + FVector(0, 0, 120.f);
				const FVector ToW   = SimToWorld(TE->CurrentSim) + FVector(0, 0, 120.f);
				const FElementPalette Pal = ReplayGrammar::Resolve(FString(), Event.EffectElement, FString());
				if (Event.EffectT - Event.T < kHitscanGap)
				{
					// gap ~ 0 -> hitscan: an instant element-tinted streak.
					DrawDebugLine(World, FromW, ToW, Pal.Primary.ToFColor(true), false, 0.15f, 0, 5.f);
				}
				else if (ProjectileFX)
				{
					// travelling projectile: head+trail lerped over [castT, effectT] in Tick.
					if (UNiagaraComponent* Comp =
						UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ProjectileFX, FromW))
					{
						Comp->SetVariableLinearColor(TEXT("User.Color"), Pal.Primary);
						FActiveProjectile PR; PR.Comp = Comp; PR.FromW = FromW; PR.ToW = ToW;
						PR.StartSim = Event.T; PR.EndSim = Event.EffectT;
						Projectiles.Add(PR);
					}
				}
			}
		}
		else if (Event.Delivery == EReplayDelivery::Self)
		{
			// self delivery: a radial burst at the caster (element tint if the self-effect
			// carries one; else the default). The verb effect renders separately.
			if (FReplayEntity* CE = FindEntity(Event.CasterId))
			{
				SpawnVerbFX(SelfBurstFX, SimToWorld(CE->CurrentSim) + FVector(0, 0, 100.f),
					Event.EffectElement, 1.0f);
			}
		}
		else if (Event.Delivery == EReplayDelivery::GroundAoE)
		{
			// groundAoE: faint telegraph decal at the future zone centre; the
			// ZoneSpawned event fills it (Law 2 - centre/radius from the zone event).
			SpawnZoneDecal(Event.ZoneCenterSim,
				Event.ZoneRadiusSim > 0.0 ? Event.ZoneRadiusSim : 2.0, 0.15f, -1);
		}
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
		// displace archetype, mode-aware (grammar v1.1):
		//  Push/Dash -> ribbon trail parented to the moving body (trails as it interpolates);
		//  Teleport  -> no trail: an implosion flash at the origin, an appearance flash at the destination.
		FString Mode; P->TryGetStringField(TEXT("mode"), Mode);
		if (FReplayEntity* E = FindEntity(Subject))
		{
			const FVector FromW = SimToWorld(E->CurrentSim) + FVector(0, 0, 50.f);
			const FVector ToW   = SimToWorld(E->TargetSim) + FVector(0, 0, 50.f);
			if (Mode == TEXT("Teleport"))
			{
				SpawnVerbFX(DisplaceFX, FromW, FString(), 0.8f); // implosion at origin
				SpawnVerbFX(DisplaceFX, ToW,   FString(), 1.0f); // appearance at destination
			}
			else if (DisplaceFX && World && E->Capsule.IsValid())
			{
				UNiagaraFunctionLibrary::SpawnSystemAttached(DisplaceFX, E->Capsule->GetRootComponent(),
					NAME_None, FVector(0, 0, 50.f), FRotator::ZeroRotator,
					EAttachLocation::KeepRelativeOffset, true);
			}
			else
			{
				SpawnVerbFX(DisplaceFX, FromW, FString(), 1.0f);
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
		// spawnZone archetype: burst at the zone center.
		SpawnVerbFX(ZoneFX, SimToWorld(Z.CenterSim) + FVector(0, 0, 20.f), FString(),
			FMath::Clamp((float)Z.RadiusSim, 1.0f, 4.0f));

		// Fill the groundAoE telegraph: adopt the pending decal at this centre and raise
		// it to full opacity + zone radius. If no telegraph exists (cast not classified
		// as GroundAoE), spawn the filled decal directly.
		bool bFilled = false;
		for (FActiveDecal& AD : Decals)
		{
			if (AD.ZoneId == -1 && FVector::DistSquared(AD.CenterSim, Z.CenterSim) < 1.0)
			{
				AD.ZoneId = Z.Id;
				if (AD.MID.IsValid()) { AD.MID->SetScalarParameterValue(TEXT("Opacity"), 0.5f); }
				if (AD.Decal.IsValid())
				{
					const float R = FMath::Max((float)Z.RadiusSim * WorldScale, 60.f);
					AD.Decal->DecalSize = FVector(256.f, R, R);
					AD.Decal->MarkRenderStateDirty();
				}
				bFilled = true;
				break;
			}
		}
		if (!bFilled) { SpawnZoneDecal(Z.CenterSim, Z.RadiusSim, 0.5f, Z.Id); }
	}
	else if (Event.Type == TEXT("ZoneExpired"))
	{
		int32 Id = 0; P->TryGetNumberField(TEXT("id"), Id);
		for (FActiveZone& Z : Zones)
		{
			if (Z.Id == Id) { Z.bActive = false; }
		}
		// Zone expired: remove its ground decal (grammar: expiry contracts and fades).
		for (int32 i = Decals.Num() - 1; i >= 0; --i)
		{
			if (Decals[i].ZoneId == Id)
			{
				if (Decals[i].Decal.IsValid()) { Decals[i].Decal->DestroyComponent(); }
				Decals.RemoveAt(i);
			}
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
