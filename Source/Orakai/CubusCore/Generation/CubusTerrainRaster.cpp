#include "CubusCore/Generation/CubusTerrainRaster.h"
#include "CubusCore/Generation/CubusTerrainMorphology.h"

bool FCubusTerrainRasterTile::IsValid() const
{
    return InteriorCellCount > 0 &&
        StorageSampleCount == InteriorCellCount + 1 + HaloSamples * 2 &&
        HeightMeters.Num() == StorageSampleCount * StorageSampleCount &&
        SampleSpacingMeters > 0.0f &&
        TileSizeMeters > 0.0;
}

FVector2D FCubusTerrainRasterTile::GetWorldMinimumMeters() const
{
    return FVector2D(
        static_cast<double>(TileCoordinate.X) * TileSizeMeters,
        static_cast<double>(TileCoordinate.Y) * TileSizeMeters
    );
}

FVector2D FCubusTerrainRasterTile::GetWorldMaximumMeters() const
{
    return GetWorldMinimumMeters() + FVector2D(TileSizeMeters, TileSizeMeters);
}

int32 FCubusTerrainRasterTile::StorageIndex(const int32 GridX, const int32 GridY) const
{
    check(IsValid());
    const int32 StorageX = GridX + HaloSamples;
    const int32 StorageY = GridY + HaloSamples;
    check(StorageX >= 0 && StorageX < StorageSampleCount);
    check(StorageY >= 0 && StorageY < StorageSampleCount);
    return StorageY * StorageSampleCount + StorageX;
}

float FCubusTerrainRasterTile::GetHeightSampleMeters(const int32 GridX, const int32 GridY) const
{
    return HeightMeters[StorageIndex(GridX, GridY)];
}

float FCubusTerrainRasterTile::CubicInterpolate(
    const float P0,
    const float P1,
    const float P2,
    const float P3,
    const float Alpha
)
{
    const float A0 = -0.5f * P0 + 1.5f * P1 - 1.5f * P2 + 0.5f * P3;
    const float A1 = P0 - 2.5f * P1 + 2.0f * P2 - 0.5f * P3;
    const float A2 = -0.5f * P0 + 0.5f * P2;
    return ((A0 * Alpha + A1) * Alpha + A2) * Alpha + P1;
}

float FCubusTerrainRasterTile::SampleHeightMeters(
    const double WorldXmeters,
    const double WorldYmeters
) const
{
    check(IsValid());

    const FVector2D Minimum = GetWorldMinimumMeters();
    const double LocalSampleX = (WorldXmeters - Minimum.X) / static_cast<double>(SampleSpacingMeters);
    const double LocalSampleY = (WorldYmeters - Minimum.Y) / static_cast<double>(SampleSpacingMeters);

    const int32 BaseX = FMath::Clamp(FMath::FloorToInt(LocalSampleX), 0, InteriorCellCount - 1);
    const int32 BaseY = FMath::Clamp(FMath::FloorToInt(LocalSampleY), 0, InteriorCellCount - 1);
    const float AlphaX = FMath::Clamp(static_cast<float>(LocalSampleX - static_cast<double>(BaseX)), 0.0f, 1.0f);
    const float AlphaY = FMath::Clamp(static_cast<float>(LocalSampleY - static_cast<double>(BaseY)), 0.0f, 1.0f);

    float Rows[4];
    for (int32 Row = -1; Row <= 2; ++Row)
    {
        Rows[Row + 1] = CubicInterpolate(
            GetHeightSampleMeters(BaseX - 1, BaseY + Row),
            GetHeightSampleMeters(BaseX, BaseY + Row),
            GetHeightSampleMeters(BaseX + 1, BaseY + Row),
            GetHeightSampleMeters(BaseX + 2, BaseY + Row),
            AlphaX
        );
    }

    return CubicInterpolate(Rows[0], Rows[1], Rows[2], Rows[3], AlphaY);
}

