#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CubusVoxelEditLibrary.generated.h"

class ACubusBlockWorldActor;

UCLASS()
class ORAKAI_API UCubusVoxelEditLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Resolve Cubus Hit Voxel"))
    static bool ResolveHitVoxel(
        const FHitResult& Hit,
        FIntVector& OutWorldVoxel,
        ACubusBlockWorldActor*& OutBlockWorld
    );

    UFUNCTION(BlueprintPure, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Resolve Cubus Adjacent Voxel"))
    static bool ResolveAdjacentVoxel(
        const FHitResult& Hit,
        FIntVector& OutWorldVoxel,
        ACubusBlockWorldActor*& OutBlockWorld
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Remove Cubus Voxel From Hit"))
    static bool RemoveVoxelFromHit(const FHitResult& Hit);

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Add Cubus Voxel From Hit"))
    static bool AddVoxelFromHit(
        const FHitResult& Hit,
        int32 MaterialId,
        bool bIsWater = false
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Remove Cubus Block Brush From Hit"))
    static int32 RemoveBlockBrushFromHit(
        const FHitResult& Hit,
        int32 BrushRadius = 1
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Add Cubus Block Brush From Hit"))
    static int32 AddBlockBrushFromHit(
        const FHitResult& Hit,
        int32 BrushRadius,
        int32 MaterialId,
        bool bIsWater = false
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Remove Cubus Density From Hit"))
    static int32 RemoveDensityFromHit(
        const FHitResult& Hit,
        int32 BrushRadius = 1,
        float Strength = 2.0f
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Add Cubus Density From Hit"))
    static int32 AddDensityFromHit(
        const FHitResult& Hit,
        int32 BrushRadius,
        float Strength,
        int32 MaterialId
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Smooth Cubus Density From Hit"))
    static int32 SmoothDensityFromHit(
        const FHitResult& Hit,
        int32 BrushRadius,
        float Strength
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Level Cubus Density From Hit"))
    static int32 LevelDensityFromHit(
        const FHitResult& Hit,
        int32 BrushRadius,
        float Strength,
        int32 MaterialId
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Voxel Editing", meta = (DisplayName = "Restore Cubus Density From Hit"))
    static int32 RestoreDensityFromHit(
        const FHitResult& Hit,
        int32 BrushRadius,
        float Strength
    );
};
