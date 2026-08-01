#include "CubusCore/Vegetation/CubusVegetationChunkFilter.h"

#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

bool FCubusVegetationChunkFilter::IsWithinCameraRadius(
    const ACubusVoxelVolumeActor* Chunk,
    const FVector& CameraLocation,
    const bool bUseCameraChunkCulling,
    const int32 HorizontalRadius,
    const int32 VerticalRadius
)
{
    if (!IsValid(Chunk))
    {
        return false;
    }

    if (!bUseCameraChunkCulling)
    {
        return true;
    }

    const float SafeVoxelSize =
        FMath::Max(
            1.0f,
            Chunk->GetVoxelSize()
        );

    const double ChunkWorldSize =
        static_cast<double>(Cubus::ChunkSize) *
        static_cast<double>(SafeVoxelSize);

    const double HalfChunkWorldSize =
        ChunkWorldSize * 0.5;

    const FIntVector CameraChunk(
        FMath::FloorToInt(
            (
                static_cast<double>(CameraLocation.X) +
                HalfChunkWorldSize
            ) /
            ChunkWorldSize
        ),
        FMath::FloorToInt(
            (
                static_cast<double>(CameraLocation.Y) +
                HalfChunkWorldSize
            ) /
            ChunkWorldSize
        ),
        FMath::FloorToInt(
            (
                static_cast<double>(CameraLocation.Z) +
                HalfChunkWorldSize
            ) /
            ChunkWorldSize
        )
    );

    const FIntVector ChunkCoordinate =
        Chunk->GetChunkCoordinate();

    return
        FMath::Abs(
            ChunkCoordinate.X -
            CameraChunk.X
        ) <= FMath::Max(0, HorizontalRadius) &&
        FMath::Abs(
            ChunkCoordinate.Y -
            CameraChunk.Y
        ) <= FMath::Max(0, HorizontalRadius) &&
        FMath::Abs(
            ChunkCoordinate.Z -
            CameraChunk.Z
        ) <= FMath::Max(0, VerticalRadius);
}