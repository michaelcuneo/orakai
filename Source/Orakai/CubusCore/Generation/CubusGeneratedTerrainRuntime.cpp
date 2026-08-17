#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"

FRWLock FCubusGeneratedTerrainRuntime::StateLock;
FCubusGeneratedTerrainRuntime::FState FCubusGeneratedTerrainRuntime::State;

void FCubusGeneratedTerrainRuntime::Configure(
    const int32 WorldSeed,
    const FCubusTerrainRasterSettings& RasterSettings
)
{
    FWriteScopeLock Lock(StateLock);
    State = FState();
    State.WorldSeed = WorldSeed;
    State.RasterSettings = RasterSettings;

    const FCubusGenerationSeeds Seeds = FCubusGenerationSeeds::FromWorldSeed(WorldSeed);
    State.TerrainDomainOffsetVoxels = FIntPoint(
        (FCubusGenerationSeeds::DomainOffsetX(Seeds.Terrain) / Cubus::ChunkSize) * Cubus::ChunkSize,
        (FCubusGenerationSeeds::DomainOffsetY(Seeds.Terrain) / Cubus::ChunkSize) * Cubus::ChunkSize
    );
    State.bActive = true;
}

void FCubusGeneratedTerrainRuntime::StoreTile(const FCubusTerrainRasterTile& Tile)
{
    if (!Tile.IsValid())
    {
        return;
    }
    FWriteScopeLock Lock(StateLock);
    State.Tiles.Add(Tile.GetTileCoordinate(), Tile);
}

void FCubusGeneratedTerrainRuntime::StoreTileIfActive(const FCubusTerrainRasterTile& Tile)
{
    if (!Tile.IsValid())
    {
        return;
    }
    FWriteScopeLock Lock(StateLock);
    if (!State.bActive)
    {
        return;
    }
    State.Tiles.Add(Tile.GetTileCoordinate(), Tile);
}

void FCubusGeneratedTerrainRuntime::StorePreviewSnapshot(
    const int32 Resolution,
    const FBox2D& BoundsMeters,
    const TArray<FColor>& Pixels
)
{
    if (Resolution <= 0 || Pixels.Num() != Resolution * Resolution || !BoundsMeters.bIsValid)
    {
        return;
    }
    FWriteScopeLock Lock(StateLock);
    if (!State.bActive)
    {
        return;
    }
    State.PreviewResolution = Resolution;
    State.PreviewBoundsMeters = BoundsMeters;
    State.PreviewPixels = Pixels;
}

bool FCubusGeneratedTerrainRuntime::GetPreviewSnapshot(
    int32& OutResolution,
    FBox2D& OutBoundsMeters,
    TArray<FColor>& OutPixels
)
{
    FReadScopeLock Lock(StateLock);
    if (!State.bActive || State.PreviewResolution <= 0 || State.PreviewPixels.IsEmpty())
    {
        return false;
    }
    OutResolution = State.PreviewResolution;
    OutBoundsMeters = State.PreviewBoundsMeters;
    OutPixels = State.PreviewPixels;
    return true;
}

bool FCubusGeneratedTerrainRuntime::GetPreviewBoundsMeters(FBox2D& OutBoundsMeters)
{
    FReadScopeLock Lock(StateLock);
    if (!State.bActive || !State.PreviewBoundsMeters.bIsValid)
    {
        return false;
    }
    OutBoundsMeters = State.PreviewBoundsMeters;
    return true;
}

bool FCubusGeneratedTerrainRuntime::TrySampleHeightMeters(
    const FVector2D& WorldMeters,
    float& OutHeightMeters)
{
    FReadScopeLock Lock(StateLock);
    if (!State.bActive)
    {
        return false;
    }
    return TrySampleHeightMetersLocked(WorldMeters, OutHeightMeters);
}

