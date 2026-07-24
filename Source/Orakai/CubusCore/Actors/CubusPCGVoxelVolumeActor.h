#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusPCGVoxelVolumeActor.generated.h"

class UCubusVegetationRendererComponent;
class UPCGComponent;
class UPCGGraphInterface;

/**
 * Cubus chunk actor with a non-partitioned PCG component owned by the chunk.
 *
 * Every loaded chunk generates and cleans its own Megaplant output. No world
 * sized PCG volume is required; destroying the chunk also removes its PCG
 * resources.
 */
UCLASS(
    Transient,
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus PCG Block Chunk")
)
class ORAKAI_API ACubusPCGVoxelVolumeActor : public ACubusVoxelVolumeActor
{
    GENERATED_BODY()

public:
    ACubusPCGVoxelVolumeActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GenerateTestShapeData() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    void SetTerrainRayTracingEnabled(bool bEnabled);

    bool IsTerrainRayTracingRequested() const
    {
        return bTerrainRayTracingRequested;
    }

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Vegetation|PCG"
    )
    void RegenerateVegetationPCG();

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Vegetation|PCG"
    )
    void CleanupVegetationPCG();

    void ConfigureVegetationPCG(
        UPCGGraphInterface* InVegetationGraph,
        bool bInGenerateVegetationPCG
    );

protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Components"
    )
    TObjectPtr<UCubusVegetationRendererComponent>
        VegetationPointSource;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Components"
    )
    TObjectPtr<UPCGComponent> VegetationPCG;

    /** One graph asset is reused by every chunk instance. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PCG"
    )
    TObjectPtr<UPCGGraphInterface> VegetationGraph = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PCG"
    )
    bool bGenerateVegetationPCG = true;

private:
    TObjectPtr<UPCGGraphInterface> LastConfiguredGraph = nullptr;
    bool bTerrainRayTracingRequested = false;

    void ConfigurePCGComponent();
};
