#include "CubusCore/Generation/CubusWorldGenerationLoaderActor.h"
#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"
#include "CubusCore/Generation/CubusGeneratedLoadingWorldActor.h"
#include "CubusCore/Actors/CubusTerrainLodWorldActor.h"
#include "Gameplay/WorldObjects/CubusSpawnStreamingPawn.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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
        case ECubusGenerationLoaderStage::Erosion:
            ProcessErosion();
            break;
        case ECubusGenerationLoaderStage::Deposition:
            ProcessDeposition();
            break;
        case ECubusGenerationLoaderStage::FineErosion:
            ProcessFineErosion();
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

    FCubusGeneratedTerrainRuntime::Configure(WorldSeed, RasterSettings);

    BuildWorkQueues();
    EnsurePreviewTexture();
    ClearPreview();

    if (TerrainTileQueue.IsEmpty())
    {
        FCubusGeneratedTerrainRuntime::Reset();
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    BeginStage(ECubusGenerationLoaderStage::StructuralDEM);
}

void ACubusWorldGenerationLoaderActor::CancelGeneration()
{
    FCubusGeneratedTerrainRuntime::Reset();
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
    case ECubusGenerationLoaderStage::Erosion:
        return FText::FromString(TEXT("Coarse Hillslope Erosion"));
    case ECubusGenerationLoaderStage::Deposition:
        return FText::FromString(TEXT("Building Floodplains and Alluvial Fans"));
    case ECubusGenerationLoaderStage::FineErosion:
        return FText::FromString(TEXT("Cutting Fine Gullies and Weathering"));
    case ECubusGenerationLoaderStage::SupportChunks:
    case ECubusGenerationLoaderStage::GameplayChunks:
    case ECubusGenerationLoaderStage::TerrainLOD:
        return FText::FromString(TEXT("Transferring Generated World"));
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
    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (PlayerController->GetPawn() == RuntimeStreamingPawn)
        {
            PlayerController->UnPossess();
        }
    }

    if (IsValid(RuntimeStreamingPawn))
    {
        RuntimeStreamingPawn->Destroy();
    }
    if (IsValid(RuntimeLodWorld))
    {
        RuntimeLodWorld->Destroy();
    }
    if (IsValid(RuntimeBlockWorld))
    {
        RuntimeBlockWorld->Destroy();
    }

    RuntimeStreamingPawn = nullptr;
    RuntimeLodWorld = nullptr;
    RuntimeBlockWorld = nullptr;

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
    ErosionSettings = FCubusTerrainErosionSettings();
    ErosionSettings.Iterations = FMath::Clamp(ErosionIterations, 1, 12);
    ErosionSettings.TalusAngleDegrees = FMath::Clamp(ErosionTalusAngleDegrees, 5.0f, 60.0f);
    ErosionSettings.ThermalTransport = 0.23f;
    ErosionSettings.ConcavityIncision = 0.20f;
    ErosionSettings.MaxIncisionPerIterationMeters = 0.55f;
    ErosionSettings.MinimumIncisionSlopeDegrees = 2.5f;
    ErosionSettings.HillslopeDiffusion = 0.045f;

    DepositionSettings = FCubusTerrainDepositionSettings();
    DepositionSettings.Iterations = FMath::Clamp(DepositionIterations, 1, 8);

    FineErosionSettings = FCubusTerrainErosionSettings();
    FineErosionSettings.Iterations = FMath::Clamp(FineErosionIterations, 1, 8);
    FineErosionSettings.TalusAngleDegrees = 36.0f;
    FineErosionSettings.ThermalTransport = 0.11f;
    FineErosionSettings.ConcavityIncision = 0.42f;
    FineErosionSettings.MaxIncisionPerIterationMeters = 0.38f;
    FineErosionSettings.MinimumIncisionSlopeDegrees = 1.4f;
    FineErosionSettings.HillslopeDiffusion = 0.018f;

    RasterSettings = FCubusTerrainRasterSettings();
    RasterSettings.SampleSpacingMeters = FMath::Max(0.25f, DEMSampleSpacingMeters);
    RasterSettings.TileSizeMeters = FMath::Max(64.0f, DEMTileSizeMeters);
    RasterSettings.HaloSamples = FMath::Max(
        2,
        ErosionSettings.Iterations +
        DepositionSettings.Iterations +
        FineErosionSettings.Iterations + 3
    );
    RasterSettings.Structure.Seed = WorldSeed;

    DrainageSettings = FCubusTerrainDrainageSettings();
    DrainageSettings.Raster = RasterSettings;
    DrainageSettings.AnalysisCellSizeMeters = FMath::Max(2.0f, DrainageCellSizeMeters);
    DrainageSettings.StreamSourceAreaSquareKm = FMath::Max(0.001f, StreamSourceAreaSquareKm);

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
    CarvingSettings.SegmentBucketSizeMeters = 96.0f;
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

