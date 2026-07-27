#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusPCGVoxelVolumeActor.generated.h"

class UPCGGraphInterface;

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
    virtual void GenerateTestShapeData() override;

    void SetTerrainRayTracingEnabled(bool bEnabled);

    bool IsTerrainRayTracingRequested() const
    {
        return bTerrainRayTracingRequested;
    }

    // Deprecated compatibility entry points. They intentionally do nothing;
    // vegetation is generated and rendered by the world vegetation actor.
    void ConfigureVegetationPCG(
        UPCGGraphInterface* InVegetationGraph,
        bool bInGenerateVegetationPCG
    );

    void RegenerateVegetationPCG();
    void CleanupVegetationPCG();

private:
    bool bTerrainRayTracingRequested = false;
};
