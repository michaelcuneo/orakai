// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusTerrainLodWorldActor.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"
#include "CubusCore/Generation/CubusWorldGenerationLoaderActor.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/UI/OrakaiWorldLoadingWidget.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Gameplay/WorldObjects/CubusSpawnStreamingPawn.h"
#include "InputMappingContext.h"
#include "Orakai.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace OrakaiTerrainInspector
{
constexpr uint64 ScreenMessageKey = 0x43554255534D4154ull;

bool ResolveSectionAndFace(UProceduralMeshComponent* ProceduralMesh, const FHitResult& Hit, const FProcMeshSection*& OutSection,
	int32& OutFaceIndex, int32& OutSectionIndex)
{
	OutSection = nullptr;
	OutFaceIndex = INDEX_NONE;
	OutSectionIndex = INDEX_NONE;
	if (!IsValid(ProceduralMesh) || Hit.FaceIndex < 0)
	{
		return false;
	}
	if (Hit.Item >= 0)
	{
		if (FProcMeshSection* Section = ProceduralMesh->GetProcMeshSection(Hit.Item))
		{
			const int32 TriangleCount = Section->ProcIndexBuffer.Num() / 3;
			if (Hit.FaceIndex < TriangleCount)
			{
				OutSection = Section;
				OutFaceIndex = Hit.FaceIndex;
				OutSectionIndex = Hit.Item;
				return true;
			}
		}
	}
	int32 RemainingFaceIndex = Hit.FaceIndex;
	const int32 SectionCount = ProceduralMesh->GetNumSections();
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		FProcMeshSection* Section = ProceduralMesh->GetProcMeshSection(SectionIndex);
		if (Section == nullptr)
		{
			continue;
		}
		const int32 TriangleCount = Section->ProcIndexBuffer.Num() / 3;
		if (RemainingFaceIndex < TriangleCount)
		{
			OutSection = Section;
			OutFaceIndex = RemainingFaceIndex;
			OutSectionIndex = SectionIndex;
			return true;
		}
		RemainingFaceIndex -= TriangleCount;
	}
	return false;
}

FString ResolveMaterialName(const int32 MaterialId)
{
	for (TObjectIterator<UCubusMaterialRegistry> It; It; ++It)
	{
		UCubusMaterialRegistry* Registry = *It;
		if (!IsValid(Registry) || Registry->HasAnyFlags(RF_ClassDefaultObject))
		{
			continue;
		}
		if (const FCubusMaterialDefinition* Definition = Registry->FindMaterialDefinition(MaterialId))
		{
			if (!Definition->DisplayName.IsEmpty())
			{
				return Definition->DisplayName.ToString();
			}
			if (!Definition->Name.IsNone())
			{
				return Definition->Name.ToString();
			}
		}
	}
	return TEXT("Unknown");
}
} // namespace OrakaiTerrainInspector

AOrakaiPlayerController::AOrakaiPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContext(
		TEXT("/Game/Cubus/Input/IMC_Default.IMC_Default"));
	if (DefaultContext.Succeeded())
	{
		DefaultMappingContexts.Add(DefaultContext.Object);
	}

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseLookContext(
		TEXT("/Game/Cubus/Input/IMC_MouseLook.IMC_MouseLook"));
	if (MouseLookContext.Succeeded())
	{
		MobileExcludedMappingContexts.Add(MouseLookContext.Object);
	}
}

void AOrakaiPlayerController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateWorldLoadingScreen(DeltaSeconds);
	if (!IsValid(WorldLoadingWidget) && bShowTerrainMaterialInspector && IsLocalPlayerController())
	{
		UpdateTerrainMaterialInspector();
	}
}

void AOrakaiPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InitializeWorldLoadingScreen();
	if (IsValid(WorldLoadingWidget))
	{
		EnterWorldLoadingInputMode();
		return;
	}

	if (IsLocalPlayerController() && IsValid(GetWorld()))
	{
		for (TActorIterator<ACubusWorldGenerationLoaderActor> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			if (!IsValid(*Iterator))
			{
				continue;
			}

			FInputModeGameAndUI GenerationInputMode;
			GenerationInputMode.SetHideCursorDuringCapture(false);
			GenerationInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(GenerationInputMode);
			SetShowMouseCursor(true);
			bEnableClickEvents = true;
			bEnableMouseOverEvents = true;
			return;
		}
	}

	EnterGameplayInputMode();
	CreateMobileControlsIfNeeded();
}