void FCubusGeneratedTerrainRuntime::SetProposedSpawnFromPreviewUV(const FVector2D& PreviewUV)
{
    FWriteScopeLock Lock(StateLock);
    if (!State.bActive || !State.PreviewBoundsMeters.bIsValid)
    {
        return;
    }

    const double U = FMath::Clamp(PreviewUV.X, 0.0, 1.0);
    const double V = FMath::Clamp(PreviewUV.Y, 0.0, 1.0);
    const FVector2D Size = State.PreviewBoundsMeters.GetSize();
    State.ProposedSpawnWorldMeters = FVector2D(
        State.PreviewBoundsMeters.Min.X + U * Size.X,
        State.PreviewBoundsMeters.Min.Y + (1.0 - V) * Size.Y
    );
    State.bHasProposedSpawn = true;
    State.bHasConfirmedSpawn = false;
}

bool FCubusGeneratedTerrainRuntime::GetProposedSpawnWorldMeters(FVector2D& OutWorldMeters)
{
    FReadScopeLock Lock(StateLock);
    if (!State.bActive || !State.bHasProposedSpawn)
    {
        return false;
    }
    OutWorldMeters = State.ProposedSpawnWorldMeters;
    return true;
}

bool FCubusGeneratedTerrainRuntime::ConfirmProposedSpawn()
{
    FWriteScopeLock Lock(StateLock);
    if (!State.bActive || !State.bHasProposedSpawn)
    {
        return false;
    }

    float HeightMeters = 0.0f;
    if (!TrySampleHeightMetersLocked(State.ProposedSpawnWorldMeters, HeightMeters))
    {
        return false;
    }
    State.ConfirmedSpawnWorldMeters = State.ProposedSpawnWorldMeters;
    State.bHasConfirmedSpawn = true;
    return true;
}

bool FCubusGeneratedTerrainRuntime::HasConfirmedSpawn()
{
    FReadScopeLock Lock(StateLock);
    return State.bActive && State.bHasConfirmedSpawn;
}

bool FCubusGeneratedTerrainRuntime::GetConfirmedSpawnWorldMeters(FVector2D& OutWorldMeters)
{
    FReadScopeLock Lock(StateLock);
    if (!State.bActive || !State.bHasConfirmedSpawn)
    {
        return false;
    }
    OutWorldMeters = State.ConfirmedSpawnWorldMeters;
    return true;
}

bool FCubusGeneratedTerrainRuntime::TryGetConfirmedSpawnSurfaceHeightMeters(float& OutHeightMeters)
{
    FReadScopeLock Lock(StateLock);
    if (!State.bActive || !State.bHasConfirmedSpawn)
    {
        return false;
    }
    return TrySampleHeightMetersLocked(State.ConfirmedSpawnWorldMeters, OutHeightMeters);
}

void FCubusGeneratedTerrainRuntime::Reset()
{
    FWriteScopeLock Lock(StateLock);
    State = FState();
}

bool FCubusGeneratedTerrainRuntime::IsActive()
{
    FReadScopeLock Lock(StateLock);
    return State.bActive && !State.Tiles.IsEmpty();
}

int32 FCubusGeneratedTerrainRuntime::GetWorldSeed()
{
    FReadScopeLock Lock(StateLock);
    return State.WorldSeed;
}

int32 FCubusGeneratedTerrainRuntime::GetTileCount()
{
    FReadScopeLock Lock(StateLock);
    return State.Tiles.Num();
}

bool FCubusGeneratedTerrainRuntime::TrySampleHeightMetersLocked(
    const FVector2D& WorldMeters,
    float& OutHeightMeters
)
{
    const FIntPoint TileCoordinate = FCubusTerrainRasterBuilder::WorldToTileCoordinate(
        WorldMeters.X,
        WorldMeters.Y,
        State.RasterSettings
    );
    const FCubusTerrainRasterTile* Tile = State.Tiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        return false;
    }
    OutHeightMeters = Tile->SampleHeightMeters(WorldMeters.X, WorldMeters.Y);
    return true;
}

