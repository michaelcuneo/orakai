#include "CubusCore/Meshing/CubusBlockSurfaceShape.h"

#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"

namespace CubusBlockSurfaceShape
{
    bool IsRenderableSolid(
        const FCubusBlockVoxel* Voxel,
        const UCubusMaterialRegistry* MaterialRegistry
    )
    {
        if (Voxel == nullptr || !Voxel->IsSolid())
        {
            return false;
        }

        return
            MaterialRegistry == nullptr ||
            MaterialRegistry->IsRenderableSolid(Voxel->MaterialId);
    }

    bool IsEmptyForTerrain(
        const FCubusBlockVoxel* Voxel,
        const UCubusMaterialRegistry* MaterialRegistry
    )
    {
        return !IsRenderableSolid(Voxel, MaterialRegistry);
    }
}

FCubusBlockSurfaceClassification FCubusBlockSurfaceClassifier::Classify(
    const FCubusBlockChunkNeighborhood& Neighborhood,
    const UCubusMaterialRegistry* MaterialRegistry,
    const int32 X,
    const int32 Y,
    const int32 Z
)
{
    FCubusBlockSurfaceClassification Result;

    const FCubusBlockVoxel* Current = Neighborhood.GetVoxel(X, Y, Z);
    const FCubusBlockVoxel* Above = Neighborhood.GetVoxel(X, Y, Z + 1);
    const FCubusBlockVoxel* Below = Neighborhood.GetVoxel(X, Y, Z - 1);

    if (
        !CubusBlockSurfaceShape::IsRenderableSolid(Current, MaterialRegistry) ||
        !CubusBlockSurfaceShape::IsEmptyForTerrain(Above, MaterialRegistry) ||
        !CubusBlockSurfaceShape::IsRenderableSolid(Below, MaterialRegistry)
    )
    {
        return Result;
    }

    struct FRampCandidate
    {
        ECubusBlockSurfaceShape Shape;
        FIntVector Uphill;
        FIntVector Downhill;
    };

    static const FRampCandidate Candidates[] =
    {
        {
            ECubusBlockSurfaceShape::RampPositiveX,
            FIntVector(-1, 0, 0),
            FIntVector(1, 0, 0)
        },
        {
            ECubusBlockSurfaceShape::RampNegativeX,
            FIntVector(1, 0, 0),
            FIntVector(-1, 0, 0)
        },
        {
            ECubusBlockSurfaceShape::RampPositiveY,
            FIntVector(0, -1, 0),
            FIntVector(0, 1, 0)
        },
        {
            ECubusBlockSurfaceShape::RampNegativeY,
            FIntVector(0, 1, 0),
            FIntVector(0, -1, 0)
        }
    };

    int32 MatchCount = 0;
    ECubusBlockSurfaceShape MatchedShape = ECubusBlockSurfaceShape::Cube;

    for (const FRampCandidate& Candidate : Candidates)
    {
        const FCubusBlockVoxel* UphillVoxel = Neighborhood.GetVoxel(
            X + Candidate.Uphill.X,
            Y + Candidate.Uphill.Y,
            Z
        );

        const FCubusBlockVoxel* DownhillVoxel = Neighborhood.GetVoxel(
            X + Candidate.Downhill.X,
            Y + Candidate.Downhill.Y,
            Z
        );

        const FCubusBlockVoxel* DownhillSupport = Neighborhood.GetVoxel(
            X + Candidate.Downhill.X,
            Y + Candidate.Downhill.Y,
            Z - 1
        );

        if (
            CubusBlockSurfaceShape::IsRenderableSolid(UphillVoxel, MaterialRegistry) &&
            CubusBlockSurfaceShape::IsEmptyForTerrain(DownhillVoxel, MaterialRegistry) &&
            CubusBlockSurfaceShape::IsRenderableSolid(DownhillSupport, MaterialRegistry)
        )
        {
            ++MatchCount;
            MatchedShape = Candidate.Shape;
        }
    }

    // Junctions and diagonal transitions need dedicated corner templates.
    // Until those exist, preserving the cube is safer than guessing.
    if (MatchCount == 1)
    {
        Result.Shape = MatchedShape;
    }

    return Result;
}
