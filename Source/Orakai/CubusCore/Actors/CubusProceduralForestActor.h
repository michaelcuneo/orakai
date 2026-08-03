#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusProceduralForestActor.generated.h"

class UCubusTreeSpecies;
class UProceduralMeshComponent;

/**
 * Immediate forest preview/runtime actor. It merges many deterministic trees
 * into one bark section and one canopy section so a whole stand can be tested
 * without spawning hundreds of individual actors.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Cubus")
class ORAKAI_API ACubusProceduralForestActor final : public AActor
{
    GENERATED_BODY()

public:
    ACubusProceduralForestActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Forest")
    void RebuildForest();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Forest")
    void RandomizeForest();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Forest")
    TObjectPtr<UProceduralMeshComponent> ForestMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest")
    TObjectPtr<UCubusTreeSpecies> Species;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest")
    int32 Seed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest", meta = (ClampMin = "1", ClampMax = "512"))
    int32 TreeCount = 48;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest", meta = (ClampMin = "100.0", Units = "cm"))
    float ForestRadius = 6000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest", meta = (ClampMin = "0.0", Units = "cm"))
    float MinimumSpacing = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest")
    FVector2D ScaleRange = FVector2D(0.8f, 1.25f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest")
    bool bSnapTreesToGround = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest", meta = (EditCondition = "bSnapTreesToGround", ClampMin = "100.0", Units = "cm"))
    float GroundTraceHeight = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest", meta = (EditCondition = "bSnapTreesToGround", ClampMin = "100.0", Units = "cm"))
    float GroundTraceDepth = 20000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Forest")
    bool bRebuildOnConstruction = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Forest|Diagnostics")
    int32 GeneratedTreeCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Forest|Diagnostics")
    int32 BarkTriangleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Forest|Diagnostics")
    int32 CanopyTriangleCount = 0;
};