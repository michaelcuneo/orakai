#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"

#include "CubusDensityMeshComponent.generated.h"

class ACubusVoxelVolumeActor;
class UCubusMaterialRegistry;
class UMaterialInterface;

/**
 * Parallel smooth-mesh renderer for an existing Cubus voxel chunk.
 *
 * The default path samples the configured terrain function directly as a
 * continuous scalar field. The older block-backed field remains available as
 * a diagnostic fallback while the hybrid data model is being completed.
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

    virtual void BeginPlay() override;

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Density"
    )
    bool RebuildDensityMesh();

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Density"
    )
    void ClearDensityMesh();

    void ConfigureTerrainDensity(
        const FCubusTerrainDensitySettings& InSettings
    )
    {
        TerrainDensitySettings = InSettings;
    }

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Density"
    )
    bool bAutoRebuildOnBeginPlay = true;

    /**
     * Uses fractional terrain height directly. Disable only when comparing
     * against the transitional block-occupancy density adapter.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Density"
    )
    bool bUseNativeTerrainDensity = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Density"
    )
    bool bTreatWaterAsEmpty = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Density",
        meta = (ClampMin = "0.0001")
    )
    float DensityMagnitude = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Density",
        meta = (UIMin = "-1.0", UIMax = "1.0")
    )
    float IsoLevel = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Density|Collision"
    )
    bool bGenerateDensityCollision = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Density|Materials"
    )
    TObjectPtr<UCubusMaterialRegistry> MaterialRegistry = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Density|Materials"
    )
    TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Transient,
        Category = "Cubus|Density|Diagnostics"
    )
    int32 GeneratedDensityVertexCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Transient,
        Category = "Cubus|Density|Diagnostics"
    )
    int32 GeneratedDensityTriangleCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Transient,
        Category = "Cubus|Density|Diagnostics"
    )
    int32 GeneratedDensitySectionCount = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Transient,
        Category = "Cubus|Density|Diagnostics",
        meta = (Units = "ms")
    )
    float LastDensityBuildTimeMilliseconds = 0.0f;

private:
    FCubusTerrainDensitySettings TerrainDensitySettings;

    FCubusBlockVoxel SampleVoxelAtWorldCoordinate(
        const ACubusVoxelVolumeActor& OwnerChunk,
        const FIntVector& WorldVoxelCoordinate
    ) const;

    static FIntVector WorldVoxelToChunkCoordinate(
        const FIntVector& WorldVoxelCoordinate
    );

    static int32 FloorDivide(
        int32 Value,
        int32 PositiveDivisor
    );

    void RebuildDensityMeshDeferred();
    void ResetDensityDiagnostics();
};
