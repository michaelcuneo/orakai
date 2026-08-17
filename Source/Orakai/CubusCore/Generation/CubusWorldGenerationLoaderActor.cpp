#include "CubusCore/Generation/CubusWorldGenerationLoaderActor.h"

#include "Engine/Texture2D.h"
#include "Rendering/Texture2DResource.h"

ACubusWorldGenerationLoaderActor::ACubusWorldGenerationLoaderActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACubusWorldGenerationLoaderActor::BeginPlay()
{
    Super::BeginPlay();
    ResetSession();
    EnsurePreviewTexture();
    ClearPreview();
}

void ACubusWorldGenerationLoaderActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    (void)DeltaSeconds;

    const int32 Budget = FMath::Clamp(WorkItemsPerTick, 1, 8);
    for (int32 Work = 0; Work < Budget; ++Work)
    {
        switch (Stage)
        {
        case ECubusGenerationLoaderStage::StructuralDEM:
            ProcessStructuralDEM();
            break;
        case ECubusGenerationLoaderStage::Drainage:
            ProcessDrainage();
            break;
        case ECubusGenerationLoaderStage::TerrainCarving:
            ProcessTerrainCarving();
            break;
        default:
            return;
        }
    }
}

void ACubusWorldGenerationLoaderActor::StartGeneration(const int32 InWorldSeed)
{
    ResetSession();
    WorldSeed = InWorldSeed;
    BuildSettings();
    BuildWorkQueues();
    EnsurePreviewTexture();
    ClearPreview();

    if (TerrainTileQueue.IsEmpty())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    BeginStage(ECubusGenerationLoaderStage::StructuralDEM);
}

void ACubusWorldGenerationLoaderActor::CancelGeneration()
{
    ResetSession();
    EnsurePreviewTexture();
    ClearPreview();
}

FText ACubusWorldGenerationLoaderActor::GetStageDisplayName() const
{
    switch (Stage)
    {
    case ECubusGenerationLoaderStage::WaitingForSeed:
        return FText::FromString(TEXT("Waiting for Seed"));
    case ECubusGenerationLoaderStage::StructuralDEM:
        return FText::FromString(TEXT("Building Structural DEM"));
    case ECubusGenerationLoaderStage::Drainage:
        return FText::FromString(TEXT("Solving Drainage"));
    case ECubusGenerationLoaderStage::TerrainCarving:
        return FText::FromString(TEXT("Carving Valleys and Rivers"));
    case ECubusGenerationLoaderStage::Complete:
        return FText::FromString(TEXT("Terrain Generation Complete"));
    case ECubusGenerationLoaderStage::Failed:
        return FText::FromString(TEXT("Generation Failed"));
    default:
        return FText::GetEmpty();
    }
}

bool ACubusWorldGenerationLoaderActor::GetGeneratedHeightMeters(
    const double WorldXmeters,
    const double WorldYmeters,
    float& OutHeightMeters
) const
{
    const FIntPoint TileCoordinate = FCubusTerrainRasterBuilder::WorldToTileCoordinate(
        WorldXmeters,
        WorldYmeters,
        RasterSettings
    );

    const FCubusTerrainRasterTile* Tile = TerrainTiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        return false;
    }

    OutHeightMeters = Tile->SampleHeightMeters(WorldXmeters, WorldYmeters);
    return true;
}

void ACubusWorldGenerationLoaderActor::ResetSession()
{
    Stage = ECubusGenerationLoaderStage::WaitingForSeed;
    StageProgress = 0.0f;
    OverallProgress = 0.0f;
    CurrentWorkIndex = 0;
    TerrainTileQueue.Reset();
    DrainageRegionQueue.Reset();
    TerrainTiles.Reset();
    DrainageSegments.Reset();
}

