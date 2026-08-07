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

    bool MatchesDrop(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        const int32 X,
        const int32 Y,
        const int32 Z,
        const FIntVector& Uphill,
        const FIntVector& Downhill
    )
    {
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
            IsRenderableSolid(UphillVoxel, MaterialRegistry) &&
            IsEmptyForTerrain(DownhillVoxel, MaterialRegistry) &&
            IsRenderableSolid(DownhillSupport, MaterialRegistry);
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

    const bool bHighNegativeX = CubusBlockSurfaceShape::MatchesDrop(
        Neighborhood,
        MaterialRegistry,
        X,
        Y,
        Z,
        FIntVector(-1, 0, 0),
        FIntVector(1, 0, 0)
    );

    const bool bHighPositiveX = CubusBlockSurfaceShape::MatchesDrop(
        Neighborhood,
        MaterialRegistry,
        X,
        Y,
        Z,
        FIntVector(1, 0, 0),
        FIntVector(-1, 0, 0)
    );

    const bool bHighNegativeY = CubusBlockSurfaceShape::MatchesDrop(
        Neighborhood,
        MaterialRegistry,
        X,
        Y,
        Z,
        FIntVector(0, -1, 0),
        FIntVector(0, 1, 0)
    );

    const bool bHighPositiveY = CubusBlockSurfaceShape::MatchesDrop(
        Neighborhood,
        MaterialRegistry,
        X,
        Y,
        Z,
        FIntVector(0, 1, 0),
        FIntVector(0, -1, 0)
    );

    const int32 XMatches =
        static_cast<int32>(bHighNegativeX) +
        static_cast<int32>(bHighPositiveX);

    const int32 YMatches =
        static_cast<int32>(bHighNegativeY) +
        static_cast<int32>(bHighPositiveY);

    // Opposing matches are ambiguous ridges or channels. Keep a cube until
    // dedicated ridge/valley templates are added.
    if (XMatches > 1 || YMatches > 1)
    {
        return Result;
    }

    if (XMatches == 1 && YMatches == 1)
    {
        // Two downhill directions identify the open diagonal corner. Lower
        // that one corner while retaining the other three at full block
        // height, producing an inverse cut rather than a protruding point.
        if (bHighNegativeX && bHighNegativeY)
        {
            Result.Shape =
                ECubusBlockSurfaceShape::CornerLowPositiveXPositiveY;
        }
        else if (bHighNegativeX && bHighPositiveY)
        {
            Result.Shape =
                ECubusBlockSurfaceShape::CornerLowPositiveXNegativeY;
        }
        else if (bHighPositiveX && bHighNegativeY)
        {
            Result.Shape =
                ECubusBlockSurfaceShape::CornerLowNegativeXPositiveY;
        }
        else
        {
            Result.Shape =
                ECubusBlockSurfaceShape::CornerLowNegativeXNegativeY;
        }

        return Result;
    }

    if (bHighNegativeX)
    {
        Result.Shape = ECubusBlockSurfaceShape::RampPositiveX;
    }
    else if (bHighPositiveX)
    {
        Result.Shape = ECubusBlockSurfaceShape::RampNegativeX;
    }
    else if (bHighNegativeY)
    {
        Result.Shape = ECubusBlockSurfaceShape::RampPositiveY;
    }
    else if (bHighPositiveY)
    {
        Result.Shape = ECubusBlockSurfaceShape::RampNegativeY;
    }

    return Result;
}
