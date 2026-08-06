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

        OutWorldVoxel =
            ChunkActor->GetChunkCoordinate() * Cubus::ChunkSize +
            LocalCoordinate;

        OutBlockWorld = BlockWorld;
        return true;
    }

    float SmoothBrushWeight(const int32 Radius, const int32 MaximumRadius)
    {
        if (MaximumRadius <= 0)
        {
            return Radius <= 0 ? 1.0f : 0.0f;
        }

        const float NormalizedRadius =
            static_cast<float>(Radius) /
            static_cast<float>(MaximumRadius + 1);

        const float T = FMath::Clamp(1.0f - NormalizedRadius, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    int32 ApplySmoothDensityBrush(
        ACubusBlockWorldActor& BlockWorld,
        const FIntVector& CentreWorldSample,
        const int32 BrushRadius,
        const float DensityDelta,
        const int32 MaterialId
    )
    {
        const int32 SafeRadius = FMath::Max(0, BrushRadius);

        if (SafeRadius == 0)
        {
            return BlockWorld.EditDensitySphereAtWorldSample(
                CentreWorldSample,
                0,
                DensityDelta,
                MaterialId
            );
        }

        int32 ChangedSampleCount = 0;
        float OuterWeight = 0.0f;

        for (int32 Radius = SafeRadius; Radius >= 0; --Radius)
        {
            const float CurrentWeight =
                SmoothBrushWeight(Radius, SafeRadius);

            const float IncrementalWeight =
                FMath::Max(0.0f, CurrentWeight - OuterWeight);

            OuterWeight = CurrentWeight;

            if (IncrementalWeight <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            ChangedSampleCount +=
                BlockWorld.EditDensitySphereAtWorldSample(
                    CentreWorldSample,
                    Radius,
                    DensityDelta * IncrementalWeight,
                    MaterialId
                );
        }

        return ChangedSampleCount;
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

int32 UCubusVoxelEditLibrary::RemoveBlockBrushFromHit(
    const FHitResult& Hit,
    const int32 BrushRadius
)
{
    FIntVector WorldVoxel;
    ACubusBlockWorldActor* BlockWorld = nullptr;

    if (!ResolveHitVoxel(Hit, WorldVoxel, BlockWorld))
    {
        return 0;
    }

    return BlockWorld->EditBlockSphereAtWorldVoxel(
        WorldVoxel,
        BrushRadius,
        0,
        false
    );
}

int32 UCubusVoxelEditLibrary::AddBlockBrushFromHit(
    const FHitResult& Hit,
    const int32 BrushRadius,
    const int32 MaterialId,
    const bool bIsWater
)
{
    if (MaterialId <= 0)
    {
        return 0;
    }

    FIntVector WorldVoxel;
    ACubusBlockWorldActor* BlockWorld = nullptr;

    if (!ResolveAdjacentVoxel(Hit, WorldVoxel, BlockWorld))
    {
        return 0;
    }

    return BlockWorld->EditBlockSphereAtWorldVoxel(
        WorldVoxel,
        BrushRadius,
        MaterialId,
        bIsWater
    );
}

int32 UCubusVoxelEditLibrary::RemoveDensityFromHit(
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

    return CubusVoxelEdit::ApplySmoothDensityBrush(
        *BlockWorld,
        WorldSample,
        BrushRadius,
        -FMath::Abs(Strength),
        0
    );
}

int32 UCubusVoxelEditLibrary::AddDensityFromHit(
    const FHitResult& Hit,
    const int32 BrushRadius,
    const float Strength,
    const int32 MaterialId
)
{
    if (MaterialId <= 0)
    {
        return 0;
    }

    FIntVector WorldSample;
    ACubusBlockWorldActor* BlockWorld = nullptr;

    // Density addition and removal must use the same hit-centred sample.
    // Offsetting addition into the adjacent sample makes the operations
    // asymmetric and can leave a one-sample residual shell or opening.
    if (!ResolveHitVoxel(Hit, WorldSample, BlockWorld))
    {
        return 0;
    }

    return CubusVoxelEdit::ApplySmoothDensityBrush(
        *BlockWorld,
        WorldSample,
        BrushRadius,
        FMath::Abs(Strength),
        MaterialId
    );
}
