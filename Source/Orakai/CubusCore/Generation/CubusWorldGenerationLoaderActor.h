#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "CubusCore/Generation/CubusTerrainCarving.h"
#include "CubusCore/Generation/CubusTerrainErosion.h"
#include "CubusCore/Generation/CubusTerrainDeposition.h"
#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"
#include "CubusCore/Generation/CubusWorldGenerationPreviewActor.h"
#include "CubusWorldGenerationLoaderActor.generated.h"

class ACubusBlockWorldActor;
class ACubusTerrainLodWorldActor;
class ACubusSpawnStreamingPawn;
class UImage;
class UTexture2D;

UENUM(BlueprintType)
enum class ECubusGenerationLoaderStage : uint8
{
    WaitingForSeed UMETA(DisplayName="Waiting for Seed"),
    Structural