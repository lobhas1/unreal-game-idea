// Copyright enaDyne. Phase D milestone 1 renderer.

#include "ReplayPlayer.h"

#include "ReplayVisualGrammar.h"
#include "ReplayStatusGlyphs.h"
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
#include "Components/DirectionalLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/DecalComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
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

	LoadManifestElements(); // concept-element palette per showcase spell + ordered browser list (G2/3b)

	LoadAndBuild(ReplayPath); // initial load (TeardownScene is a no-op on the first call)
	SetupBrowserInput();      // left/right cycle showcases, R replays current (PIE only)
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

void AReplayPlayer::LoadManifestElements()
{
	// Read the showcase manifest so a spell wears its concept-element palette (G2). Absent
	// for the M1 fights, which is fine - their casts then resolve with an empty concept.
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Replays/Showcases/manifest.json"));
	FString Raw;
	if (!FFileHelper::LoadFileToString(Raw, *Path)) { return; }
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { return; }
	const TArray<TSharedPtr<FJsonValue>>* Spells = nullptr;
	if (!Root->TryGetArrayField(TEXT("spells"), Spells) || !Spells) { return; }
	ShowcaseOrder.Empty();
	for (const TSharedPtr<FJsonValue>& V : *Spells)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		if (!O.IsValid()) { continue; }
		FString Id, Elem;
		O->TryGetStringField(TEXT("id"), Id);
		O->TryGetStringField(TEXT("element"), Elem);
		if (!Id.IsEmpty())
		{
			SpellElement.Add(Id.ToLower(), Elem.ToLower());
			ShowcaseOrder.Add(Id); // manifest order == browser cycle order (Step 3b)
		}
	}
}

void AReplayPlayer::LoadAndBuild(const FString& NewReplayPath)
{
	// Presentation-layer replay switch: tear down the current scene, load the new replay,
	// rebuild the scaffold, and reset the shared clock. The event loop and the canonical
	// REPLAY| echo are untouched, so G1 byte-identity holds for any replay we play.
	TeardownScene();

	ReplayPath = NewReplayPath;
	SimTime = 0.0;
	NextEventIndex = 0;
	bFinished = false;
	WinnerName.Empty();
	EndReason.Empty();
	CurrentConceptElement.Empty();

	bLoaded = LoadReplay();
	if (!bLoaded)
	{
		UE_LOG(LogReplay, Error, TEXT("ReplayPlayer: failed to load '%s' - nothing to render."), *ReplayPath);
		return;
	}
	UE_LOG(LogReplay, Display, TEXT("ReplayPlayer: loaded '%s' (%d events, %d entities, duration=%f, winner=%s, endReason=%s)"),
		*ReplayPath, Events.Num(), Entities.Num(), DurationSeconds, *WinnerName, *EndReason);

	BuildScaffold();

	// Browser label + index resolved from the path ("beacon.replay.json" -> "beacon").
	CurrentDisplayName = FPaths::GetBaseFilename(NewReplayPath);
	CurrentDisplayName.RemoveFromEnd(TEXT(".replay"));
	CurrentShowcaseIndex = ShowcaseOrder.IndexOfByPredicate([&NewReplayPath](const FString& Id)
	{
		return NewReplayPath.Contains(FString::Printf(TEXT("Showcases/%s.replay.json"), *Id));
	});
}

void AReplayPlayer::TeardownScene()
{
	// Destroy everything a single replay spawns; cameras + light persist (spawned once).
	for (FActiveProjectile& PR : Projectiles) { if (PR.Comp.IsValid()) { PR.Comp->DestroyComponent(); } }
	Projectiles.Empty();
	for (FActiveDecal& AD : Decals) { if (AD.Decal.IsValid()) { AD.Decal->DestroyComponent(); } }
	Decals.Empty();
	for (FActiveShell& AS : Shells) { if (AS.Mesh.IsValid()) { AS.Mesh->Destroy(); } }
	Shells.Empty();
	for (FReplayEntity& E : Entities) { if (E.Capsule.IsValid()) { E.Capsule->Destroy(); } }
	Entities.Empty();
	ActiveStatuses.Empty();
	Zones.Empty();
	Events.Empty();
	if (Floor.IsValid()) { Floor->Destroy(); Floor = nullptr; }
}