void ACubusWorldGenerationLoaderActor::BeginRuntimeTerrainLoading()
{
    // Lvl_Generator owns DEM authoring only. Do not create gameplay chunks,
    // LOD actors, vegetation or a streaming focus in this map. Publish the
    // authoritative handoff and transfer immediately to Lvl_ThirdPerson, where
    // the actual gameplay world will stream around the proposed marker.
    if (!IsValid(GetWorld()) || GameplayLevelName.IsNone())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    RebuildTerrainPreview(false);
    UploadPreview();
    PublishPreviewSnapshotToRuntime();
    FCubusGeneratedTerrainRuntime::SetProposedSpawnFromPreviewUV(FVector2D(0.5, 0.5));

    Stage = ECubusGenerationLoaderStage::Complete;
    StageProgress = 1.0f;
    OverallProgress = 1.0f;
    OnStageChanged.Broadcast(Stage);
    OnProgressChanged.Broadcast(Stage, 1.0f);
    OnGenerationFinished.Broadcast();

    UE_LOG(LogTemp, Display,
        TEXT("Cubus DEM generation complete: seed=%d tiles=%d; opening gameplay spawn map %s"),
        WorldSeed,
        FCubusGeneratedTerrainRuntime::GetTileCount(),
        *GameplayLevelName.ToString());

    UGameplayStatics::OpenLevel(this, GameplayLevelName);
}

void ACubusWorldGenerationLoaderActor::UpdateRuntimeLoadingProgress(
    const float InStageProgress,
    const float StageIndex)
{
    // Legacy source compatibility only. Gameplay loading now lives entirely in
    // Lvl_ThirdPerson and is reported by its BlockWorld/LOD actors.
    StageProgress = FMath::Clamp(InStageProgress, 0.0f, 1.0f);
    OverallProgress = StageProgress;
    (void)StageIndex;
    OnProgressChanged.Broadcast(Stage, StageProgress);
}

void ACubusWorldGenerationLoaderActor::ProcessRuntimeTerrainLoading()
{
    // Intentionally unused: Lvl_Generator never builds gameplay terrain.
}

void ACubusWorldGenerationLoaderActor::FinishCurrentPipeline()
{
    // Intentionally unused by the DEM-only generator path.
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
    FCubusGeneratedTerrainRuntime::StoreTile(Tile);
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
        RebuildTerrainPreview(false);
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
        RebuildTerrainPreview(false);
        UploadPreview();
        BeginStage(ECubusGenerationLoaderStage::Erosion);
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
    FCubusGeneratedTerrainRuntime::StoreTileIfActive(*StructuralTile);
    PaintTileToPreview(*StructuralTile);
    RebuildTerrainPreview(false);

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
    UploadPreview();
}

void ACubusWorldGenerationLoaderActor::ProcessErosion()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
        RebuildTerrainPreview(false);
        UploadPreview();
        BeginStage(ECubusGenerationLoaderStage::Deposition);
        return;
    }

    const FIntPoint TileCoordinate = TerrainTileQueue[CurrentWorkIndex];
    FCubusTerrainRasterTile* Tile = TerrainTiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    FCubusTerrainRasterTile Eroded = FCubusTerrainErosion::ErodeTile(*Tile, ErosionSettings);
    if (!Eroded.IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    *Tile = MoveTemp(Eroded);
    FCubusGeneratedTerrainRuntime::StoreTileIfActive(*Tile);
    PaintTileToPreview(*Tile);
    RebuildTerrainPreview(false);

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
    UploadPreview();
}

void ACubusWorldGenerationLoaderActor::ProcessDeposition()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
        RebuildTerrainPreview(false);
        UploadPreview();
        BeginStage(ECubusGenerationLoaderStage::FineErosion);
        return;
    }

    const FIntPoint TileCoordinate = TerrainTileQueue[CurrentWorkIndex];
    FCubusTerrainRasterTile* Tile = TerrainTiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    FCubusTerrainRasterTile Deposited = FCubusTerrainDeposition::DepositTile(
        *Tile,
        DepositionSettings,
        CarvingSettings,
        DrainageSegments
    );
    if (!Deposited.IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    *Tile = MoveTemp(Deposited);
    FCubusGeneratedTerrainRuntime::StoreTileIfActive(*Tile);
    PaintTileToPreview(*Tile);
    RebuildTerrainPreview(false);

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
    UploadPreview();
}

void ACubusWorldGenerationLoaderActor::ProcessFineErosion()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
        RebuildTerrainPreview(false);
        UploadPreview();
        BeginRuntimeTerrainLoading();
        return;
    }

    const FIntPoint TileCoordinate = TerrainTileQueue[CurrentWorkIndex];
    FCubusTerrainRasterTile* Tile = TerrainTiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    FCubusTerrainRasterTile Refined = FCubusTerrainErosion::ErodeTile(*Tile, FineErosionSettings);
    if (!Refined.IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    *Tile = MoveTemp(Refined);
    FCubusGeneratedTerrainRuntime::StoreTileIfActive(*Tile);
    PaintTileToPreview(*Tile);
    RebuildTerrainPreview(false);

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

    constexpr float TotalStageCount = 6.0f;
    float StageIndex = 0.0f;
    switch (Stage)
    {
    case ECubusGenerationLoaderStage::StructuralDEM:
        StageIndex = 0.0f;
        break;
    case ECubusGenerationLoaderStage::Drainage:
        StageIndex = 1.0f;
        break;
    case ECubusGenerationLoaderStage::TerrainCarving:
        StageIndex = 2.0f;
        break;
    case ECubusGenerationLoaderStage::Erosion:
        StageIndex = 3.0f;
        break;
    case ECubusGenerationLoaderStage::Deposition:
        StageIndex = 4.0f;
        break;
    case ECubusGenerationLoaderStage::FineErosion:
        StageIndex = 5.0f;
        break;
    default:
        break;
    }

    OverallProgress = FMath::Clamp((StageIndex + StageProgress) / TotalStageCount, 0.0f, 0.999f);
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