void AOrakaiPlayerController::InitializeWorldLoadingScreen()
{
	if (!IsLocalPlayerController() || !IsValid(GetWorld()))
	{
		return;
	}

	for (TActorIterator<ACubusWorldGenerationLoaderActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (IsValid(*Iterator))
		{
			return;
		}
	}

	for (TActorIterator<ACubusBlockWorldActor> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		LoadingBlockWorld = *Iterator;
		break;
	}
	const bool bGeneratedWorld = FCubusGeneratedTerrainRuntime::IsActive();
	if (!IsValid(LoadingBlockWorld) || (!bGeneratedWorld && LoadingBlockWorld->IsWorldLoadingComplete()))
	{
		return;
	}
	WorldLoadingWidget = CreateWidget<UOrakaiWorldLoadingWidget>(this, UOrakaiWorldLoadingWidget::StaticClass());
	if (!IsValid(WorldLoadingWidget))
	{
		return;
	}
	WorldLoadingWidget->AddToPlayerScreen(1000);
	WorldLoadingWidget->SetLoadingState(0.0f, FText::FromString(TEXT("Choose a spawn location")));
	WorldLoadingWidget->SetSpawnSelectionAvailable(bGeneratedWorld);
	WorldLoadingWidget->SetSpawnReady(false);
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void AOrakaiPlayerController::UpdateWorldLoadingScreen(const float DeltaSeconds)
{
	if (!IsValid(WorldLoadingWidget) || !IsValid(LoadingBlockWorld))
	{
		return;
	}

	const bool bGeneratedWorld = FCubusGeneratedTerrainRuntime::IsActive();
	if (bGeneratedWorld)
	{
		WorldLoadingWidget->SetSpawnSelectionAvailable(true);

		ACubusTerrainLodWorldActor* LodWorld = nullptr;
		for (TActorIterator<ACubusTerrainLodWorldActor> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator))
			{
				LodWorld = *Iterator;
				break;
			}
		}

		FVector2D ProposedWorldMeters;
		float ProposedHeightMeters = 0.0f;
		const bool bHasProposal =
			FCubusGeneratedTerrainRuntime::GetProposedSpawnWorldMeters(ProposedWorldMeters) &&
			FCubusGeneratedTerrainRuntime::TrySampleHeightMeters(ProposedWorldMeters, ProposedHeightMeters);

		bool bDensityReady = false;
		bool bLodReady = false;
		if (bHasProposal)
		{
			const FVector ProposedFocusLocation(
				ProposedWorldMeters.X * 100.0,
				ProposedWorldMeters.Y * 100.0,
				static_cast<double>(ProposedHeightMeters) * 100.0 + 500.0);
			bDensityReady = LoadingBlockWorld->IsWorldLoadingComplete() &&
				LoadingBlockWorld->IsDensityStreamingCoverageReadyAtWorldLocation(ProposedFocusLocation);
			bLodReady = IsValid(LodWorld) && LodWorld->IsPreSpawnVisualCoverageReady();
		}

		const bool bSelectedAreaReady = bHasProposal && bDensityReady && bLodReady;
		WorldLoadingWidget->SetSpawnReady(bSelectedAreaReady);

		if (FCubusGeneratedTerrainRuntime::HasConfirmedSpawn())
		{
			APawn* CurrentPawn = GetPawn();
			if (!IsValid(CurrentPawn) || CurrentPawn->IsA<ACubusSpawnStreamingPawn>())
			{
				WorldLoadingWidget->SetSpawnPromotionActive(true);
				WorldLoadingWidget->SetLoadingState(
					1.0f,
					FText::FromString(TEXT("Spawning into selected terrain")));
				return;
			}

			// The GameMode has successfully replaced the hidden streaming pawn with
			// the real OrakaiCharacter. Fade the spawn page away now.
			WorldLoadingWidget->SetSpawnPromotionActive(true);
			constexpr float GeneratedFadeDuration = 0.35f;
			LoadingFadeElapsedSeconds += DeltaSeconds;
			WorldLoadingWidget->SetRenderOpacity(1.0f - FMath::Clamp(LoadingFadeElapsedSeconds / GeneratedFadeDuration, 0.0f, 1.0f));
			if (LoadingFadeElapsedSeconds < GeneratedFadeDuration)
			{
				return;
			}

			WorldLoadingWidget->RemoveFromParent();
			WorldLoadingWidget = nullptr;
			ResetIgnoreMoveInput();
			ResetIgnoreLookInput();
			EnterGameplayInputMode();
			CreateMobileControlsIfNeeded();
			return;
		}

		WorldLoadingWidget->SetSpawnPromotionActive(false);

		if (!bHasProposal)
		{
			WorldLoadingWidget->SetLoadingState(
				0.0f,
				FText::FromString(TEXT("Choose a spawn location")));
			return;
		}

		if (!LoadingBlockWorld->IsInitialSpawnAreaReady())
		{
			WorldLoadingWidget->SetLoadingState(
				FMath::Min(0.15f, LoadingBlockWorld->GetWorldLoadingProgress()),
				FText::FromString(TEXT("Preparing support terrain at selected spawn")));
			return;
		}

		if (!bDensityReady)
		{
			const float ChunkProgress = LoadingBlockWorld->IsWorldLoadingComplete()
				? 0.75f
				: LoadingBlockWorld->GetWorldLoadingProgress();
			WorldLoadingWidget->SetLoadingState(
				FMath::Clamp(ChunkProgress, 0.15f, 0.85f),
				FText::FromString(TEXT("Building gameplay chunks for selected spawn")));
			return;
		}

		if (!bLodReady)
		{
			const float LodProgress = IsValid(LodWorld)
				? LodWorld->GetPreSpawnVisualCoverageProgress()
				: 0.0f;
			WorldLoadingWidget->SetLoadingState(
				FMath::Clamp(0.85f + LodProgress * 0.15f, 0.85f, 0.99f),
				FText::FromString(TEXT("Building terrain LOD1-LOD6 for selected spawn")));
			return;
		}

		WorldLoadingWidget->SetLoadingState(
			1.0f,
			FText::FromString(TEXT("Selected spawn area ready")));
		return;
	}

	WorldLoadingWidget->SetLoadingState(LoadingBlockWorld->GetWorldLoadingProgress(), LoadingBlockWorld->GetWorldLoadingStatus());
	if (!LoadingBlockWorld->IsWorldLoadingComplete())
	{
		return;
	}

	constexpr float FadeDuration = 0.35f;
	LoadingFadeElapsedSeconds += DeltaSeconds;
	WorldLoadingWidget->SetRenderOpacity(1.0f - FMath::Clamp(LoadingFadeElapsedSeconds / FadeDuration, 0.0f, 1.0f));
	if (LoadingFadeElapsedSeconds < FadeDuration)
	{
		return;
	}

	WorldLoadingWidget->RemoveFromParent();
	WorldLoadingWidget = nullptr;
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	EnterGameplayInputMode();
	CreateMobileControlsIfNeeded();
}