void AReplayPlayer::SetupBrowserInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) { return; }
	EnableInput(PC);
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Left,  IE_Pressed, this, &AReplayPlayer::OnPrevShowcase);
		InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AReplayPlayer::OnNextShowcase);
		InputComponent->BindKey(EKeys::R,     IE_Pressed, this, &AReplayPlayer::OnReplayCurrent);
	}
}

void AReplayPlayer::OnPrevShowcase()
{
	if (ShowcaseOrder.Num() == 0) { return; }
	CurrentShowcaseIndex = (CurrentShowcaseIndex <= 0) ? ShowcaseOrder.Num() - 1 : CurrentShowcaseIndex - 1;
	LoadAndBuild(FString::Printf(TEXT("Replays/Showcases/%s.replay.json"), *ShowcaseOrder[CurrentShowcaseIndex]));
}

void AReplayPlayer::OnNextShowcase()
{
	if (ShowcaseOrder.Num() == 0) { return; }
	CurrentShowcaseIndex = (CurrentShowcaseIndex + 1) % ShowcaseOrder.Num();
	LoadAndBuild(FString::Printf(TEXT("Replays/Showcases/%s.replay.json"), *ShowcaseOrder[CurrentShowcaseIndex]));
}

void AReplayPlayer::OnReplayCurrent()
{
	LoadAndBuild(ReplayPath); // reload whatever is current (M1 fight or a showcase)
}

void AReplayPlayer::DrawLabel(const FVector& Loc, const FString& Text, const FColor& Color, float Scale)
{
	// Single gate for every piece of on-screen text (G3 apparatus). Cosmetic only.
	if (!bTextVisible) { return; }
	if (UWorld* W = GetWorld())
	{
		DrawDebugString(W, Loc, Text, nullptr, Color, kLabelSeconds, true, Scale);
	}
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
		// Name labels are drawn per-tick in Tick (gated by bTextVisible), not as a
		// persistent attached string, so the G3 text toggle can hide them live.
	}

	// --- Cameras + light: spawned ONCE and kept across showcase reloads (the browser
	// only swaps floor + capsules; the tracking camera reframes each Tick). ---
	if (!bScaffoldInit)
	{
		const FVector Above = SimToWorld(CenterSim) + FVector(0, 0, FMath::Max(SpanSim.X, SpanSim.Y) * WorldScale + 900.f);
		if (ACameraActor* OCam = World->SpawnActor<ACameraActor>(Above, FRotator(-90.f, 0.f, 0.f)))
		{
			OCam->SetActorLabel(TEXT("ReplayOverheadCamera"));
			OverheadCamera = OCam;
		}
		if (ACameraActor* TCam = World->SpawnActor<ACameraActor>(Above, FRotator(kCamPitch, kCamYaw, 0.f)))
		{
			TCam->SetActorLabel(TEXT("ReplayTrackingCamera"));
			TrackingCamera = TCam;
		}
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			ACameraActor* View = bDebugOverheadCamera ? OverheadCamera.Get() : TrackingCamera.Get();
			if (View) { PC->SetViewTarget(View); }
			if (bDebugOverheadCamera) { CameraBaseLoc = Above; }
		}
		if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
				FVector(0, 0, 1000.f), FRotator(-60.f, 30.f, 0.f)))
		{
			Sun->SetActorLabel(TEXT("ReplaySun"));
			// Win the forward-shading directional-light selection uniquely so the editor's
			// "multiple directional lights competing" warning stays out of captures (3b patch).
			if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
			{
				LC->ForwardShadingPriority = 10;
				LC->MarkRenderStateDirty();
			}
		}
		bScaffoldInit = true;
	}

	UpdateTrackingCamera(0.f); // snap-frame to the (re)built capsules before first draw
}

