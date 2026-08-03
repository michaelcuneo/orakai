#pragma once

#include "CoreMinimal.h"
#include "CubusProceduralTreeGenerator.generated.h"

class UCubusTreeSpecies;

USTRUCT(BlueprintType)
struct ORAKAI_API FCubusTreeMeshSection
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FVector> Vertices;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<int32> Triangles;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FVector> Normals;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FVector2D> UV0;

    /**
     * R = wind strength, G = height mask, B = deterministic phase,
     * A = stiffness. These channels are intentionally compatible with the
     * existing Cubus foliage wind bridge.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FLinearColor> VertexColors;

    void Reset();
    bool IsValid() const;
};

USTRUCT(BlueprintType)
struct ORAKAI_API FCubusGeneratedTreeMesh
{
    GENERATED_BODY()

    /** Material slot 0. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FCubusTreeMeshSection Bark;

    /** Material slot 1. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FCubusTreeMeshSection Canopy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FBox Bounds = FBox(EForceInit::ForceInit);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float GeneratedHeight = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 Seed = 0;

    void Reset();
    bool IsValid() const;
};

/**
 * Deterministic low-poly tree generator. The output is plain mesh data so it
 * can be baked into UStaticMesh assets, previewed with a procedural mesh, or
 * cached and instanced by the existing vegetation renderer.
 */
struct ORAKAI_API FCubusProceduralTreeGenerator
{
    static bool BuildTree(
        const UCubusTreeSpecies& Species,
        int32 Seed,
        FCubusGeneratedTreeMesh& OutMesh
    );
};