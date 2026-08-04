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

    bool MatchesRampPattern(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        const int32 X,
        const int32 Y,
        const int32 Z,
        const FIntVector& Uphill,
        const FIntVector& Downhill
    )
    {
        const FCubusBlockVoxel* Current = Neighborhood.GetVoxel(X, Y, Z);
        const FCubusBlockVoxel* Above = Neighborhood.GetVoxel(X, Y, Z + 1);
        const FCubusBlockVoxel* Below = Neighborhood.GetVoxel(X, Y, Z - 1);
        const FCubusBlockVoxel* UphillVoxel = Neighborhood.GetVoxel(
            X + Uphill.X,
            Y + Uphill.Y,
            Z
        );
        const FCubusBlockVoxel* DownhillVoxel = Neighborhood.GetVoxel(
            X + Downhill.X,
            Y + Downhill.Y,
            Z
        );
        const FCubusBlockVoxel* DownhillSupport = Neighborhood.GetVoxel(
            X + Downhill.X,
            Y + Downhill.Y,
            Z - 1
        );

        return
            IsRenderableSolid(Current, MaterialRegistry) &&
            IsEmptyForTerrain(Above, MaterialRegistry) &&
            IsRenderableSolid(Below, MaterialRegistry) &&
            IsRenderableSolid(UphillVoxel, MaterialRegistry) &&
            IsEmptyForTerrain(DownhillVoxel, MaterialRegistry) &&
            IsRenderableSolid(DownhillSupport, MaterialRegistry);
    }

    bool HasCompatibleSideNeighbours(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        const int32 X,
        const int32 Y,
        const int32 Z,
        const FIntVector& Uphill,
        const FIntVector& Downhill
    )
    {
        const FIntVector SideAxis =
            Uphill.X != 0
                ? FIntVector(0, 1, 0)
                : FIntVector(1, 0, 0);

        const FIntVector SideOffsets[] =
        {
            SideAxis,
            -SideAxis
        };

        for (const FIntVector& SideOffset : SideOffsets)
        {
            const int32 SideX = X + SideOffset.X;
            const int32 SideY = Y + SideOffset.Y;
            const int32 SideZ = Z;

            const FCubusBlockVoxel* SideVoxel =
                Neighborhood.GetVoxel(SideX, SideY, SideZ);

            if (!IsRenderableSolid(SideVoxel, MaterialRegistry))
            {
                continue;
            }

            // A solid side neighbour causes the ramp mesher to cull the shared
            // side triangle. That is safe only when the neighbour resolves to
            // the same ramp profile. Cubes and differently oriented ramps
            // leave the unused triangular half visibly open.
            if (
                !MatchesRampPattern(
                    Neighborhood,
                    MaterialRegistry,
                    SideX,
                    SideY,
                    SideZ,
                    Uphill,
                    Downhill
                )
            )
            {
                return false;
            }
        }

        return true;
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
        if (
            CubusBlockSurfaceShape::MatchesRampPattern(
                Neighborhood,
                MaterialRegistry,
                X,
                Y,
                Z,
                Candidate.Uphill,
                Candidate.Downhill
            ) &&
            CubusBlockSurfaceShape::HasCompatibleSideNeighbours(
                Neighborhood,
                MaterialRegistry,
                X,
                Y,
                Z,
                Candidate.Uphill,
                Candidate.Downhill
            )
        )
        {
            ++MatchCount;
            MatchedShape = Candidate.Shape;
        }
    }

    // Junctions, incompatible side profiles and diagonal transitions need
    // dedicated corner templates. Preserve the cube until those templates
    // exist so the generated surface always remains closed.
    if (MatchCount == 1)
    {
        Result.Shape = MatchedShape;
    }

    return Result;
}
