#pragma once

#include "CoreMinimal.h"

/**
 * Density-LOD scale rules shared by streaming, chunks and meshing.
 *
 * Fine density subdivision is temporarily disabled as a runtime default.
 * The current adaptive path rebuilds chunks synchronously, cooks collision,
 * and multiplies Marching Cubes work by the cube of the subdivision count.
 * Keep the canonical 100 cm lattice authoritative until refinement is moved
 * to an asynchronous, budgeted near-field system.
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
