#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CubusWorldVegetationActor.generated.h"

class ACubusBlockWorldActor;
class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;
class UPCGGraphInterface;
class USceneComponent;
class USkeletalMesh;
class UStaticMesh;

/**
 * One world-level vegetation owner for all currently streamed Cubus chunks.
 * Chunks only generate deterministic placement records; this actor owns the
 * fixed set of shared species batches used to render those records.
 */
UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus World Vegetation")
)
class ORAKAI_API ACubusWorldVegetationActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusWorldVegetationActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void ConfigureForWorld(ACubusBlockWorldActor* InBlockWorld);

    // Temporary source-compatibility overload for call sites compiled against
    // the removed PCG configuration API. The graph and bool are ignored.
    void ConfigureForWorld(
        ACubusBlockWorldActor* InBlockWorld,
        UPCGGraphInterface* InVegetationGraph,
        bool bInEnableRuntimeVegetation
    );

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void RebuildWorldVegetation();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void ClearWorldVegetation();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation")
    TObjectPtr<ACubusBlockWorldActor> BlockWorld = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bRenderWorldPlantBatches = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Species")
    TSoftObjectPtr<USkeletalMesh> ExistingTreeMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Species")
    TSoftObjectPtr<USkeletalMesh> ElderMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Species")
    TSoftObjectPtr<USkeletalMesh> NorwaySpruceMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Species")
    TSoftObjectPtr<USkeletalMesh> GreasewoodMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering", meta = (ClampMin = "0", Units = "cm"))
    int32 PlantStartCullDistance = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering", meta = (ClampMin = "0", Units = "cm"))
    int32 PlantEndCullDistance = 30000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering", meta = (ClampMin = "0"))
    int32 MaximumRenderedPlants = 4096;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming", meta = (ClampMin = "0.1", Units = "s"))
    float RefreshInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming", meta = (ClampMin = "0"))
    int32 MaximumPublishedPoints = 20000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Debug")
    TObjectPtr<UStaticMesh> MarkerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Debug")
    bool bShowDebugMarkers = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 LoadedChunkCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 PublishedPointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 RenderedPlantCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int64 PublishedPlacementHash = 0;

private:
    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> GrassPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ShrubPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> TreePoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ConiferTreePoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ReedsPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> AlpinePoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedSkinnedMeshComponent> ExistingTreeInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedSkinnedMeshComponent> ElderInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedSkinnedMeshComponent> NorwaySpruceInstances = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedSkinnedMeshComponent> GreasewoodInstances = nullptr;

    float TimeUntilRefresh = 0.0f;

    void ResolveBlockWorld();
    void EnsurePointCarriers();
    void EnsurePlantBatches();
    uint32 CalculateLoadedPlacementHash(int32& OutLoadedChunkCount) const;

    UInstancedStaticMeshComponent* CreatePointCarrier(
        FName ComponentName,
        FName ComponentTag
    );

    UInstancedSkinnedMeshComponent* CreatePlantBatch(FName ComponentName);
    UInstancedStaticMeshComponent* ResolveCarrierForType(int32 TypeId) const;
};