void ACubusWorldGenerationLoaderActor::BuildSettings()
{
    RasterSettings = FCubusTerrainRasterSettings();
    RasterSettings.SampleSpacingMeters = FMath::Max(0.25f, DEMSampleSpacingMeters);
    RasterSettings.TileSizeMeters = FMath::Max(64.0f, DEMTileSizeMeters);
    RasterSettings.HaloSamples = 2;
    RasterSettings.Structure.Seed = WorldSeed;

    DrainageSettings = FCubusTerrainDrainageSettings();
    DrainageSettings.Raster = RasterSettings;
    DrainageSettings.AnalysisCellSizeMeters = FMath::Max(2.0f, DrainageCellSizeMeters);
    DrainageSettings.StreamSourceAreaSquareKm = FMath::Max(0.001f, StreamSourceAreaSquareKm);

    // For loader generation, align hydrology to the authored DEM instead of
    // allowing an arbitrary region boundary through world XY zero. A 4 km map
    // at the default 8 m analysis spacing fits in one 500 x 500 drainage domain.
    const FBox2D Bounds = GetGenerationBoundsMeters();
    const FVector2D Size = Bounds.GetSize();
    const double LongestSideMeters = FMath::Max(Size.X, Size.Y);
    const int32 RequiredCells = FMath::CeilToInt(
        LongestSideMeters / static_cast<double>(DrainageSettings.AnalysisCellSizeMeters)
    );
    DrainageSettings.RegionCellCount = FMath::Clamp(RequiredCells, 32, 512);
    DrainageSettings.RegionOriginMeters = Bounds.Min;

    CarvingSettings = FCubusTerrainCarvingSettings();
    CarvingSettings.Drainage = DrainageSettings;
}

void ACubusWorldGenerationLoaderActor::BuildWorkQueues()
{
    const FBox2D Bounds = GetGenerationBoundsMeters();
    const double TileSize = FCubusTerrainRasterBuilder::ResolveTileSizeMeters(RasterSettings);

    const FIntPoint MinimumTile(
        FMath::FloorToInt(Bounds.Min.X / TileSize),
        FMath::FloorToInt(Bounds.Min.Y / TileSize)
    );
    const FIntPoint MaximumTile(
        FMath::FloorToInt((Bounds.Max.X - UE_SMALL_NUMBER) / TileSize),
        FMath::FloorToInt((Bounds.Max.Y - UE_SMALL_NUMBER) / TileSize)
    );

    for (int32 Y = MinimumTile.Y; Y <= MaximumTile.Y; ++Y)
    {
        for (int32 X = MinimumTile.X; X <= MaximumTile.X; ++X)
        {
            TerrainTileQueue.Add(FIntPoint(X, Y));
        }
    }

    const double RegionSize =
        static_cast<double>(FMath::Clamp(DrainageSettings.RegionCellCount, 32, 512)) *
        static_cast<double>(DrainageSettings.AnalysisCellSizeMeters);

    const FIntPoint MinimumRegion(
        FMath::FloorToInt((Bounds.Min.X - DrainageSettings.RegionOriginMeters.X) / RegionSize),
        FMath::FloorToInt((Bounds.Min.Y - DrainageSettings.RegionOriginMeters.Y) / RegionSize)
    );
    const FIntPoint MaximumRegion(
        FMath::FloorToInt((Bounds.Max.X - DrainageSettings.RegionOriginMeters.X - UE_SMALL_NUMBER) / RegionSize),
        FMath::FloorToInt((Bounds.Max.Y - DrainageSettings.RegionOriginMeters.Y - UE_SMALL_NUMBER) / RegionSize)
    );

    for (int32 Y = MinimumRegion.Y; Y <= MaximumRegion.Y; ++Y)
    {
        for (int32 X = MinimumRegion.X; X <= MaximumRegion.X; ++X)
        {
            DrainageRegionQueue.Add(FIntPoint(X, Y));
        }
    }
}

void ACubusWorldGenerationLoaderActor::BeginStage(const ECubusGenerationLoaderStage NewStage)
{
    Stage = NewStage;
    StageProgress = 0.0f;
    CurrentWorkIndex = 0;
    OnStageChanged.Broadcast(Stage);
    OnProgressChanged.Broadcast(Stage, StageProgress);
}

void ACubusWorldGenerationLoaderActor::FinishCurrentPipeline()
{
    Stage = ECubusGenerationLoaderStage::Complete;
    StageProgress = 1.0f;
    OverallProgress = 1.0f;
    OnStageChanged.Broadcast(Stage);
    OnProgressChanged.Broadcast(Stage, 1.0f);
    OnGenerationFinished.Broadcast();
}

