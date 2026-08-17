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

    if (!State.bActive || State.WorldSeed != WorldSeed)
    {
        State = FState();
    }

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

    const FIntPoint TileCoordinate = FCubusTerrainRasterBuilder::WorldToTileCoordinate(
        WorldMetersX,
        WorldMetersY,
        State.RasterSettings
    );
    const FCubusTerrainRasterTile* Tile = State.Tiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        return false;
    }

    const float HeightMeters = Tile->SampleHeightMeters(WorldMetersX, WorldMetersY);
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
