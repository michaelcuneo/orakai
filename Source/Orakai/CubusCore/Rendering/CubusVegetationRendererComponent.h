#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CubusVegetationRendererComponent.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Publishes deterministic Cubus vegetation placements as tagged ISM point
 * sources and can directly spawn transient PVE tree actors for tree points.
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
        Category = "Cubus|Vegetation|PCG"
    )
    void RebuildVegetation();

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Vegetation|PCG"
    )
    void ClearVegetation();

protected:
    /**
     * Small static mesh used only to give the ISM point carriers valid bounds.
     * A basic cube or editor-only marker mesh is sufficient.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PCG"
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
        Category = "Cubus|Vegetation|PCG"
    )
    bool bShowDebugMarkers = false;

    /**
     * Emergency-disabled actor spawning path.
     * Transient prevents a previously saved Blueprint value from enabling it
     * again while the project is loading.
     */
    UPROPERTY(
        Transient,
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Vegetation|PVE"
    )
    bool bSpawnTreeActors = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PVE",
        meta = (EditCondition = "bSpawnTreeActors")
    )
    TSubclassOf<AActor> TreeActorClass;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|PCG",
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
    int32 SpawnedTreeActorCount = 0;

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
    TArray<TObjectPtr<AActor>> SpawnedTreeActors;

    uint32 LastPlacementHash = 0;
    float TimeUntilNextCheck = 0.0f;

    void EnsurePointComponents();
    void DestroySpawnedTreeActors();
    uint32 CalculatePlacementHash() const;

    UInstancedStaticMeshComponent* CreatePointComponent(
        FName ComponentName,
        FName ComponentTag
    );

    UInstancedStaticMeshComponent* ResolvePointComponentForType(
        int32 TypeId
    ) const;
};