void ACubusWorldGenerationLoaderActor::ProcessStructuralDEM()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
        RebuildTerrainPreview(false);
        UploadPreview();
        BeginStage(ECubusGenerationLoaderStage::Drainage);
        return;
    }

    const FIntPoint TileCoordinate = TerrainTileQueue[CurrentWorkIndex];
    FCubusTerrainRasterTile Tile = FCubusTerrainRasterBuilder::BuildTile(TileCoordinate, RasterSettings);
    if (!Tile.IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    TerrainTiles.Add(TileCoordinate, Tile);
    PaintTileToPreview(Tile);
    RebuildTerrainPreview(false);
    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
    UploadPreview();
}

void ACubusWorldGenerationLoaderActor::ProcessDrainage()
{
    if (!DrainageRegionQueue.IsValidIndex(CurrentWorkIndex))
    {
        UploadPreview();
        BeginStage(ECubusGenerationLoaderStage::TerrainCarving);
        return;
    }

    const FIntPoint RegionCoordinate = DrainageRegionQueue[CurrentWorkIndex];
    const double RegionSize =
        static_cast<double>(FMath::Clamp(DrainageSettings.RegionCellCount, 32, 512)) *
        static_cast<double>(DrainageSettings.AnalysisCellSizeMeters);

    const FVector2D Minimum = DrainageSettings.RegionOriginMeters + FVector2D(
        static_cast<double>(RegionCoordinate.X) * RegionSize,
        static_cast<double>(RegionCoordinate.Y) * RegionSize
    );
    const FVector2D Maximum = Minimum + FVector2D(RegionSize, RegionSize);

    TArray<FCubusTerrainDrainageSegment> Segments;
    FCubusTerrainDrainage::CollectSegments(FBox2D(Minimum, Maximum), DrainageSettings, Segments);
    DrainageSegments.Append(Segments);
    DrawDrainageSegmentsToPreview(Segments);

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, DrainageRegionQueue.Num());
    UploadPreview();
}

