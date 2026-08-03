#pragma once

#include "CoreMinimal.h"

/** Pure density-LOD scale rules shared by streaming, chunks and meshing. */
class ORAKAI_API FCubusDensityLod
{
public:
    static int32 NormalizeSubdivisions(
        const int32 RequestedSubdivisions
    )
    {
        const int32 Supported[] = { 1, 2, 4, 10 };
        int32 Best = 1;
        int32 BestError = MAX_int32;

        for (const int32 Candidate : Supported)
        {
            const int32 Error = FMath::Abs(
                Candidate - RequestedSubdivisions
            );

            if (Error < BestError)
            {
                BestError = Error;
                Best = Candidate;
            }
        }

        return Best;
    }

    static int32 ResolveSubdivisionsForSpacing(
        const float CanonicalVoxelSize,
        const float TargetSampleSpacing
    )
    {
        const float SafeVoxelSize =
            FMath::Max(1.0f, CanonicalVoxelSize);
        const float SafeTargetSpacing = FMath::Clamp(
            TargetSampleSpacing,
            1.0f,
            SafeVoxelSize
        );
        const int32 Supported[] = { 1, 2, 4, 10 };

        int32 Best = 1;
        float BestError = MAX_flt;

        for (const int32 Candidate : Supported)
        {
            const float CandidateSpacing =
                SafeVoxelSize /
                static_cast<float>(Candidate);
            const float Error = FMath::Abs(
                CandidateSpacing - SafeTargetSpacing
            );

            if (Error < BestError)
            {
                BestError = Error;
                Best = Candidate;
            }
        }

        return Best;
    }

    static float GetSampleSpacing(
        const float CanonicalVoxelSize,
        const int32 SubdivisionsPerVoxel
    )
    {
        return
            FMath::Max(1.0f, CanonicalVoxelSize) /
            static_cast<float>(
                NormalizeSubdivisions(SubdivisionsPerVoxel)
            );
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