void AOrakaiPlayerController::EnterWorldLoadingInputMode()
{
	if (!IsLocalPlayerController() || !IsValid(WorldLoadingWidget))
	{
		return;
	}

	FInputModeUIOnly LoadingInputMode;
	LoadingInputMode.SetWidgetToFocus(WorldLoadingWidget->TakeWidget());
	LoadingInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(LoadingInputMode);
	SetShowMouseCursor(true);
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void AOrakaiPlayerController::EnterGameplayInputMode()
{
	if (!IsLocalPlayerController())
	{
		return;
	}
	FInputModeGameOnly GameInputMode;
	SetInputMode(GameInputMode);
	SetShowMouseCursor(false);
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AOrakaiPlayerController::CreateMobileControlsIfNeeded()
{
	if (!IsLocalPlayerController() || IsValid(MobileControlsWidget) || !ShouldUseTouchControls())
	{
		return;
	}
	MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
	if (MobileControlsWidget)
	{
		MobileControlsWidget->AddToPlayerScreen(0);
	}
	else
	{
		UE_LOG(LogOrakai, Error, TEXT("Could not spawn mobile controls widget."));
	}
}

void AOrakaiPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AOrakaiPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AOrakaiPlayerController::UpdateTerrainMaterialInspector()
{
	if (!IsValid(GetWorld()) || GEngine == nullptr)
	{
		return;
	}
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TerrainMaterialTraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OrakaiTerrainMaterialInspector), true, GetPawn());
	QueryParams.bReturnFaceIndex = true;
	QueryParams.bTraceComplex = true;

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, QueryParams);
	if (!bHit)
	{
		GEngine->AddOnScreenDebugMessage(OrakaiTerrainInspector::ScreenMessageKey, 0.0f, FColor::Silver, TEXT("Terrain material: no hit"));
		return;
	}

	const int32 MaterialId = ResolveRenderedTerrainMaterialId(Hit);
	const FString MaterialName = MaterialId > 0 ? OrakaiTerrainInspector::ResolveMaterialName(MaterialId) : TEXT("Not a Cubus density triangle");
	const FString Message = MaterialId > 0
		? FString::Printf(TEXT("Terrain material: %s [ID %d]"), *MaterialName, MaterialId)
		: FString::Printf(TEXT("Terrain material: %s | Actor: %s"), *MaterialName, *GetNameSafe(Hit.GetActor()));
	GEngine->AddOnScreenDebugMessage(
		OrakaiTerrainInspector::ScreenMessageKey, 0.0f,
		MaterialId > 0 ? FColor::Yellow : FColor::Silver, Message);
	DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 12.0f, FColor::Yellow, false, 0.0f);
}

