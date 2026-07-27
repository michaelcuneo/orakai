#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusPCGVoxelVolumeActor.generated.h"

class UCubusVegetationRendererComponent;
class UPCGGraphInterface;

/**
 * Compatibility chunk class retained for existing Blueprint references.
 * Vegetation rendering is owned exclusively by ACubusWorldVegetationActor.
 *
 * The VegetationPointSource component remains as an unregistered configuration
 * container so existing Blueprint foliage mesh assignments are preserved and
 * editable without reviving the old per-chunk renderer.
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

protected:
    /**
     * Inspector-only foliage configuration inherited by BP_CubusVoxelPCGChunk.
     * bAutoRegister is disabled in the constructor, so this component cannot
     * create point carriers, skinned batches, ticks, or render-state resources.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Vegetation|Configuration"
    )
    TObjectPtr<UCubusVegetationRendererComponent> VegetationPointSource;

private:
    bool bTerrainRayTracingRequested = false;
};
