#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CubusVegetationRendererComponent.generated.h"

class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;

/**
 * Publishes deterministic Cubus vegetation placements as tagged debug point
 * sources and batches PVE tree placements through an instanced skinned mesh.
 */
UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (
        BlueprintSpawnableComponent,
        DisplayName = "Cubus Megaplant PCG Source"
    )
)
class ORAKAI_API UCubusVegetationRendererComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCubusVegetationRendererComponent();

    virtual void OnRegister() override;
    virtual void OnUnregister() override;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Vegetation"
    )
    void RebuildVegetation();

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Vegetation"
    )
    void ClearVegetation();

protected:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Debug"
    )
    TObjectPtr<UStaticMesh> MarkerMesh = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Placement",
        meta = (ClampMin = "1.0", Units = "cm")
    )
    float VoxelSize = 100.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Debug"
    )
    bool bShowDebugMarkers = false;

    /** Enables one batched instanced-skinned component per chunk. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PVE"
    )
    bool bRenderInstancedTrees = true;

    /** Assign the PVE Skeletal Mesh with Nanite Foliage asset here. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PVE",
        meta = (EditCondition = "bRenderInstancedTrees")
    )
    TObjectPtr<USkeletalMesh> TreeSkeletalMesh = nullptr;

    /** Hard safety limit per chunk. Zero disables the limit. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PVE",
        meta = (ClampMin = "0", EditCondition = "bRenderInstancedTrees")
    )
    int32 MaxTreeInstancesPerChunk = 64;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PVE",
        meta = (ClampMin = "0", Units = "cm", EditCondition = "bRenderInstancedTrees")
    )
    int32 TreeStartCullDistance = 20000;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PVE",
        meta = (ClampMin = "0", Units = "cm", EditCondition = "bRenderInstancedTrees")
    )
    int32 TreeEndCullDistance = 50000;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float ChangeCheckInterval = 0.5f;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Transient,
        Category = "Cubus|Vegetation|Diagnostics"
    )
    int32 PublishedPointCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Transient,
        Category = "Cubus|Vegetation|Diagnostics"
    )
    int32 BatchedTreeInstanceCount = 0;

private:
    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> GrassPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ShrubPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> TreePoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ReedsPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> AlpinePoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedSkinnedMeshComponent> TreeInstances = nullptr;

    uint32 LastPlacementHash = 0;
    float TimeUntilNextCheck = 0.0f;

    void EnsurePointComponents();
    void EnsureTreeInstanceComponent();
    uint32 CalculatePlacementHash() const;

    UInstancedStaticMeshComponent* CreatePointComponent(
        FName ComponentName,
        FName ComponentTag
    );

    UInstancedStaticMeshComponent* ResolvePointComponentForType(
        int32 TypeId
    ) const;
};
