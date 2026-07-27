#include "CubusCore/Blueprint/CubusVoxelEditLibrary.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

#include "Engine/World.h"
#include "EngineUtils.h"

namespace CubusVoxelEditLibrary
{
    bool ResolveContext(
        const FHitResult& Hit,
        ACubusVoxelVolumeActor*& OutChunk,
        ACubusBlockWorldActor*& OutBlockWorld
    )
    {
        OutChunk = Cast<ACubusVoxelVolumeActor>(Hit.GetActor());
        OutBlockWorld = nullptr;

        if (!IsValid(OutChunk))
        {
            return false;
        }

        OutBlockWorld = Cast<ACubusBlockWorldActor>(OutChunk->GetOwner());

        if (IsValid(OutBlockWorld))
        {
            return true;
        }

        UWorld* World = OutChunk->GetWorld();

        if (!IsValid(World))
        {
            return false;
        }

        for (TActorIterator<ACubusBlockWorldActor> Iterator(World); Iterator; ++Iterator)
        {
            ACubusBlockWorldActor* Candidate = *Iterator;

            if (
                IsValid(Candidate) &&
                Candidate->FindChunk(OutChunk->GetChunkCoordinate()) == OutChunk
            )
            {
                OutBlockWorld = Candidate;
                return true;
            }
        }

        return false;
    }

    FIntVector SampleWorldVoxel(
        const ACubusVoxelVolumeActor& Chunk,
        const FVector& SampleLocation
    )
    {
        const double VoxelSize =
            static_cast<double>(FMath::Max(1.0f, Chunk.GetVoxelSize()));

        const double HalfChunkWorldSize =
            static_cast<double>(Cubus::ChunkSize) * VoxelSize * 0.5;

        const FVector RelativeLocation =
            SampleLocation - Chunk.GetActorLocation();

        const FIntVector LocalCoordinate(
            FMath::FloorToInt(
                (static_cast<double>(RelativeLocation.X) + HalfChunkWorldSize) /
                VoxelSize
            ),
            FMath::FloorToInt(
                (static_cast<double>(RelativeLocation.Y) + HalfChunkWorldSize) /
                VoxelSize
            ),
            FMath::FloorToInt(
                (static_cast<double>(RelativeLocation.Z) + HalfChunkWorldSize) /
                VoxelSize
            )
        );

        return
            Chunk.GetChunkCoordinate() * Cubus::ChunkSize +
            LocalCoordinate;
    }

    bool ResolveVoxel(
        const FHitResult& Hit,
        const bool bAdjacent,
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

        ACubusVoxelVolumeActor* Chunk = nullptr;

        if (!ResolveContext(Hit, Chunk, OutBlockWorld))
        {
            return false;
        }

        const float VoxelSize = FMath::Max(1.0f, Chunk->GetVoxelSize());
        const float SurfaceOffset = FMath::Max(0.5f, VoxelSize * 0.05f);
        const FVector Normal = Hit.ImpactNormal.GetSafeNormal();

        if (Normal.IsNearlyZero())
        {
            return false;
        }

        const FVector SampleLocation =
            Hit.ImpactPoint +
            Normal * (bAdjacent ? SurfaceOffset : -SurfaceOffset);

        OutWorldVoxel = SampleWorldVoxel(*Chunk, SampleLocation);
        return true;
    }
}

bool UCubusVoxelEditLibrary::ResolveHitVoxel(
    const FHitResult& Hit,
    FIntVector& OutWorldVoxel,
    ACubusBlockWorldActor*& OutBlockWorld
)
{
    return CubusVoxelEditLibrary::ResolveVoxel(
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
    return CubusVoxelEditLibrary::ResolveVoxel(
        Hit,
        true,
        OutWorldVoxel,
        OutBlockWorld
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

    FIntVector WorldVoxel = FIntVector::ZeroValue;
    ACubusBlockWorldActor* BlockWorld = nullptr;

    if (!ResolveAdjacentVoxel(Hit, WorldVoxel, BlockWorld))
    {
        return false;
    }

    return BlockWorld->EditVoxelAtWorldVoxel(
        WorldVoxel,
        MaterialId,
        bIsWater
    );
}

bool UCubusVoxelEditLibrary::RemoveVoxelFromHit(
    const FHitResult& Hit
)
{
    FIntVector WorldVoxel = FIntVector::ZeroValue;
    ACubusBlockWorldActor* BlockWorld = nullptr;

    if (!ResolveHitVoxel(Hit, WorldVoxel, BlockWorld))
    {
        return false;
    }

    return BlockWorld->EditVoxelAtWorldVoxel(
        WorldVoxel,
        0,
        false
    );
}