void AReplayPlayer::UpdateTrackingCamera(float DeltaSeconds)
{
	if (!TrackingCamera.IsValid() || Entities.Num() == 0) { return; }

	// Frame both fighters: midpoint + spread of their current world positions.
	FVector Mid = FVector::ZeroVector;
	FVector Lo(TNumericLimits<float>::Max()), Hi(TNumericLimits<float>::Lowest());
	for (const FReplayEntity& E : Entities)
	{
		const FVector W = SimToWorld(E.CurrentSim) + FVector(0, 0, 100.f);
		Mid += W;
		Lo = Lo.ComponentMin(W);
		Hi = Hi.ComponentMax(W);
	}
	Mid /= (float)Entities.Num();

	// Distance so both fit with comfortable margin; pitched down ~55 deg, three-quarter yaw.
	const float Spread = FVector::Dist(Lo, Hi);
	const float R = FMath::Clamp(Spread * 1.6f + 700.f, 800.f, 5000.f);
	const FRotator Rot(kCamPitch, kCamYaw, 0.f);
	const FVector DesiredLoc = Mid - Rot.Vector() * R;

	// Snap on the first frame (DeltaSeconds<=0), then track smoothly with no cuts.
	// Interp from the stored base (not the actor location) so shake offsets never feed back.
	const FVector NewLoc = (DeltaSeconds <= 0.f)
		? DesiredLoc
		: FMath::VInterpTo(CameraBaseLoc, DesiredLoc, DeltaSeconds, 3.0f);
	TrackingCamera->SetActorLocationAndRotation(NewLoc, Rot);
	CameraBaseLoc = NewLoc; // screen shake offsets around this base
}

void AReplayPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLoaded) { return; }

	UWorld* World = GetWorld();

	// Law 6 hit-stop: while frozen, the shared clock does NOT advance - so no events fire
	// and bodies hold - for a WALL-CLOCK window. The clock only pauses (never skips or
	// reorders), so every event still fires in index order and the REPLAY| log is identical
	// with juice on or off (G1b) and at any speed. SpeedMultiplier still changes pacing only.
	const bool bFrozen = bJuiceEnabled && HitStopRemaining > 0.0;
	if (bFrozen)
	{
		HitStopRemaining -= (double)DeltaSeconds;
	}
	else
	{
		SimTime += (double)DeltaSeconds * (double)FMath::Max(SpeedMultiplier, 0.f);

		while (NextEventIndex < Events.Num() && Events[NextEventIndex].T <= SimTime)
		{
			PlayReplayEvent(Events[NextEventIndex]);
			++NextEventIndex;
		}

		// Interpolate capsule positions toward displace targets (held during hit-stop).
		for (FReplayEntity& E : Entities)
		{
			if (!E.Capsule.IsValid()) { continue; }
			E.CurrentSim = FMath::VInterpTo(E.CurrentSim, E.TargetSim, DeltaSeconds, 8.f);
			FVector Loc = SimToWorld(E.CurrentSim) + FVector(0, 0, 100.f);
			E.Capsule->SetActorLocation(Loc);
		}
	}

	// Camera law: the three-quarter tracking view is canonical; UpdateTrackingCamera
	// frames both fighters and refreshes CameraBaseLoc. Overhead is the debug fallback.
	if (!bDebugOverheadCamera)
	{
		UpdateTrackingCamera(DeltaSeconds);
	}
	ACameraActor* ActiveCam = bDebugOverheadCamera ? OverheadCamera.Get() : TrackingCamera.Get();

	// Screen shake: offset the active camera around its base (runs during hit-stop too so
	// the impact reads). Amplitude decays to rest; only damage sets it (via TriggerJuice).
	if (ActiveCam)
	{
		if (ShakeAmp > 0.05f)
		{
			const FVector Off(FMath::FRandRange(-ShakeAmp, ShakeAmp),
			                  FMath::FRandRange(-ShakeAmp, ShakeAmp),
			                  FMath::FRandRange(-ShakeAmp, ShakeAmp));
			ActiveCam->SetActorLocation(CameraBaseLoc + Off);
			ShakeAmp = FMath::FInterpTo(ShakeAmp, 0.f, DeltaSeconds, kShakeDecay);
		}
		else if (ShakeAmp > 0.f)
		{
			ShakeAmp = 0.f;
			ActiveCam->SetActorLocation(CameraBaseLoc);
		}
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

	// Status sigils: redraw each active status as a tinted text glyph docked over its
	// entity (per-tick redraw = persistent while active), stacked when several stack.
	// Regen emits green motes on a cosmetic cadence (no per-tick regen event exists).
	if (World)
	{
		TMap<int32, int32> StackByEntity;
		for (FActiveStatus& AS : ActiveStatuses)
		{
			FReplayEntity* E = FindEntity(AS.EntityId);
			if (!E) { continue; }
			int32& StackN = StackByEntity.FindOrAdd(AS.EntityId);
			const FStatusGlyph G = ReplayStatus::GlyphFor(AS.Status);
			const FVector Loc = SimToWorld(E->CurrentSim) +
				FVector(0, 0, 100.f + kHeadOffsetZ + 30.f + StackN * 22.f);
			// Visual sigil marker: a tinted docked orb, one per active status, stacked.
			// Always drawn so the status stays legible with text hidden (G3 apparatus);
			// the word glyph beside it is text and gated by bTextVisible.
			DrawDebugSphere(World, Loc, 14.f, 8, G.Tint.ToFColor(true), false, 0.f, 0, 2.f);
			if (bTextVisible)
			{
				DrawDebugString(World, Loc + FVector(0, 0, 12.f), G.Text, nullptr, G.Tint.ToFColor(true), 0.f, true);
			}
			++StackN;
			if (G.bRisingMotes && SimTime >= AS.NextMoteSim)
			{
				SpawnVerbFX(HealFX, SimToWorld(E->CurrentSim) + FVector(0, 0, 60.f), FString(), 0.5f);
				AS.NextMoteSim = SimTime + kRegenMoteInterval;
			}
		}
	}

	// Entity name labels + the showcase-browser label. All text: gated by bTextVisible (G3).
	if (World && bTextVisible)
	{
		for (const FReplayEntity& E : Entities)
		{
			DrawDebugString(World, SimToWorld(E.CurrentSim) + FVector(0, 0, 100.f + kHeadOffsetZ),
				E.Name, nullptr, FColor::White, 0.f, true);
		}
		DrawDebugString(World, SimToWorld((SimMin + SimMax) * 0.5f) + FVector(0, 0, 320.f),
			FString::Printf(TEXT("< %s >   (left/right browse, R replay)"), *CurrentDisplayName),
			nullptr, FColor(200, 230, 255), 0.f, true, 1.4f);
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
		// Two-level element law: clause element tints where present, else inherit the
		// current cast's concept element (from the manifest; empty for M1 fights).
		if (!ClauseElement.IsEmpty() || !CurrentConceptElement.IsEmpty())
		{
			const FElementPalette Pal = ReplayGrammar::Resolve(CurrentConceptElement, ClauseElement, FString());
			Comp->SetVariableLinearColor(TEXT("User.Color"), Pal.Primary);
		}
		Comp->SetVariableFloat(TEXT("User.Scale"), Scale);
		Comp->SetWorldScale3D(FVector(Scale));
	}
}