void ACubusWorldGenerationLoaderActor::ProcessTerrainCarving()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
        RebuildTerrainPreview(true);
        UploadPreview();
        FinishCurrentPipeline();
        return;
    }

    const FIntPoint TileCoordinate = TerrainTileQueue[CurrentWorkIndex];
    FCubusTerrainRasterTile* StructuralTile = TerrainTiles.Find(TileCoordinate);
    if (StructuralTile == nullptr || !StructuralTile->IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    FCubusTerrainRasterTile Carved = FCubusTerrainCarving::CarveTile(*StructuralTile, CarvingSettings);
    if (!Carved.IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    *StructuralTile = MoveTemp(Carved);
    PaintTileToPreview(*StructuralTile);
    RebuildTerrainPreview(true);

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
    UploadPreview();
}

FBox2D ACubusWorldGenerationLoaderActor::GetGenerationBoundsMeters() const
{
    const FVector2D HalfExtent(
        FMath::Max(500.0, GenerationSizeKm.X * 500.0),
        FMath::Max(500.0, GenerationSizeKm.Y * 500.0)
    );
    return FBox2D(-HalfExtent, HalfExtent);
}

void ACubusWorldGenerationLoaderActor::SetWorkProgress(const int32 CompletedItems, const int32 TotalItems)
{
    StageProgress = TotalItems > 0
        ? FMath::Clamp(static_cast<float>(CompletedItems) / static_cast<float>(TotalItems), 0.0f, 1.0f)
        : 1.0f;

    float StageBase = 0.0f;
    switch (Stage)
    {
    case ECubusGenerationLoaderStage::StructuralDEM:
        StageBase = 0.0f;
        break;
    case ECubusGenerationLoaderStage::Drainage:
        StageBase = 1.0f / 3.0f;
        break;
    case ECubusGenerationLoaderStage::TerrainCarving:
        StageBase = 2.0f / 3.0f;
        break;
    default:
        break;
    }

    OverallProgress = FMath::Clamp(StageBase + StageProgress / 3.0f, 0.0f, 1.0f);
    OnProgressChanged.Broadcast(Stage, StageProgress);
}

void ACubusWorldGenerationLoaderActor::EnsurePreviewTexture()
{
    const int32 Resolution = FMath::Clamp(PreviewTextureResolution, 64, 2048);
    if (PreviewTexture != nullptr &&
        PreviewTexture->GetSizeX() == Resolution &&
        PreviewTexture->GetSizeY() == Resolution)
    {
        if (PreviewHeightMeters.Num() != Resolution * Resolution)
        {
            PreviewHeightMeters.Init(MAX_flt, Resolution * Resolution);
        }
        return;
    }

    PreviewTexture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
    if (PreviewTexture != nullptr)
    {
        PreviewTexture->SRGB = true;
        PreviewTexture->Filter = TF_Bilinear;
        PreviewTexture->NeverStream = true;
        PreviewTexture->UpdateResource();
    }

    PreviewHeightMeters.Init(MAX_flt, Resolution * Resolution);
    PreviewPixels.SetNumZeroed(Resolution * Resolution);
}

void ACubusWorldGenerationLoaderActor::ClearPreview()
{
    EnsurePreviewTexture();
    PreviewHeightMeters.Init(MAX_flt, PreviewPixels.Num());
    for (FColor& Pixel : PreviewPixels)
    {
        Pixel = FColor(8, 8, 8, 255);
    }
    UploadPreview();
}

void ACubusWorldGenerationLoaderActor::UploadPreview()
{
    if (PreviewTexture == nullptr || PreviewPixels.IsEmpty() || PreviewTexture->GetPlatformData() == nullptr)
    {
        return;
    }

    FTexture2DMipMap& Mip = PreviewTexture->GetPlatformData()->Mips[0];
    void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(TextureData, PreviewPixels.GetData(), PreviewPixels.Num() * sizeof(FColor));
    Mip.BulkData.Unlock();
    PreviewTexture->UpdateResource();
}

void ACubusWorldGenerationLoaderActor::PaintTileToPreview(const FCubusTerrainRasterTile& Tile)
{
    if (!Tile.IsValid())
    {
        return;
    }

    const int32 Resolution = FMath::Clamp(PreviewTextureResolution, 64, 2048);
    const FBox2D TileBounds(Tile.GetWorldMinimumMeters(), Tile.GetWorldMaximumMeters());

    for (int32 Y = 0; Y < Resolution; ++Y)
    {
        for (int32 X = 0; X < Resolution; ++X)
        {
            const FVector2D World = PreviewPixelToWorld(X, Y);
            if (!TileBounds.IsInsideOrOn(World))
            {
                continue;
            }

            PreviewHeightMeters[Y * Resolution + X] = Tile.SampleHeightMeters(World.X, World.Y);
        }
    }
}

void ACubusWorldGenerationLoaderActor::RebuildTerrainPreview(const bool bOverlayDrainage)
{
    const int32 Resolution = FMath::Clamp(PreviewTextureResolution, 64, 2048);
    if (PreviewHeightMeters.Num() != Resolution * Resolution || PreviewPixels.Num() != Resolution * Resolution)
    {
        return;
    }

    float MinimumHeight = MAX_flt;
    float MaximumHeight = -MAX_flt;
    for (const float Height : PreviewHeightMeters)
    {
        if (Height == MAX_flt)
        {
            continue;
        }
        MinimumHeight = FMath::Min(MinimumHeight, Height);
        MaximumHeight = FMath::Max(MaximumHeight, Height);
    }

    if (MinimumHeight == MAX_flt)
    {
        return;
    }

    // Always use the elevation range actually present in this generated map.
    // A minimum span prevents tiny numerical variation becoming full contrast.
    const float HeightSpan = FMath::Max(60.0f, MaximumHeight - MinimumHeight);
    const FBox2D Bounds = GetGenerationBoundsMeters();
    const FVector2D WorldSize = Bounds.GetSize();
    const float PixelWorldX = static_cast<float>(WorldSize.X / FMath::Max(1, Resolution - 1));
    const float PixelWorldY = static_cast<float>(WorldSize.Y / FMath::Max(1, Resolution - 1));
    const FVector LightDirection = FVector(-0.48f, -0.36f, 0.80f).GetSafeNormal();

    auto ReadHeight = [&](const int32 X, const int32 Y, const float Fallback)
    {
        const int32 SafeX = FMath::Clamp(X, 0, Resolution - 1);
        const int32 SafeY = FMath::Clamp(Y, 0, Resolution - 1);
        const float Height = PreviewHeightMeters[SafeY * Resolution + SafeX];
        return Height == MAX_flt ? Fallback : Height;
    };

    for (int32 Y = 0; Y < Resolution; ++Y)
    {
        for (int32 X = 0; X < Resolution; ++X)
        {
            const int32 Index = Y * Resolution + X;
            const float Height = PreviewHeightMeters[Index];
            if (Height == MAX_flt)
            {
                PreviewPixels[Index] = FColor(8, 8, 8, 255);
                continue;
            }

            float NormalizedHeight = FMath::Clamp((Height - MinimumHeight) / HeightSpan, 0.0f, 1.0f);
            NormalizedHeight = NormalizedHeight * NormalizedHeight * (3.0f - 2.0f * NormalizedHeight);

            float Hillshade = 1.0f;
            if (bPreviewHillshade)
            {
                const float Left = ReadHeight(X - 1, Y, Height);
                const float Right = ReadHeight(X + 1, Y, Height);
                const float Up = ReadHeight(X, Y - 1, Height);
                const float Down = ReadHeight(X, Y + 1, Height);
                const float Dx = (Right - Left) / FMath::Max(1.0f, PixelWorldX * 2.0f);
                const float Dy = (Down - Up) / FMath::Max(1.0f, PixelWorldY * 2.0f);
                const FVector SurfaceNormal = FVector(-Dx, Dy, 1.0f).GetSafeNormal();
                const float Lit = FMath::Clamp(FVector::DotProduct(SurfaceNormal, LightDirection), 0.0f, 1.0f);
                const float RawShade = 0.32f + Lit * 0.88f;
                Hillshade = FMath::Lerp(1.0f, RawShade, FMath::Clamp(PreviewHillshadeStrength, 0.0f, 1.0f));
            }

            PreviewPixels[Index] = HeightToPreviewColor(NormalizedHeight, Hillshade);
        }
    }

    if (bOverlayDrainage)
    {
        DrawDrainageSegmentsToPreview(DrainageSegments);
    }
}

void ACubusWorldGenerationLoaderActor::DrawDrainageSegmentsToPreview(
    const TArray<FCubusTerrainDrainageSegment>& Segments
)
{
    for (const FCubusTerrainDrainageSegment& Segment : Segments)
    {
        const float AreaStrength = FMath::Clamp(
            Segment.ContributingAreaSquareKm / FMath::Max(DrainageSettings.MajorRiverAreaSquareKm, 0.001f),
            0.0f,
            1.0f
        );
        const int32 Radius = FMath::Clamp(FMath::RoundToInt(1.0f + AreaStrength * 3.0f), 1, 4);
        const uint8 Intensity = static_cast<uint8>(FMath::Clamp(150.0f + AreaStrength * 105.0f, 0.0f, 255.0f));
        DrawPreviewLine(
            WorldToPreviewPixel(Segment.StartMeters),
            WorldToPreviewPixel(Segment.EndMeters),
            FColor(24, 96, Intensity, 255),
            Radius
        );
    }
}

FColor ACubusWorldGenerationLoaderActor::HeightToPreviewColor(
    const float NormalizedHeight,
    const float Hillshade
) const
{
    const float T = FMath::Clamp(NormalizedHeight, 0.0f, 1.0f);

    FLinearColor BaseColor;
    if (T < 0.28f)
    {
        BaseColor = FMath::Lerp(
            FLinearColor(0.055f, 0.12f, 0.07f),
            FLinearColor(0.18f, 0.29f, 0.13f),
            T / 0.28f
        );
    }
    else if (T < 0.58f)
    {
        BaseColor = FMath::Lerp(
            FLinearColor(0.18f, 0.29f, 0.13f),
            FLinearColor(0.42f, 0.36f, 0.22f),
            (T - 0.28f) / 0.30f
        );
    }
    else if (T < 0.82f)
    {
        BaseColor = FMath::Lerp(
            FLinearColor(0.42f, 0.36f, 0.22f),
            FLinearColor(0.52f, 0.52f, 0.49f),
            (T - 0.58f) / 0.24f
        );
    }
    else
    {
        BaseColor = FMath::Lerp(
            FLinearColor(0.52f, 0.52f, 0.49f),
            FLinearColor(0.93f, 0.95f, 0.96f),
            (T - 0.82f) / 0.18f
        );
    }

    BaseColor *= FMath::Clamp(Hillshade, 0.18f, 1.25f);
    BaseColor.A = 1.0f;
    return BaseColor.ToFColorSRGB();
}

FIntPoint ACubusWorldGenerationLoaderActor::WorldToPreviewPixel(const FVector2D& WorldMeters) const
{
    const int32 Resolution = FMath::Clamp(PreviewTextureResolution, 64, 2048);
    const FBox2D Bounds = GetGenerationBoundsMeters();
    const FVector2D Size = Bounds.GetSize();

    const double U = FMath::Clamp((WorldMeters.X - Bounds.Min.X) / FMath::Max(Size.X, 1.0), 0.0, 1.0);
    const double V = FMath::Clamp((WorldMeters.Y - Bounds.Min.Y) / FMath::Max(Size.Y, 1.0), 0.0, 1.0);

    return FIntPoint(
        FMath::Clamp(FMath::RoundToInt(U * static_cast<double>(Resolution - 1)), 0, Resolution - 1),
        FMath::Clamp(FMath::RoundToInt((1.0 - V) * static_cast<double>(Resolution - 1)), 0, Resolution - 1)
    );
}

FVector2D ACubusWorldGenerationLoaderActor::PreviewPixelToWorld(const int32 PixelX, const int32 PixelY) const
{
    const int32 Resolution = FMath::Clamp(PreviewTextureResolution, 64, 2048);
    const FBox2D Bounds = GetGenerationBoundsMeters();
    const FVector2D Size = Bounds.GetSize();
    const double U = static_cast<double>(PixelX) / static_cast<double>(FMath::Max(1, Resolution - 1));
    const double V = 1.0 - static_cast<double>(PixelY) / static_cast<double>(FMath::Max(1, Resolution - 1));
    return Bounds.Min + FVector2D(U * Size.X, V * Size.Y);
}

void ACubusWorldGenerationLoaderActor::DrawPreviewLine(
    FIntPoint Start,
    FIntPoint End,
    const FColor& Color,
    const int32 RadiusPixels
)
{
    const int32 Resolution = FMath::Clamp(PreviewTextureResolution, 64, 2048);
    const int32 DX = FMath::Abs(End.X - Start.X);
    const int32 SX = Start.X < End.X ? 1 : -1;
    const int32 DY = -FMath::Abs(End.Y - Start.Y);
    const int32 SY = Start.Y < End.Y ? 1 : -1;
    int32 Error = DX + DY;

    for (;;)
    {
        for (int32 OffsetY = -RadiusPixels; OffsetY <= RadiusPixels; ++OffsetY)
        {
            for (int32 OffsetX = -RadiusPixels; OffsetX <= RadiusPixels; ++OffsetX)
            {
                if (OffsetX * OffsetX + OffsetY * OffsetY > RadiusPixels * RadiusPixels)
                {
                    continue;
                }
                const int32 X = Start.X + OffsetX;
                const int32 Y = Start.Y + OffsetY;
                if (X >= 0 && Y >= 0 && X < Resolution && Y < Resolution)
                {
                    PreviewPixels[Y * Resolution + X] = Color;
                }
            }
        }

        if (Start == End)
        {
            break;
        }

        const int32 TwiceError = 2 * Error;
        if (TwiceError >= DY)
        {
            Error += DY;
            Start.X += SX;
        }
        if (TwiceError <= DX)
        {
            Error += DX;
            Start.Y += SY;
        }
    }
}
