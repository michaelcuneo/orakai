#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusProceduralTreeStreamer.generated.h"

class ACubusBlockWorldActor;
class ACubusVoxelVolumeActor;
class UCubusTreeSpecies;
class UProceduralMeshComponent;
class USceneComponent;

USTRUCT()
struct FCubusStreamedTreeChunk
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> Mesh = nullptr;

    uint32 Signature = 0;
};

/**
 * Streams generated blocky trees from the existing chunk vegetation records.
 * One procedural mesh component is owned per visible chunk. Each component has
 * bark in section 0 and canopy in section 1.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Cubus")
class ORAKAI_API ACubusProceduralTreeStreamer final : public AActor
{
    GENERATED_BODY()

public:
    ACubusProceduralTreeStreamer();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Trees|Streaming")
    void RebuildStreamedTrees();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Trees|Streaming")
    void ClearStreamedTrees();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Streaming")
    TObjectPtr<ACubusBlockWorldActor> BlockWorld = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Species")
    TObjectPtr<UCubusTreeSpecies> BroadleafSpecies = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Species")
    TObjectPtr<UCubusTreeSpecies> ConiferSpecies = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Streaming", meta = (ClampMin = "0.1", Units = "s"))
    float RefreshInterval = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Streaming")
    bool bCullByCameraChunkRadius = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Streaming", meta = (ClampMin = "0", UIMax = "24", EditCondition = "bCullByCameraChunkRadius"))
    int32 CameraChunkHorizontalRadius = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Streaming", meta = (ClampMin = "0", UIMax = "12", EditCondition = "bCullByCameraChunkRadius"))
    int32 CameraChunkVerticalRadius = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Generation", meta = (ClampMin = "1", ClampMax = "32"))
    int32 VariantsPerSpecies = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Generation")
    int32 GenerationSeed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Generation", meta = (ClampMin = "0.01"))
    float GlobalTreeScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Generation")
    bool bGenerateCollision = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Trees|Rendering")
    bool bCastShadows = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Trees|Diagnostics")
    int32 StreamedChunkCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Trees|Diagnostics")
    int32 StreamedTreeCount = 0;

private:
    UPROPERTY(Transient)
    TMap<FIntVector, FCubusStreamedTreeChunk> StreamedChunks;

    float TimeUntilRefresh = 0.0f;

    void ResolveBlockWorld();
    void RefreshVisibleChunks();
    void BuildChunkTrees(ACubusVoxelVolumeActor& Chunk, uint32 Signature);
    bool IsChunkVisible(const ACubusVoxelVolumeActor& Chunk, const FVector& CameraLocation, bool bHasCamera) const;
};