#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "CubusTerrainLodWorldSubsystem.generated.h"

/**
 * Creates the transient terrain-LOD world actor automatically for game worlds.
 * No existing level asset or ACubusBlockWorldActor wiring is required.
 */
UCLASS()
class ORAKAI_API UCubusTerrainLodWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