int32 FCubusTerrainRasterBuilder::ResolveInteriorCellCount(const FCubusTerrainRasterSettings& Settings)
{
    const double SafeSpacing = FMath::Max(0.25, static_cast<double>(Settings.SampleSpacingMeters));
    const double SafeTileSize = FMath::Max(SafeSpacing, static_cast<double>(Settings.TileSizeMeters));
    return FMath::Max(1, FMath::RoundToInt(SafeTileSize / SafeSpacing));
}

double FCubusTerrainRasterBuilder::ResolveTileSizeMeters(const FCubusTerrainRasterSettings& Settings)
{
    const double SafeSpacing = FMath::Max(0.25, static_cast<double>(Settings.SampleSpacingMeters));
    return static_cast<double>(ResolveInteriorCellCount(Settings)) * SafeSpacing;
}

float FCubusTerrainRasterBuilder::SampleStructuralHeightMeters(
    const double WorldXmeters,
    const double WorldYmeters,
    const FCubusTerrainRasterSettings& Settings
)
{
    const double SourceXmeters = WorldXmeters + Settings.DomainOffsetMeters.X;
    const double SourceYmeters = WorldYmeters + Settings.DomainOffsetMeters.Y;
    const FCubusTerrainStructureSample Structure = FCubusTerrainStructure::Sample(
        SourceXmeters,
        SourceYmeters,
        Settings.Structure
    );
    const FCubusTerrainMorphologySample Morphology = FCubusTerrainMorphology::Sample(
        SourceXmeters,
        SourceYmeters,
        Structure,
        Settings.Structure
    );
    return Structure.HeightMeters + Morphology.HeightOffsetMeters;
}

FIntPoint FCubusTerrainRasterBuilder::WorldToTileCoordinate(
    const double WorldXmeters,
    const double WorldYmeters,
    const FCubusTerrainRasterSettings& Settings
)
{
    const double TileSize = ResolveTileSizeMeters(Settings);
    return FIntPoint(
        FMath::FloorToInt(WorldXmeters / TileSize),
        FMath::FloorToInt(WorldYmeters / TileSize)
    );
}

FCubusTerrainRasterTile FCubusTerrainRasterBuilder::BuildTile(
    const FIntPoint& TileCoordinate,
    const FCubusTerrainRasterSettings& InSettings
)
{
    FCubusTerrainRasterSettings Settings = InSettings;
    Settings.SampleSpacingMeters = FMath::Max(0.25f, Settings.SampleSpacingMeters);
    Settings.HaloSamples = FMath::Max(2, Settings.HaloSamples);

    FCubusTerrainRasterTile Tile;
    Tile.TileCoordinate = TileCoordinate;
    Tile.SampleSpacingMeters = Settings.SampleSpacingMeters;
    Tile.InteriorCellCount = ResolveInteriorCellCount(Settings);
    Tile.TileSizeMeters = ResolveTileSizeMeters(Settings);
    Tile.HaloSamples = Settings.HaloSamples;
    Tile.StorageSampleCount = Tile.InteriorCellCount + 1 + Tile.HaloSamples * 2;
    Tile.HeightMeters.SetNumUninitialized(Tile.StorageSampleCount * Tile.StorageSampleCount);

    const FVector2D TileMinimum = Tile.GetWorldMinimumMeters();
    for (int32 StorageY = 0; StorageY < Tile.StorageSampleCount; ++StorageY)
    {
        const int32 GridY = StorageY - Tile.HaloSamples;
        const double WorldYmeters = TileMinimum.Y + static_cast<double>(GridY) * Tile.SampleSpacingMeters;

        for (int32 StorageX = 0; StorageX < Tile.StorageSampleCount; ++StorageX)
        {
            const int32 GridX = StorageX - Tile.HaloSamples;
            const double WorldXmeters = TileMinimum.X + static_cast<double>(GridX) * Tile.SampleSpacingMeters;
            Tile.HeightMeters[StorageY * Tile.StorageSampleCount + StorageX] =
                SampleStructuralHeightMeters(WorldXmeters, WorldYmeters, Settings);
        }
    }

    return Tile;
}