int32 AOrakaiPlayerController::ResolveRenderedTerrainMaterialId(const FHitResult& Hit) const
{
	UProceduralMeshComponent* ProceduralMesh = Cast<UProceduralMeshComponent>(Hit.GetComponent());
	const FProcMeshSection* Section = nullptr;
	int32 FaceIndex = INDEX_NONE;
	int32 SectionIndex = INDEX_NONE;
	if (!OrakaiTerrainInspector::ResolveSectionAndFace(ProceduralMesh, Hit, Section, FaceIndex, SectionIndex))
	{
		return INDEX_NONE;
	}
	const int32 FirstIndex = FaceIndex * 3;
	if (FirstIndex < 0 || FirstIndex + 2 >= Section->ProcIndexBuffer.Num())
	{
		return INDEX_NONE;
	}
	const uint32 VertexIndices[3] = {
		Section->ProcIndexBuffer[FirstIndex],
		Section->ProcIndexBuffer[FirstIndex + 1],
		Section->ProcIndexBuffer[FirstIndex + 2]
	};
	for (const uint32 VertexIndex : VertexIndices)
	{
		if (VertexIndex >= static_cast<uint32>(Section->ProcVertexBuffer.Num()))
		{
			return INDEX_NONE;
		}
	}
	const FProcMeshVertex& Vertex0 = Section->ProcVertexBuffer[VertexIndices[0]];
	const FProcMeshVertex& Vertex1 = Section->ProcVertexBuffer[VertexIndices[1]];
	const FProcMeshVertex& Vertex2 = Section->ProcVertexBuffer[VertexIndices[2]];
	const int32 PackingBase = FCubusDensityMesher::MaterialIdPackingBase;
	const int32 Packed01 = FMath::RoundToInt(Vertex0.UV0.X);
	const int32 Packed23 = FMath::RoundToInt(Vertex0.UV0.Y);
	const int32 MaterialIds[4] = {
		Packed01 % PackingBase,
		Packed01 / PackingBase,
		Packed23 % PackingBase,
		Packed23 / PackingBase
	};
	const FLinearColor AverageWeights = (FLinearColor(Vertex0.Color) + FLinearColor(Vertex1.Color) + FLinearColor(Vertex2.Color)) / 3.0f;
	const float Weights[4] = {AverageWeights.R, AverageWeights.G, AverageWeights.B, AverageWeights.A};
	int32 DominantSlot = 0;
	for (int32 Slot = 1; Slot < 4; ++Slot)
	{
		if (Weights[Slot] > Weights[DominantSlot])
		{
			DominantSlot = Slot;
		}
	}
	return MaterialIds[DominantSlot] > 0 ? MaterialIds[DominantSlot] : INDEX_NONE;
}