void AReplayPlayer::TriggerJuice(double DamageFraction)
{
	// Law 6: derived from the damage event's fraction of the victim's max HP. Modulates
	// only the shared clock (hit-stop) and the camera (shake) + a crunch cue - never the
	// event or the log. Gated by the master switch so G1b (on vs off) is byte-identical.
	if (!bJuiceEnabled) { return; }

	double Stop = 0.0;
	if (DamageFraction >= 0.50)      { Stop = kHitStop50; }
	else if (DamageFraction >= 0.25) { Stop = kHitStop25; }
	// Max so the biggest simultaneous hit dominates rather than summing.
	HitStopRemaining = FMath::Max(HitStopRemaining, Stop);

	// Screen shake: amplitude proportional to the fraction, capped at a readable max.
	ShakeAmp = FMath::Min(kShakeMaxAmp, ShakeAmp + (float)DamageFraction * kShakeMaxAmp);

	// Crunch: three tiers keyed to the same fractions (silent until samples are assigned).
	USoundBase* Crunch = (DamageFraction >= 0.50) ? CrunchTierMax
		: (DamageFraction >= 0.25) ? CrunchTierHeavy : CrunchTierLight;
	if (Crunch) { UGameplayStatics::PlaySound2D(this, Crunch); }
}

void AReplayPlayer::SpawnZoneDecal(const FVector& CenterSim, double RadiusSim, float Opacity, int32 ZoneId,
                                  const FString& ClauseElement)
{
	UWorld* World = GetWorld();
	if (!World || !ZoneDecalMat) { return; }
	const float R = FMath::Max((float)RadiusSim * WorldScale, 60.f);
	// Project straight down onto the floor (pitch -90). DecalSize is a half-extent box sized
	// from the event radius (Law 2); M_ZoneDecal masks it to a circle inscribed in that box.
	UDecalComponent* Dec = UGameplayStatics::SpawnDecalAtLocation(
		World, ZoneDecalMat, FVector(256.f, R, R), SimToWorld(CenterSim), FRotator(-90.f, 0.f, 0.f), 0.f);
	if (!Dec) { return; }
	FActiveDecal AD;
	AD.Decal = Dec;
	AD.MID = Dec->CreateDynamicMaterialInstance();
	if (AD.MID.IsValid())
	{
		AD.MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		// Two-level element law: clause element tints where present, else inherit the cast's
		// concept palette (empty concept => arcane-neutral for M1 fights, which carry no zones).
		const FElementPalette Pal = ReplayGrammar::Resolve(CurrentConceptElement, ClauseElement, FString());
		AD.MID->SetVectorParameterValue(TEXT("TintColor"), Pal.Primary);
	}
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
	auto Float = [this](const FVector& Loc, const FString& Text, const FColor& Color)
	{
		DrawLabel(Loc, Text, Color); // gated by bTextVisible (G3)
	};

	if (Event.Type == TEXT("DamageDealt"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		double Amt = 0; P->TryGetNumberField(TEXT("amount"), Amt);
		bool bKilled = false; P->TryGetBoolField(TEXT("killed"), bKilled);

		// M2 damage archetype: element-tinted impact burst at the target, scaled by damage
		// share of the victim's max HP, plus an element-tinted amount-scaled damage number.
		if (FReplayEntity* E = FindEntity(Target))
		{
			FString Elem; P->TryGetStringField(TEXT("element"), Elem);
			const double Frac = (E->MaxHp > 0.0) ? (Amt / E->MaxHp) : 0.0;
			const FElementPalette Pal = ReplayGrammar::Resolve(CurrentConceptElement, Elem, FString());
			// Floating number: element-tinted, scaled with the amount, camera-facing
			// (DrawDebugString always billboards to the view).
			const float NumScale = FMath::Clamp(1.0f + 2.0f * (float)Frac, 1.0f, 3.0f);
			DrawLabel(HeadOf(Target), FString::Printf(TEXT("-%d"), FMath::RoundToInt(Amt)),
				Pal.Primary.ToFColor(true), NumScale); // element-tinted, amount-scaled, gated

			const float S = FMath::Clamp(0.6f + 0.8f * (float)Frac, 0.4f, 3.0f);
			SpawnVerbFX(DamageFX, SimToWorld(E->CurrentSim) + FVector(0, 0, 100.f), Elem, S);
			// Law 6 juice: hit-stop / shake / crunch keyed to the damage fraction.
			TriggerJuice(Frac);
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
		// applyStatus archetype: sigil rises and docks over the head (persistent while
		// active; see Tick), plus a dock burst. Track it unless already docked.
		bool bAlready = false;
		for (const FActiveStatus& AS : ActiveStatuses)
		{
			if (AS.EntityId == Target && AS.Status == Status) { bAlready = true; break; }
		}
		if (!bAlready)
		{
			FActiveStatus AS; AS.EntityId = Target; AS.Status = Status; AS.NextMoteSim = SimTime;
			ActiveStatuses.Add(AS);
		}
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
		// Concept-element palette for this cast's clauses (empty if not a showcase spell).
		CurrentConceptElement = SpellElement.FindRef(Spell.ToLower());

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
				const FElementPalette Pal = ReplayGrammar::Resolve(CurrentConceptElement, Event.EffectElement, FString());
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
			// groundAoE: faint element-tinted telegraph decal at the future zone centre; the
			// ZoneSpawned event fills it (Law 2 - centre/radius from the zone event). Empty clause
			// element => inherits the cast's concept palette.
			SpawnZoneDecal(Event.ZoneCenterSim,
				Event.ZoneRadiusSim > 0.0 ? Event.ZoneRadiusSim : 2.0, 0.12f, -1, Event.EffectElement);
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
		FString ZoneElem; P->TryGetStringField(TEXT("element"), ZoneElem); // clause element, if any
		const TSharedPtr<FJsonObject>* C = nullptr;
		if (P->TryGetObjectField(TEXT("center"), C) && C)
		{
			Z.CenterSim = FVector((*C)->GetNumberField(TEXT("x")),
			                      (*C)->GetNumberField(TEXT("y")),
			                      (*C)->GetNumberField(TEXT("z")));
		}
		Z.bActive = true;
		Zones.Add(Z);
		// spawnZone archetype: element-tinted burst at the zone center.
		SpawnVerbFX(ZoneFX, SimToWorld(Z.CenterSim) + FVector(0, 0, 20.f), ZoneElem,
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
				if (AD.MID.IsValid())
				{
					AD.MID->SetScalarParameterValue(TEXT("Opacity"), 0.35f); // low-alpha fill
					const FElementPalette Pal = ReplayGrammar::Resolve(CurrentConceptElement, ZoneElem, FString());
					AD.MID->SetVectorParameterValue(TEXT("TintColor"), Pal.Primary);
				}
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
		if (!bFilled) { SpawnZoneDecal(Z.CenterSim, Z.RadiusSim, 0.35f, Z.Id, ZoneElem); }
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
	else if (Event.Type == TEXT("StatusRemoved"))
	{
		int32 Target = 0; P->TryGetNumberField(TEXT("target"), Target);
		FString Status; P->TryGetStringField(TEXT("status"), Status);
		// Removal shatters the sigil: undock it and pop a small burst at the head.
		for (int32 i = ActiveStatuses.Num() - 1; i >= 0; --i)
		{
			if (ActiveStatuses[i].EntityId == Target && ActiveStatuses[i].Status == Status)
			{
				ActiveStatuses.RemoveAt(i);
			}
		}
		if (FReplayEntity* E = FindEntity(Target))
		{
			SpawnVerbFX(StatusFX, SimToWorld(E->CurrentSim) + FVector(0, 0, 150.f), FString(), 0.6f);
		}
	}
	// ZoneTicked: no dedicated visual (still logged canonically).
}

void AReplayPlayer::FinishFight()
{
	bFinished = true;
	UE_LOG(LogReplay, Display, TEXT("ReplayPlayer: fight end - winner=%s reason=%s"), *WinnerName, *EndReason);

	// Winner banner is text: gated by bTextVisible (G3). The log line above is not.
	if (bTextVisible)
	{
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
}
