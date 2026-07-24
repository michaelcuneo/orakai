#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

void ACubusPCGVoxelVolumeActor::GenerateTestShapeData()
{
    const FIntVector Coordinate = GetChunkCoordinate();

    if (TryLoadCachedChunk())
    {
        // Vegetation is deterministic derived data and is deliberately not
        // serialized in the voxel cache.
        RegenerateVegetationData();

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus chunk cache used before generation (%d, %d, %d)"),
            Coordinate.X,
            Coordinate.Y,
            Coordinate.Z
        );
        return;
    }

    Super::GenerateTestShapeData();

    if (SaveCachedChunk())
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus chunk cache miss (%d, %d, %d): generated data saved"),
            Coordinate.X,
            Coordinate.Y,
            Coordinate.Z
        );
    }
}
