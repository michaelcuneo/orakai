#include "Gameplay/Interaction/Voxel/CubusVoxelEditLibrary.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"

int32 UCubusVoxelEditLibrary::SmoothDensityFromHit(
    const FHitResult& Hit,
    const int32 BrushRadius,
    const float Strength
)
{
    FIntVector WorldSample;
    ACubusBlockWorldActor* BlockWorld = nullptr;
    if (!ResolveHitVoxel(Hit, WorldSample, BlockWorld))
    {
        return 0;
    }

    return BlockWorld->SmoothDensityEditsAtWorldSample(
        WorldSample,
        FMath::Max(0, BrushRadius),
        FMath::Clamp(Strength, 0.0f, 1.0f)
    );
}

int32 UCubusVoxelEditLibrary::LevelDensityFromHit(
    const FHitResult& Hit,
    const int32 BrushRadius,
    const float Strength,
    const int32 MaterialId
)
{
    FIntVector WorldSample;
    ACubusBlockWorldActor* BlockWorld = nullptr;
    if (!ResolveHitVoxel(Hit, WorldSample, BlockWorld))
    {
        return 0;
    }

    return BlockWorld->LevelDensityEditsAtWorldSample(
        WorldSample,
        FMath::Max(0, BrushRadius),
        FMath::Clamp(Strength, 0.0f, 1.0f),
        FMath::Max(1, MaterialId)
    );
}

int32 UCubusVoxelEditLibrary::RestoreDensityFromHit(
    const FHitResult& Hit,
    const int32 BrushRadius,
    const float Strength
)
{
    FIntVector WorldSample;
    ACubusBlockWorldActor* BlockWorld = nullptr;
    if (!ResolveHitVoxel(Hit, WorldSample, BlockWorld))
    {
        return 0;
    }

    return BlockWorld->RestoreDensityEditsAtWorldSample(
        WorldSample,
        FMath::Max(0, BrushRadius),
        FMath::Clamp(Strength, 0.0f, 1.0f)
    );
}
