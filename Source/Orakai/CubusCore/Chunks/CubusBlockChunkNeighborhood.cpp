#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

const FCubusBlockVoxel*
FCubusBlockChunkNeighborhood::GetVoxel(
    const int32 X,
    const int32 Y,
    const int32 Z
) const
{
    if (Centre == nullptr)
    {
        return nullptr;
    }

    if (
        X >= 0 && X < Cubus::ChunkSize &&
        Y >= 0 && Y < Cubus::ChunkSize &&
        Z >= 0 && Z < Cubus::ChunkSize
    )
    {
        return Centre->GetVoxel(X, Y, Z);
    }

    if (
        X == Cubus::ChunkSize &&
        Y >= 0 && Y < Cubus::ChunkSize &&
        Z >= 0 && Z < Cubus::ChunkSize
    )
    {
        return PositiveX != nullptr
            ? PositiveX->GetVoxel(0, Y, Z)
            : nullptr;
    }

    if (
        X == -1 &&
        Y >= 0 && Y < Cubus::ChunkSize &&
        Z >= 0 && Z < Cubus::ChunkSize
    )
    {
        return NegativeX != nullptr
            ? NegativeX->GetVoxel(
                Cubus::ChunkSize - 1,
                Y,
                Z
            )
            : nullptr;
    }

    if (
        Y == Cubus::ChunkSize &&
        X >= 0 && X < Cubus::ChunkSize &&
        Z >= 0 && Z < Cubus::ChunkSize
    )
    {
        return PositiveY != nullptr
            ? PositiveY->GetVoxel(X, 0, Z)
            : nullptr;
    }

    if (
        Y == -1 &&
        X >= 0 && X < Cubus::ChunkSize &&
        Z >= 0 && Z < Cubus::ChunkSize
    )
    {
        return NegativeY != nullptr
            ? NegativeY->GetVoxel(
                X,
                Cubus::ChunkSize - 1,
                Z
            )
            : nullptr;
    }

    if (
        Z == Cubus::ChunkSize &&
        X >= 0 && X < Cubus::ChunkSize &&
        Y >= 0 && Y < Cubus::ChunkSize
    )
    {
        return PositiveZ != nullptr
            ? PositiveZ->GetVoxel(X, Y, 0)
            : nullptr;
    }

    if (
        Z == -1 &&
        X >= 0 && X < Cubus::ChunkSize &&
        Y >= 0 && Y < Cubus::ChunkSize
    )
    {
        return NegativeZ != nullptr
            ? NegativeZ->GetVoxel(
                X,
                Y,
                Cubus::ChunkSize - 1
            )
            : nullptr;
    }

    return nullptr;
}