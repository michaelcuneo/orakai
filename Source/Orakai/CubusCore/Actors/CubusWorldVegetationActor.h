#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CubusWorldVegetationActor.generated.h"

class ACubusBlockWorldActor;
class UInstancedStaticMeshComponent;
class UPCGComponent;
class UPCGGraphInterface;
class USceneComponent;
class UStaticMesh;

/**
 * One world-level vegetation owner for all currently streamed Cubus chunks.
 *
 * Chunks only generate deterministic vegetation placement records. This actor
 * merges those records into shared tagged point carriers and executes one PCG
 * graph for the loaded region. No PCG or skinned vegetation component is owned
 * by an individual runtime chunk.
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

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void RebuildWorldVegetation();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void ClearWorldVegetation();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<UPCGComponent> VegetationPCG;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation")
    TObjectPtr<ACubusBlockWorldActor> BlockWorld = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PCG")
    TObjectPtr<UPCGGraphInterface> VegetationGraph = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|PCG")
    bool bEnableRuntimeVegetation = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Streaming",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float RefreshInterval = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Streaming",
        meta = (ClampMin = "0")
    )
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
    uint32 PublishedPlacementHash = 0;

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

    float TimeUntilRefresh = 0.0f;
    TObjectPtr<UPCGGraphInterface> LastConfiguredGraph = nullptr;

    void ResolveBlockWorld();
    void EnsurePointCarriers();
    void ConfigurePCG();
    uint32 CalculateLoadedPlacementHash(int32& OutLoadedChunkCount) const;

    UInstancedStaticMeshComponent* CreatePointCarrier(
        FName ComponentName,
        FName ComponentTag
    );

    UInstancedStaticMeshComponent* ResolveCarrierForType(int32 TypeId) const;
};
