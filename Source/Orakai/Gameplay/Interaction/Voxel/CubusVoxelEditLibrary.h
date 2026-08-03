#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CubusVoxelEditLibrary.generated.h"

class ACubusBlockWorldActor;

/**
 * Blueprint-facing helpers for editing Cubus block voxels.
 *
 * Input handling and line tracing remain in Blueprint. Pass the resulting
 * hit into these functions to remove the struck voxel or place a voxel
 * against the struck face.
 */
UCLASS()
class ORAKAI_API UCubusVoxelEditLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Resolve the solid voxel immediately inside the struck face.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "Cubus|Voxel Editing",
        meta = (DisplayName = "Resolve Cubus Hit Voxel")
    )
    static bool ResolveHitVoxel(
        const FHitResult& Hit,
        FIntVector& OutWorldVoxel,
        ACubusBlockWorldActor*& OutBlockWorld
    );

    /**
     * Resolve the neighbouring voxel immediately outside the struck face.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "Cubus|Voxel Editing",
        meta = (DisplayName = "Resolve Cubus Adjacent Voxel")
    )
    static bool ResolveAdjacentVoxel(
        const FHitResult& Hit,
        FIntVector& OutWorldVoxel,
        ACubusBlockWorldActor*& OutBlockWorld
    );

    /**
     * Remove the solid voxel struck by the supplied trace.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Voxel Editing",
        meta = (DisplayName = "Remove Cubus Voxel From Hit")
    )
    static bool RemoveVoxelFromHit(
        const FHitResult& Hit
    );

    /**
     * Place a voxel in the empty cell immediately outside the struck face.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Voxel Editing",
        meta = (DisplayName = "Add Cubus Voxel From Hit")
    )
    static bool AddVoxelFromHit(
        const FHitResult& Hit,
        int32 MaterialId,
        bool bIsWater = false
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Voxel Editing",
        meta = (DisplayName = "Remove Cubus Block Brush From Hit")
    )
    static int32 RemoveBlockBrushFromHit(
        const FHitResult& Hit,
        int32 BrushRadius = 1
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Voxel Editing",
        meta = (DisplayName = "Add Cubus Block Brush From Hit")
    )
    static int32 AddBlockBrushFromHit(
        const FHitResult& Hit,
        int32 BrushRadius,
        int32 MaterialId,
        bool bIsWater = false
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Voxel Editing",
        meta = (DisplayName = "Remove Cubus Density From Hit")
    )
    static int32 RemoveDensityFromHit(
        const FHitResult& Hit,
        int32 BrushRadius = 1,
        float Strength = 2.0f
    );

    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Voxel Editing",
        meta = (DisplayName = "Add Cubus Density From Hit")
    )
    static int32 AddDensityFromHit(
        const FHitResult& Hit,
        int32 BrushRadius,
        float Strength,
        int32 MaterialId
    );
};
