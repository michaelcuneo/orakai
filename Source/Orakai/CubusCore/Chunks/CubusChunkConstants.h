#pragma once

#include "CoreMinimal.h"

namespace Cubus
{
    inline constexpr int32 ChunkSize = 32;
    inline constexpr int32 ChunkArea =
        ChunkSize * ChunkSize;

    inline constexpr int32 ChunkVolume =
        ChunkArea * ChunkSize;

    FORCEINLINE bool IsValidLocalCoordinate(
        const int32 X,
        const int32 Y,
        const int32 Z
    )
    {
        return
            X >= 0 && X < ChunkSize &&
            Y >= 0 && Y < ChunkSize &&
            Z >= 0 && Z < ChunkSize;
    }

    FORCEINLINE bool IsValidLocalCoordinate(
        const FIntVector& Coordinate
    )
    {
        return IsValidLocalCoordinate(
            Coordinate.X,
            Coordinate.Y,
            Coordinate.Z
        );
    }

    FORCEINLINE int32 FlattenLocalCoordinate(
        const int32 X,
        const int32 Y,
        const int32 Z
    )
    {
        check(IsValidLocalCoordinate(X, Y, Z));

        return
            X +
            ChunkSize *
            (
                Y +
                ChunkSize * Z
            );
    }

    FORCEINLINE int32 FlattenLocalCoordinate(
        const FIntVector& Coordinate
    )
    {
        return FlattenLocalCoordinate(
            Coordinate.X,
            Coordinate.Y,
            Coordinate.Z
        );
    }

    FORCEINLINE FIntVector UnflattenLocalIndex(
        const int32 Index
    )
    {
        check(Index >= 0 && Index < ChunkVolume);

        const int32 Z =
            Index / ChunkArea;

        const int32 Remaining =
            Index - Z * ChunkArea;

        const int32 Y =
            Remaining / ChunkSize;

        const int32 X =
            Remaining - Y * ChunkSize;

        return FIntVector(X, Y, Z);
    }

    FORCEINLINE FIntVector LocalToWorldVoxel(
        const FIntVector& ChunkCoordinate,
        const FIntVector& LocalCoordinate
    )
    {
        return
            ChunkCoordinate * ChunkSize +
            LocalCoordinate;
    }
}