// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Orakai.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Input/SVirtualJoystick.h"

namespace OrakaiTerrainInspector
{
	constexpr uint64 ScreenMessageKey = 0x43554255534D4154ull;

	bool ResolveSectionAndFace(
		const UProceduralMeshComponent* ProceduralMesh,
		const FHitResult& Hit,
		const FProcMeshSection*& OutSection,
		int32& OutFaceIndex,
		int32& OutSectionIndex
	)
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
			if (const FProcMeshSection* Section =
				ProceduralMesh->GetProcMeshSection(Hit.Item))
			{
				const int32 TriangleCount =
					Section->ProcIndexBuffer.Num() / 3;

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
			const FProcMeshSection* Section =
				ProceduralMesh->GetProcMeshSection(SectionIndex);

			if (Section == nullptr)
			{
				continue;
			}

			const int32 TriangleCount =
				Section->ProcIndexBuffer.Num() / 3;

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

			if (const FCubusMaterialDefinition* Definition =
				Registry->FindMaterialDefinition(MaterialId))
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
}

AOrakaiPlayerController::AOrakaiPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AOrakaiPlayerController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bShowTerrainMaterialInspector && IsLocalPlayerController())
	{
		UpdateTerrainMaterialInspector();
	}
}

void AOrakaiPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogOrakai, Error, TEXT("Could not spawn mobile controls widget."));
		}

	}
}

void AOrakaiPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
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
	// are we on a mobile platform? Should we force touch?
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

	const FVector TraceEnd =
		ViewLocation + ViewRotation.Vector() * TerrainMaterialTraceDistance;

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(OrakaiTerrainMaterialInspector),
		true,
		GetPawn()
	);
	QueryParams.bReturnFaceIndex = true;
	QueryParams.bTraceComplex = true;

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		ViewLocation,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	if (!bHit)
	{
		GEngine->AddOnScreenDebugMessage(
			OrakaiTerrainInspector::ScreenMessageKey,
			0.0f,
			FColor::Silver,
			TEXT("Terrain material: no hit")
		);
		return;
	}

	const int32 MaterialId = ResolveRenderedTerrainMaterialId(Hit);
	const FString MaterialName = MaterialId > 0
		? OrakaiTerrainInspector::ResolveMaterialName(MaterialId)
		: TEXT("Not a Cubus density triangle");

	const FString Message = MaterialId > 0
		? FString::Printf(
			TEXT("Terrain material: %s [ID %d]"),
			*MaterialName,
			MaterialId
		)
		: FString::Printf(
			TEXT("Terrain material: %s | Actor: %s"),
			*MaterialName,
			*GetNameSafe(Hit.GetActor())
		);

	GEngine->AddOnScreenDebugMessage(
		OrakaiTerrainInspector::ScreenMessageKey,
		0.0f,
		MaterialId > 0 ? FColor::Yellow : FColor::Silver,
		Message
	);

	DrawDebugPoint(
		GetWorld(),
		Hit.ImpactPoint,
		12.0f,
		FColor::Yellow,
		false,
		0.0f
	);
}

int32 AOrakaiPlayerController::ResolveRenderedTerrainMaterialId(
	const FHitResult& Hit
) const
{
	const UProceduralMeshComponent* ProceduralMesh =
		Cast<UProceduralMeshComponent>(Hit.GetComponent());

	const FProcMeshSection* Section = nullptr;
	int32 FaceIndex = INDEX_NONE;
	int32 SectionIndex = INDEX_NONE;

	if (!OrakaiTerrainInspector::ResolveSectionAndFace(
		ProceduralMesh,
		Hit,
		Section,
		FaceIndex,
		SectionIndex
	))
	{
		return INDEX_NONE;
	}

	const int32 FirstIndex = FaceIndex * 3;
	if (FirstIndex < 0 || FirstIndex + 2 >= Section->ProcIndexBuffer.Num())
	{
		return INDEX_NONE;
	}

	const uint32 VertexIndices[3] =
	{
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

	const FProcMeshVertex& Vertex0 =
		Section->ProcVertexBuffer[VertexIndices[0]];
	const FProcMeshVertex& Vertex1 =
		Section->ProcVertexBuffer[VertexIndices[1]];
	const FProcMeshVertex& Vertex2 =
		Section->ProcVertexBuffer[VertexIndices[2]];

	const int32 PackingBase = FCubusDensityMesher::MaterialIdPackingBase;
	const int32 Packed01 = FMath::RoundToInt(Vertex0.UV0.X);
	const int32 Packed23 = FMath::RoundToInt(Vertex0.UV0.Y);

	const int32 MaterialIds[4] =
	{
		Packed01 % PackingBase,
		Packed01 / PackingBase,
		Packed23 % PackingBase,
		Packed23 / PackingBase
	};

	const FLinearColor AverageWeights =
		(
			FLinearColor(Vertex0.Color) +
			FLinearColor(Vertex1.Color) +
			FLinearColor(Vertex2.Color)
		) / 3.0f;

	const float Weights[4] =
	{
		AverageWeights.R,
		AverageWeights.G,
		AverageWeights.B,
		AverageWeights.A
	};

	int32 DominantSlot = 0;
	for (int32 Slot = 1; Slot < 4; ++Slot)
	{
		if (Weights[Slot] > Weights[DominantSlot])
		{
			DominantSlot = Slot;
		}
	}

	return MaterialIds[DominantSlot] > 0
		? MaterialIds[DominantSlot]
		: INDEX_NONE;
}
