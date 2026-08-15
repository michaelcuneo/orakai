#pragma once

#include "CoreMinimal.h"

/**
 * Density-LOD scale rules shared by streaming, chunks and meshing.
 *
 * The procedural scalar field remains canonical and resolution-independent.
 * Subdivision changes only how densely a chunk samples that field for its
 * Marching Cubes mesh; generated terrain is never persisted at the finer
 * resolution. Player density edits remain a separate sparse field and are
 * evaluated continuously by FCubusDensityEditField.
 *
 * Runtime refinement is deliberately capped at 4 subdivisions per canonical
 * voxel. With an 80 cm canonical voxel this gives 20 cm near-field samples,
 * 40 cm transition samples, and 80 cm ordinary samples. BuildAdaptiveChunk()
 * rejects coarse cells that cannot contain a surface before marching the fine
 * lattice, avoiding the old cubic full-volume refinement cost.
 */
class ORAKAI_API FCubusDensityLod
{
public:
    static constexpr int32 MaximumRuntimeSubdivisions = 4;

    static int32 NormalizeSubdivisions(
        const int32 RequestedSubdivisions
    )
    {
        const int32 SafeRequested = FMath::Clamp(
            RequestedSubdivisions,
            1,
            MaximumRuntimeSubdivisions
        );

        // Keep neighbouring fine lattices on power-of-two sample intervals.
        // This gives stable 1x / 2x / 4x tiers and avoids arbitrary fractional
        // lattice relationships between streamed chunks.
        if (SafeRequested <= 1)
        {
            return 1;
        }

        if (SafeRequested <= 2)
        {
            return 2;
        }

        return 4;
    }

    static int32 ResolveSubdivisionsForSpacing(
        const float CanonicalVoxelSize,
        const float TargetSampleSpacing
    )
    {
        const float SafeVoxelSize = FMath::Max(1.0f, CanonicalVoxelSize);
        const float SafeTargetSpacing = FMath::Clamp(
            TargetSampleSpacing,
            1.0f,
            SafeVoxelSize
        );

        const int32 RequiredSubdivisions = FMath::CeilToInt(
            SafeVoxelSize / SafeTargetSpacing
        );

        return NormalizeSubdivisions(RequiredSubdivisions);
    }

    static float GetSampleSpacing(
        const float CanonicalVoxelSize,
        const int32 SubdivisionsPerVoxel
    )
    {
        return FMath::Max(1.0f, CanonicalVoxelSize) /
            static_cast<float>(NormalizeSubdivisions(SubdivisionsPerVoxel));
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
