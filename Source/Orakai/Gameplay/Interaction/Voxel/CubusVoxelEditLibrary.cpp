#include "Gameplay/Interaction/Voxel/CubusVoxelEditLibrary.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

namespace CubusVoxelEdit
{
    bool ResolveVoxelFromHit(
        const FHitResult& Hit,
        const bool bOutsideSurface,
        FIntVector& OutWorldVoxel,
        ACubusBlockWorldActor*& OutBlockWorld
    )
    {
        OutWorldVoxel = FIntVector::ZeroValue;
        OutBlockWorld = nullptr;

        if (!Hit.bBlockingHit)
        {
            return false;
        }

        ACubusVoxelVolumeActor* ChunkActor =
            Cast<ACubusVoxelVolumeActor>(Hit.GetActor());

        if (!IsValid(ChunkActor))
        {
            return false;
        }

        ACubusBlockWorldActor* BlockWorld =
            ChunkActor->GetOwningBlockWorld();

        if (!IsValid(BlockWorld))
        {
            BlockWorld =
                Cast<ACubusBlockWorldActor>(
                    ChunkActor->GetOwner()
                );
        }

        if (!IsValid(BlockWorld))
        {
            return false;
        }

        const float VoxelSize =
            FMath::Max(1.0f, ChunkActor->GetVoxelSize());

        const FVector SurfaceNormal =
            Hit.ImpactNormal.GetSafeNormal();

        if (SurfaceNormal.IsNearlyZero())
        {
            return false;
        }

        /*
         * Push the sample point slightly across the hit plane:
         *
         * removal  -> inside the struck voxel
         * placement -> outside into the adjacent voxel
         */
        const float SurfaceOffset =
            FMath::Max(0.5f, VoxelSize * 0.01f);

        const float Direction =
            bOutsideSurface ? 1.0f : -1.0f;

        const FVector SamplePoint =
            Hit.ImpactPoint +
            SurfaceNormal * SurfaceOffset * Direction;

        const double ChunkWorldSize =
            static_cast<double>(Cubus::ChunkSize) *
            static_cast<double>(VoxelSize);

        const double HalfChunkWorldSize =
            ChunkWorldSize * 0.5;

        const FVector RelativePoint =
            SamplePoint -
            ChunkActor->GetActorLocation();

        const FIntVector LocalCoordinate(
            FMath::FloorToInt(
                (RelativePoint.X + HalfChunkWorldSize) /
                VoxelSize
            ),
            FMath::FloorToInt(
                (RelativePoint.Y + HalfChunkWorldSize) /
                VoxelSize
            ),
            FMath::FloorToInt(
                (RelativePoint.Z + HalfChunkWorldSize) /
                VoxelSize
            )
        );

        /*
         * LocalCoordinate may intentionally be -1 or ChunkSize when the
         * selected voxel lies across a chunk boundary. Adding it to the
         * chunk's world-voxel origin naturally produces the correct world
         * coordinate, and EditVoxelAtWorldVoxel resolves the neighbouring
         * chunk.
         */
        OutWorldVoxel =
            ChunkActor->GetChunkCoordinate() * Cubus::ChunkSize +
            LocalCoordinate;

        OutBlockWorld = BlockWorld;
        return true;
    }
}

bool UCubusVoxelEditLibrary::ResolveHitVoxel(
    const FHitResult& Hit,
    FIntVector& OutWorldVoxel,
    ACubusBlockWorldActor*& OutBlockWorld
)
{
    return CubusVoxelEdit::ResolveVoxelFromHit(
        Hit,
        false,
        OutWorldVoxel,
        OutBlockWorld
    );
}

bool UCubusVoxelEditLibrary::ResolveAdjacentVoxel(
    const FHitResult& Hit,
    FIntVector& OutWorldVoxel,
    ACubusBlockWorldActor*& OutBlockWorld
)
{
    return CubusVoxelEdit::ResolveVoxelFromHit(
        Hit,
        true,
        OutWorldVoxel,
        OutBlockWorld
    );
}

bool UCubusVoxelEditLibrary::RemoveVoxelFromHit(
    const FHitResult& Hit
)
{
    FIntVector WorldVoxel;
    ACubusBlockWorldActor* BlockWorld = nullptr;

    if (
        !ResolveHitVoxel(
            Hit,
            WorldVoxel,
            BlockWorld
        )
    )
    {
        return false;
    }

    return BlockWorld->EditVoxelAtWorldVoxel(
        WorldVoxel,
        0,
        false
    );
}

bool UCubusVoxelEditLibrary::AddVoxelFromHit(
    const FHitResult& Hit,
    const int32 MaterialId,
    const bool bIsWater
)
{
    if (MaterialId <= 0)
    {
        return false;
    }

    FIntVector WorldVoxel;
    ACubusBlockWorldActor* BlockWorld = nullptr;

    if (
        !ResolveAdjacentVoxel(
            Hit,
            WorldVoxel,
            BlockWorld
        )
    )
    {
        return false;
    }

    return BlockWorld->EditVoxelAtWorldVoxel(
        WorldVoxel,
        MaterialId,
        bIsWater
    );
}