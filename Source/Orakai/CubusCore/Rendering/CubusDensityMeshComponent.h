#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

#include "CubusDensityMeshComponent.generated.h"

/**
 * Serialization compatibility shell for BP_CubusVoxelPCGChunk assets saved
 * while density used a dedicated child procedural-mesh component.
 *
 * Density rendering no longer uses this component. Blocks, Density and Hybrid
 * all submit sections to ACubusVoxelVolumeActor's root ProceduralMesh. Keeping
 * the class available allows old DensityMesh and BodySetup exports to load so
 * the Blueprint can be resaved without package errors.
 */
UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (BlueprintSpawnableComponent)
)
class ORAKAI_API UCubusDensityMeshComponent : public UProceduralMeshComponent
{
    GENERATED_BODY()

public:
    UCubusDensityMeshComponent(
        const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()
    );

    virtual void PostLoad() override;
    virtual void OnRegister() override;

private:
    void DisableLegacyRenderer();
};
