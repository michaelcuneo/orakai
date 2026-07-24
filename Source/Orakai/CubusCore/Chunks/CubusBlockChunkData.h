#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"

/**
 * Fixed-size block voxel storage for one Cubus chunk.
 */
class ORAKAI_API FCubusBlockChunkData
{
public:
    explicit FCubusBlockChunkData(
        const FIntVector& InChunkCoordinate = FIntVector::ZeroValue
    );

    const FIntVector& GetChunkCoordinate() const
    {
        return ChunkCoordinate;
    }

    void SetChunkCoordinate(const FIntVector& InChunkCoordinate)
    {
        ChunkCoordinate = InChunkCoordinate;
    }

    const FCubusGenerationSeeds& GetGenerationSeeds() const
    {
        return GenerationSeeds;
    }

    void SetGenerationSeeds(const FCubusGenerationSeeds& InGenerationSeeds)
    {
        GenerationSeeds = InGenerationSeeds;
    }

    int32 GetVoxelCount() const
    {
        return Voxels.Num();
    }

    int32 GetOccupiedVoxelCount() const;
    bool HasAnyOccupiedVoxel() const;

    bool IsEmpty(int32 X, int32 Y, int32 Z) const;
    bool IsEmpty(const FIntVector& LocalCoordinate) const;

    FCubusBlockVoxel* GetVoxel(int32 X, int32 Y, int32 Z);
    const FCubusBlockVoxel* GetVoxel(int32 X, int32 Y, int32 Z) const;
    FCubusBlockVoxel* GetVoxel(const FIntVector& LocalCoordinate);
    const FCubusBlockVoxel* GetVoxel(const FIntVector& LocalCoordinate) const;

    FCubusBlockVoxel& GetVoxelChecked(int32 X, int32 Y, int32 Z);
    const FCubusBlockVoxel& GetVoxelChecked(int32 X, int32 Y, int32 Z) const;

    bool SetVoxel(int32 X, int32 Y, int32 Z, const FCubusBlockVoxel& Voxel);
    bool SetVoxel(const FIntVector& LocalCoordinate, const FCubusBlockVoxel& Voxel);

    bool SetMaterialId(int32 X, int32 Y, int32 Z, int32 MaterialId);
    bool SetMaterialId(const FIntVector& LocalCoordinate, int32 MaterialId);

    FIntVector LocalToWorldVoxel(int32 X, int32 Y, int32 Z) const;
    FIntVector LocalToWorldVoxel(const FIntVector& LocalCoordinate) const;

    void Fill(const FCubusBlockVoxel& Voxel);
    void Clear();

    TConstArrayView<FCubusBlockVoxel> GetVoxelView() const
    {
        return MakeArrayView(Voxels);
    }

    void SetVegetationInstances(
        TArray<FCubusVegetationInstance>&& InInstances
    )
    {
        VegetationInstances = MoveTemp(InInstances);
    }

    void ClearVegetationInstances()
    {
        VegetationInstances.Reset();
    }

    TConstArrayView<FCubusVegetationInstance>
    GetVegetationInstances() const
    {
        return MakeArrayView(VegetationInstances);
    }

private:
    FIntVector ChunkCoordinate = FIntVector::ZeroValue;
    FCubusGenerationSeeds GenerationSeeds;
    TArray<FCubusBlockVoxel> Voxels;
    TArray<FCubusVegetationInstance> VegetationInstances;

    mutable bool bOccupiedStateKnown = false;
    mutable bool bHasAnyOccupiedVoxel = false;

    void InvalidateOccupiedState()
    {
        bOccupiedStateKnown = false;
    }
};
