#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CubusVegetationRendererComponent.generated.h"

class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;

UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (
        BlueprintSpawnableComponent,
        DisplayName = "Cubus Megaplant Renderer"
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

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void RebuildVegetation();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void ClearVegetation();

    UFUNCTION(BlueprintCallable, Category = "Cubus|Vegetation|Streaming")
    void SetVegetationActive(bool bActive);

    UFUNCTION(BlueprintPure, Category = "Cubus|Vegetation|Streaming")
    bool IsVegetationActive() const
    {
        return bVegetationActive;
    }

    void SetConfigurationOnly(const bool bInConfigurationOnly)
    {
        bConfigurationOnly = bInConfigurationOnly;
        if (bConfigurationOnly)
        {
            SetComponentTickEnabled(false);
        }
    }

    bool IsConfigurationOnly() const
    {
        return bConfigurationOnly;
    }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Debug")
    TObjectPtr<UStaticMesh> MarkerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float VoxelSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Debug")
    bool bShowDebugMarkers = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE")
    bool bRenderInstancedTrees = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE|Growth", meta = (EditCondition = "bRenderInstancedTrees"))
    TObjectPtr<USkeletalMesh> SeedlingTreeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE|Growth", meta = (EditCondition = "bRenderInstancedTrees"))
    TObjectPtr<USkeletalMesh> SaplingTreeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE|Growth", meta = (EditCondition = "bRenderInstancedTrees"))
    TObjectPtr<USkeletalMesh> YoungTreeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE|Growth", meta = (EditCondition = "bRenderInstancedTrees"))
    TObjectPtr<USkeletalMesh> MatureTreeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE|Growth", meta = (EditCondition = "bRenderInstancedTrees"))
    bool bSimulateTreeGrowth = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE|Growth", meta = (ClampMin = "1.0", Units = "s", EditCondition = "bRenderInstancedTrees && bSimulateTreeGrowth"))
    float GrowthStageDurationSeconds = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE", meta = (ClampMin = "0", EditCondition = "bRenderInstancedTrees"))
    int32 MaxTreeInstancesPerChunk = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE", meta = (ClampMin = "0", Units = "cm", EditCondition = "bRenderInstancedTrees"))
    int32 TreeStartCullDistance = 4800;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PVE", meta = (ClampMin = "0", Units = "cm", EditCondition = "bRenderInstancedTrees"))
    int32 TreeEndCullDistance = 9600;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming", meta = (ClampMin = "0", Units = "cm"))
    float VegetationActivationDistance = 6400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming", meta = (ClampMin = "0", Units = "cm"))
    float VegetationDeactivationDistance = 9600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation", meta = (ClampMin = "0.1", Units = "s"))
    float ChangeCheckInterval = 0.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 PublishedPointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 BatchedTreeInstanceCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    bool bVegetationActive = false;

private:
    bool bConfigurationOnly = false;

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
    TObjectPtr<UInstancedSkinnedMeshComponent> SeedlingTreeInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedSkinnedMeshComponent> SaplingTreeInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedSkinnedMeshComponent> YoungTreeInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedSkinnedMeshComponent> MatureTreeInstances = nullptr;

    uint32 LastPlacementHash = 0;
    float TimeUntilNextCheck = 0.0f;
    double GrowthStartTimeSeconds = -1.0;
    int32 LastGrowthStep = INDEX_NONE;

    void EnsurePointComponents();
    void EnsureTreeInstanceComponents();
    void DestroyTreeInstanceComponents();
    void DestroyTreeStageComponent(TObjectPtr<UInstancedSkinnedMeshComponent>& Component);
    void UpdateStreamingState();
    int32 GetCurrentGrowthStep() const;
    uint32 CalculatePlacementHash() const;

    UInstancedSkinnedMeshComponent* CreateTreeStageComponent(FName ComponentName);
    UInstancedStaticMeshComponent* CreatePointComponent(FName ComponentName, FName ComponentTag);
    UInstancedStaticMeshComponent* ResolvePointComponentForType(int32 TypeId) const;
};
