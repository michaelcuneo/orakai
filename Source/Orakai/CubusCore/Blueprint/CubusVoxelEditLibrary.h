#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CubusVoxelEditLibrary.generated.h"

class ACubusBlockWorldActor;

/**
 * Blueprint-facing helpers for adding and removing Cubus block voxels.
 *
 * The player, controller, input actions, and line trace remain entirely in
 * Blueprint. Pass the trace hit into these functions to resolve the correct
 * voxel on either side of the hit face.
 */
UCLASS()
class ORAKAI_API UCubusVoxelEditLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Resolve the solid voxel struck by a trace hit.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "Cubus|Edits",
        meta = (DisplayName = "Resolve Hit Cubus Voxel")
    )
    static bool ResolveHitVoxel(
        const FHitResult& Hit,
        FIntVector& OutWorldVoxel,
        ACubusBlockWorldActor*& OutBlockWorld
    );

    /**
     * Resolve the empty voxel immediately outside the struck face.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "Cubus|Edits",
        meta = (DisplayName = "Resolve Adjacent Cubus Voxel")
    )
    static bool ResolveAdjacentVoxel(
        const FHitResult& Hit,
        FIntVector& OutWorldVoxel,
        ACubusBlockWorldActor*& OutBlockWorld
    );

    /**
     * Add a voxel in the empty cell immediately outside the struck face.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Edits",
        meta = (DisplayName = "Add Cubus Voxel From Hit")
    )
    static bool AddVoxelFromHit(
        const FHitResult& Hit,
        int32 MaterialId,
        bool bIsWater = false
    );

    /**
     * Remove the solid voxel struck by the trace.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "Cubus|Edits",
        meta = (DisplayName = "Remove Cubus Voxel From Hit")
    )
    static bool RemoveVoxelFromHit(const FHitResult& Hit);
};
