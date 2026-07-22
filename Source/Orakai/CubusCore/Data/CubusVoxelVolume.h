#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Data/CubusVoxel.h"

/**
 * Contiguous three-dimensional voxel storage.
 */
class ORAKAI_API FCubusVoxelVolume
{
public:
    FCubusVoxelVolume();

    explicit FCubusVoxelVolume(
        const FIntVector& InDimensions
    );

    void Initialize(
        const FIntVector& InDimensions
    );

    const FIntVector& GetDimensions() const
    {
        return Dimensions;
    }

    int32 GetVoxelCount() const
    {
        return Voxels.Num();
    }

    int32 GetSolidVoxelCount() const;

    bool IsInside(
        int32 X,
        int32 Y,
        int32 Z
    ) const;

    bool IsInside(
        const FIntVector& Position
    ) const;

    bool IsEmpty(
        int32 X,
        int32 Y,
        int32 Z
    ) const;

    bool IsEmpty(
        const FIntVector& Position
    ) const;

    FCubusVoxel* GetVoxel(
        int32 X,
        int32 Y,
        int32 Z
    );

    const FCubusVoxel* GetVoxel(
        int32 X,
        int32 Y,
        int32 Z
    ) const;

    FCubusVoxel* GetVoxel(
        const FIntVector& Position
    );

    const FCubusVoxel* GetVoxel(
        const FIntVector& Position
    ) const;

    bool SetVoxel(
        int32 X,
        int32 Y,
        int32 Z,
        const FCubusVoxel& Voxel
    );

    bool SetVoxel(
        const FIntVector& Position,
        const FCubusVoxel& Voxel
    );

    bool SetMaterialId(
        int32 X,
        int32 Y,
        int32 Z,
        int32 MaterialId
    );

    bool SetMaterialId(
        const FIntVector& Position,
        int32 MaterialId
    );

    void Fill(
        const FCubusVoxel& Voxel
    );

    void Clear();

private:
    FIntVector Dimensions = FIntVector::ZeroValue;

    TArray<FCubusVoxel> Voxels;

    int32 ToIndex(
        int32 X,
        int32 Y,
        int32 Z
    ) const;

    static FIntVector SanitizeDimensions(
        const FIntVector& InDimensions
    );
};