#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusProceduralTreeActor.generated.h"

class UCubusTreeSpecies;
class UProceduralMeshComponent;

/**
 * Editor and runtime preview for one generated stylised tree.
 * Section 0 is bark and section 1 is canopy.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Cubus")
class ORAKAI_API ACubusProceduralTreeActor final : public AActor
{
    GENERATED_BODY()

public:
    ACubusProceduralTreeActor();

    virtual void OnConstruction(const FTransform& Transform) override;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Tree")
    void RebuildTree();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Tree")
    void RandomizeSeed();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Tree")
    TObjectPtr<UProceduralMeshComponent> TreeMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Tree")
    TObjectPtr<UCubusTreeSpecies> Species;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Tree")
    int32 Seed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Tree")
    bool bGenerateCollision = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Tree")
    bool bRebuildOnConstruction = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Tree|Diagnostics")
    int32 BarkTriangleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Tree|Diagnostics")
    int32 CanopyTriangleCount = 0;
};