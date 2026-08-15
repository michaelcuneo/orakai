#pragma once

#include "CoreMinimal.h"

enum class ECubusDensityFace : uint8
{
    NegativeX = 0,
    PositiveX,
    NegativeY,
    PositiveY,
    NegativeZ,
    PositiveZ,
    Count
};

/**
 * Snapshot of the target density resolution on all six neighbouring faces.
 * A transition cell is owned by the coarser chunk whenever a face neighbour
 * has exactly twice its subdivision count.
 */
struct ORAKAI_API FCubusDensityTransitionFaces
{
    int32 NeighbourSubdivisions[6] = { 0, 0, 0, 0, 0, 0 };

    int32 Get(const ECubusDensityFace Face) const
    {
        return NeighbourSubdivisions[static_cast<int32>(Face)];
    }

    void Set(const ECubusDensityFace Face, const int32 Subdivisions)
    {
        NeighbourSubdivisions[static_cast<int32>(Face)] = Subdivisions;
    }

    bool HasFinerNeighbour(const ECubusDensityFace Face, const int32 SelfSubdivisions) const
    {
        return Get(Face) == SelfSubdivisions * 2;
    }

    uint32 GetSignature(const int32 SelfSubdivisions) const
    {
        uint32 Hash = 2166136261u;
        auto Mix = [&Hash](const uint32 Value)
        {
            Hash ^= Value;
            Hash *= 16777619u;
        };

        Mix(static_cast<uint32>(SelfSubdivisions));
        for (const int32 Value : NeighbourSubdivisions)
        {
            Mix(static_cast<uint32>(Value));
        }
        return Hash;
    }

    static FIntVector GetOffset(const ECubusDensityFace Face)
    {
        switch (Face)
        {
        case ECubusDensityFace::NegativeX: return FIntVector(-1, 0, 0);
        case ECubusDensityFace::PositiveX: return FIntVector(1, 0, 0);
        case ECubusDensityFace::NegativeY: return FIntVector(0, -1, 0);
        case ECubusDensityFace::PositiveY: return FIntVector(0, 1, 0);
        case ECubusDensityFace::NegativeZ: return FIntVector(0, 0, -1);
        case ECubusDensityFace::PositiveZ: return FIntVector(0, 0, 1);
        default: return FIntVector::ZeroValue;
        }
    }
};

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
