#pragma once

#include "CoreMinimal.h"

/**
 * Density-LOD scale rules shared by streaming, chunks and meshing.
 *
 * Adaptive subdivision is temporarily disabled because the current fine-grid
 * path produces disconnected horizontal isosurfaces. Canonical one-sample-per-
 * voxel meshing is the last known-good runtime path and remains authoritative
 * until continuous fine sampling and transition handling are rebuilt safely.
 */
class ORAKAI_API FCubusDensityLod
{
public:
    static int32 NormalizeSubdivisions(
        const int32 RequestedSubdivisions
    )
    {
        (void)RequestedSubdivisions;
        return 1;
    }

    static int32 ResolveSubdivisionsForSpacing(
        const float CanonicalVoxelSize,
        const float TargetSampleSpacing
    )
    {
        (void)CanonicalVoxelSize;
        (void)TargetSampleSpacing;
        return 1;
    }

    static float GetSampleSpacing(
        const float CanonicalVoxelSize,
        const int32 SubdivisionsPerVoxel
    )
    {
        (void)SubdivisionsPerVoxel;
        return FMath::Max(1.0f, CanonicalVoxelSize);
    }

    static int32 ChunkDistance(
        const FIntVector& A,
        const FIntVector& B
    )
    {
        const FIntVector Delta = A - B;
        return FMath::Max3(
            FMath::Abs(Delta.X),
            FMath::Abs(Delta.Y),
            FMath::Abs(Delta.Z)
        );
    }
};
