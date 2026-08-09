#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusPCGVoxelVolumeActor.generated.h"

/**
 * Compatibility chunk class retained for existing Blueprint references.
 * Vegetation rendering is owned exclusively by ACubusWorldVegetationActor.
 */
UCLASS(
    Transient,
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus Block Chunk")
)
class ORAKAI_API ACubusPCGVoxelVolumeActor : public ACubusVoxelVolumeActor
{
    GENERATED_BODY()

public:
    ACubusPCGVoxelVolumeActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GenerateTerrainData() override;

    void SetTerrainRayTracingEnabled(bool bEnabled);

    bool IsTerrainRayTracingRequested() const
    {
        return bTerrainRayTracingRequested;
    }

private:
    bool bTerrainRayTracingRequested = false;
};