bool FCubusGeneratedTerrainRuntime::TrySampleTerrainForm(
    const float WorldX,
    const float WorldY,
    const float VoxelSizeCm,
    FCubusTerrainFormSample& OutSample
)
{
    FReadScopeLock Lock(StateLock);
    if (!State.bActive || State.Tiles.IsEmpty())
    {
        return false;
    }

    const double VoxelSizeMeters = static_cast<double>(FMath::Max(1.0f, VoxelSizeCm)) * 0.01;
    const double CanonicalVoxelX = static_cast<double>(WorldX) - static_cast<double>(State.TerrainDomainOffsetVoxels.X);
    const double CanonicalVoxelY = static_cast<double>(WorldY) - static_cast<double>(State.TerrainDomainOffsetVoxels.Y);
    const double WorldMetersX = CanonicalVoxelX * VoxelSizeMeters;
    const double WorldMetersY = CanonicalVoxelY * VoxelSizeMeters;

    float HeightMeters = 0.0f;
    if (!TrySampleHeightMetersLocked(FVector2D(WorldMetersX, WorldMetersY), HeightMeters))
    {
        OutSample = FCubusTerrainFormSample();
        OutSample.Height = 0.0f;
        OutSample.PlainsWeight = 1.0f;
        OutSample.RollingWeight = 0.0f;
        OutSample.MountainWeight = 0.0f;
        return true;
    }

    const FIntPoint TileCoordinate = FCubusTerrainRasterBuilder::WorldToTileCoordinate(
        WorldMetersX,
        WorldMetersY,
        State.RasterSettings
    );
    const FCubusTerrainRasterTile* Tile = State.Tiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        OutSample = FCubusTerrainFormSample();
        OutSample.Height = 0.0f;
        OutSample.PlainsWeight = 1.0f;
        OutSample.RollingWeight = 0.0f;
        OutSample.MountainWeight = 0.0f;
        return true;
    }

    const float HeightVoxels = HeightMeters / static_cast<float>(VoxelSizeMeters);
    OutSample = FCubusTerrainFormSample();
    OutSample.Height = HeightVoxels;

    const double ProbeMeters = FMath::Max(
        static_cast<double>(State.RasterSettings.SampleSpacingMeters),
        VoxelSizeMeters
    );
    const float Left = Tile->SampleHeightMeters(WorldMetersX - ProbeMeters, WorldMetersY);
    const float Right = Tile->SampleHeightMeters(WorldMetersX + ProbeMeters, WorldMetersY);
    const float Down = Tile->SampleHeightMeters(WorldMetersX, WorldMetersY - ProbeMeters);
    const float Up = Tile->SampleHeightMeters(WorldMetersX, WorldMetersY + ProbeMeters);
    const float Dx = (Right - Left) / static_cast<float>(ProbeMeters * 2.0);
    const float Dy = (Up - Down) / static_cast<float>(ProbeMeters * 2.0);
    const float Slope = FMath::Sqrt(Dx * Dx + Dy * Dy);

    const float MountainWeight = FMath::Clamp((HeightMeters - 550.0f) / 1300.0f, 0.0f, 1.0f);
    const float PlainsWeight = FMath::Clamp(1.0f - Slope / 0.18f, 0.0f, 1.0f) * (1.0f - MountainWeight);
    const float RollingWeight = FMath::Max(0.0f, 1.0f - PlainsWeight - MountainWeight);
    const float Total = FMath::Max(KINDA_SMALL_NUMBER, PlainsWeight + RollingWeight + MountainWeight);

    OutSample.PlainsWeight = PlainsWeight / Total;
    OutSample.RollingWeight = RollingWeight / Total;
    OutSample.MountainWeight = MountainWeight / Total;
    OutSample.MountainCore = MountainWeight;
    OutSample.FoothillWeight = FMath::Clamp(MountainWeight * (1.0f - MountainWeight) * 2.0f, 0.0f, 1.0f);
    OutSample.Ridge = FMath::Clamp((Slope - 0.35f) / 1.15f, 0.0f, 1.0f);
    OutSample.MassifWeight = MountainWeight;
    OutSample.Escarpment = FMath::Clamp((Slope - 0.60f) / 1.20f, 0.0f, 1.0f);
    OutSample.SurfaceRoughness = FMath::Clamp(Slope / 1.35f, 0.0f, 1.0f);
    return true;
}
