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

    // The generator page is 3D-preview only while authoring. Do not allocate,
    // rebuild, hillshade or upload the legacy 2D preview texture here.
    if (ACubusWorldGenerationPreviewActor* Preview3D = GetOrCreateGeneratedTerrain3DPreview())
    {
        Preview3D->TargetLoader = this;
        Preview3D->RefreshPreviewNow();
    }
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

    if (ACubusWorldGenerationPreviewActor* Preview3D = GetOrCreateGeneratedTerrain3DPreview())
    {
        Preview3D->TargetLoader = this;
        Preview3D->RefreshPreviewNow();
    }

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

    if (ACubusWorldGenerationPreviewActor* Preview3D = GetOrCreateGeneratedTerrain3DPreview())
    {
        Preview3D->TargetLoader = this;
        Preview3D->RefreshPreviewNow();
    }
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
    float& OutHeightMeters) const
{
    const FIntPoint TileCoordinate = FCubusTerrainRasterBuilder::WorldToTileCoordinate(
        WorldXmeters,
        WorldYmeters,
        RasterSettings);

    const FCubusTerrainRasterTile* Tile = TerrainTiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        return false;
    }

    OutHeightMeters = Tile->SampleHeightMeters(WorldXmeters, WorldYmeters);
    return true;
}

bool ACubusWorldGenerationLoaderActor::GetLivePreviewHeightField(
    const int32 Resolution,
    TArray<float>& OutHeights,
    TArray<uint8>& OutValid) const
{
    const int32 SafeResolution = FMath::Clamp(Resolution, 2, 1024);
    const int32 SampleCount = SafeResolution * SafeResolution;
    OutHeights.SetNumUninitialized(SampleCount);
    OutValid.Init(0, SampleCount);

    const FBox2D Bounds = GetGenerationBoundsMeters();
    const FVector2D Size = Bounds.GetSize();
    bool bAnyValid = false;

    for (int32 Y = 0; Y < SafeResolution; ++Y)
    {
        const double V = static_cast<double>(Y) / static_cast<double>(SafeResolution - 1);
        const double WorldY = Bounds.Min.Y + V * Size.Y;
        for (int32 X = 0; X < SafeResolution; ++X)
        {
            const double U = static_cast<double>(X) / static_cast<double>(SafeResolution - 1);
            const double WorldX = Bounds.Min.X + U * Size.X;
            const int32 Index = Y * SafeResolution + X;
            float HeightMeters = 0.0f;
            if (GetGeneratedHeightMeters(WorldX, WorldY, HeightMeters))
            {
                OutHeights[Index] = HeightMeters;
                OutValid[Index] = 1;
                bAnyValid = true;
            }
            else
            {
                OutHeights[Index] = 0.0f;
            }
        }
    }

    return bAnyValid;
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
    PreviewHeightMeters.Reset();
    PreviewPixels.Reset();
    PreviewTexture = nullptr;
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
        FineErosionSettings.Iterations + 3);
    RasterSettings.Structure.Seed = WorldSeed;

    DrainageSettings = FCubusTerrainDrainageSettings();
    DrainageSettings.Raster = RasterSettings;
    DrainageSettings.AnalysisCellSizeMeters = FMath::Max(2.0f, DrainageCellSizeMeters);
    DrainageSettings.StreamSourceAreaSquareKm = FMath::Max(0.001f, StreamSourceAreaSquareKm);

    const FBox2D Bounds = GetGenerationBoundsMeters();
    const FVector2D Size = Bounds.GetSize();
    const double LongestSideMeters = FMath::Max(Size.X, Size.Y);
    const int32 RequiredCells = FMath::CeilToInt(
        LongestSideMeters / static_cast<double>(DrainageSettings.AnalysisCellSizeMeters));
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
        FMath::FloorToInt(Bounds.Min.Y / TileSize));
    const FIntPoint MaximumTile(
        FMath::FloorToInt((Bounds.Max.X - UE_SMALL_NUMBER) / TileSize),
        FMath::FloorToInt((Bounds.Max.Y - UE_SMALL_NUMBER) / TileSize));

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
        FMath::FloorToInt((Bounds.Min.Y - DrainageSettings.RegionOriginMeters.Y) / RegionSize));
    const FIntPoint MaximumRegion(
        FMath::FloorToInt((Bounds.Max.X - DrainageSettings.RegionOriginMeters.X - UE_SMALL_NUMBER) / RegionSize),
        FMath::FloorToInt((Bounds.Max.Y - DrainageSettings.RegionOriginMeters.Y - UE_SMALL_NUMBER) / RegionSize));

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
    if (!IsValid(GetWorld()) || GameplayLevelName.IsNone())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    // The legacy 2D texture is needed only as a compact handoff snapshot for
    // runtime bounds/fallback display. Build it ONCE after the authoritative DEM
    // is finished; never rebuild or upload it during generation.
    EnsurePreviewTexture();
    ClearPreview();
    for (const TPair<FIntPoint, FCubusTerrainRasterTile>& Pair : TerrainTiles)
    {
        PaintTileToPreview(Pair.Value);
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
    StageProgress = FMath::Clamp(InStageProgress, 0.0f, 1.0f);
    OverallProgress = StageProgress;
    (void)StageIndex;
    OnProgressChanged.Broadcast(Stage, StageProgress);
}

void ACubusWorldGenerationLoaderActor::ProcessRuntimeTerrainLoading()
{
}

void ACubusWorldGenerationLoaderActor::FinishCurrentPipeline()
{
}

void ACubusWorldGenerationLoaderActor::ProcessStructuralDEM()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
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

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
}

void ACubusWorldGenerationLoaderActor::ProcessDrainage()
{
    if (!DrainageRegionQueue.IsValidIndex(CurrentWorkIndex))
    {
        BeginStage(ECubusGenerationLoaderStage::TerrainCarving);
        return;
    }

    const FIntPoint RegionCoordinate = DrainageRegionQueue[CurrentWorkIndex];
    const double RegionSize =
        static_cast<double>(FMath::Clamp(DrainageSettings.RegionCellCount, 32, 512)) *
        static_cast<double>(DrainageSettings.AnalysisCellSizeMeters);

    const FVector2D Minimum = DrainageSettings.RegionOriginMeters + FVector2D(
        static_cast<double>(RegionCoordinate.X) * RegionSize,
        static_cast<double>(RegionCoordinate.Y) * RegionSize);
    const FVector2D Maximum = Minimum + FVector2D(RegionSize, RegionSize);

    TArray<FCubusTerrainDrainageSegment> Segments;
    FCubusTerrainDrainage::CollectSegments(FBox2D(Minimum, Maximum), DrainageSettings, Segments);
    DrainageSegments.Append(Segments);

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, DrainageRegionQueue.Num());
}

void ACubusWorldGenerationLoaderActor::ProcessTerrainCarving()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
        BeginStage(ECubusGenerationLoaderStage::Erosion);
        return;
    }

    const FIntPoint TileCoordinate = TerrainTileQueue[CurrentWorkIndex];
    FCubusTerrainRasterTile* Tile = TerrainTiles.Find(TileCoordinate);
    if (Tile == nullptr || !Tile->IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    FCubusTerrainRasterTile Carved = FCubusTerrainCarving::CarveTile(*Tile, CarvingSettings);
    if (!Carved.IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    *Tile = MoveTemp(Carved);
    FCubusGeneratedTerrainRuntime::StoreTileIfActive(*Tile);

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
}

void ACubusWorldGenerationLoaderActor::ProcessErosion()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
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

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
}

void ACubusWorldGenerationLoaderActor::ProcessDeposition()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
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
        DrainageSegments);
    if (!Deposited.IsValid())
    {
        BeginStage(ECubusGenerationLoaderStage::Failed);
        return;
    }

    *Tile = MoveTemp(Deposited);
    FCubusGeneratedTerrainRuntime::StoreTileIfActive(*Tile);

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
}

void ACubusWorldGenerationLoaderActor::ProcessFineErosion()
{
    if (!TerrainTileQueue.IsValidIndex(CurrentWorkIndex))
    {
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

    ++CurrentWorkIndex;
    SetWorkProgress(CurrentWorkIndex, TerrainTileQueue.Num());
}

FBox2D ACubusWorldGenerationLoaderActor::GetGenerationBoundsMeters() const
{
    const FVector2D HalfExtent(
        FMath::Max(500.0, GenerationSizeKm.X * 500.0),
        FMath::Max(500.0, GenerationSizeKm.Y * 500.0));
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
    case ECubusGenerationLoaderStage::StructuralDEM: StageIndex = 0.0f; break;
    case ECubusGenerationLoaderStage::Drainage: StageIndex = 1.0f; break;
    case ECubusGenerationLoaderStage::TerrainCarving: StageIndex = 2.0f; break;
    case ECubusGenerationLoaderStage::Erosion: StageIndex = 3.0f; break;
    case ECubusGenerationLoaderStage::Deposition: StageIndex = 4.0f; break;
    case ECubusGenerationLoaderStage::FineErosion: StageIndex = 5.0f; break;
    default: break;
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
        if (PreviewPixels.Num() != Resolution * Resolution)
        {
            PreviewPixels.SetNumZeroed(Resolution * Resolution);
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
    if (!Tile.IsValid() || PreviewHeightMeters.IsEmpty())
    {
        return;
    }

    const int32 Resolution = FMath::Clamp(PreviewTextureResolution, 64, 2048);
    const FBox2D WorldBounds = GetGenerationBoundsMeters();
    const FVector2D WorldSize = WorldBounds.GetSize();
    const FVector2D TileMin = Tile.GetWorldMinimumMeters();
    const FVector2D TileMax = Tile.GetWorldMaximumMeters();

    const auto WorldToX = [&](const double WorldX)
    {
        const double U = (WorldX - WorldBounds.Min.X) / FMath::Max(1.0, WorldSize.X);
        return FMath::Clamp(FMath::FloorToInt(U * static_cast<double>(Resolution - 1)), 0, Resolution - 1);
    };
    const auto WorldToY = [&](const double WorldY)
    {
        const double V = (WorldY - WorldBounds.Min.Y) / FMath::Max(1.0, WorldSize.Y);
        return FMath::Clamp(FMath::FloorToInt(V * static_cast<double>(Resolution - 1)), 0, Resolution - 1);
    };

    const int32 MinX = WorldToX(TileMin.X);
    const int32 MaxX = WorldToX(TileMax.X);
    const int32 MinY = WorldToY(TileMin.Y);
    const int32 MaxY = WorldToY(TileMax.Y);

    for (int32 Y = MinY; Y <= MaxY; ++Y)
    {
        const double V = static_cast<double>(Y) / static_cast<double>(Resolution - 1);
        const double WorldY = WorldBounds.Min.Y + V * WorldSize.Y;
        for (int32 X = MinX; X <= MaxX; ++X)
        {
            const double U = static_cast<double>(X) / static_cast<double>(Resolution - 1);
            const double WorldX = WorldBounds.Min.X + U * WorldSize.X;
            PreviewHeightMeters[Y * Resolution + X] = Tile.SampleHeightMeters(WorldX, WorldY);
        }
    }
}

void ACubusWorldGenerationLoaderActor::RebuildTerrainPreview(const bool bOverlayDrainage)
{
    (void)bOverlayDrainage;
    const int32 Resolution = FMath::Clamp(PreviewTextureResolution, 64, 2048);
    if (PreviewHeightMeters.Num() != Resolution * Resolution || PreviewPixels.Num() != Resolution * Resolution)
    {
        return;
    }

    float MinHeight = MAX_flt;
    float MaxHeight = -MAX_flt;
    for (const float Height : PreviewHeightMeters)
    {
        if (Height != MAX_flt)
        {
            MinHeight = FMath::Min(MinHeight, Height);
            MaxHeight = FMath::Max(MaxHeight, Height);
        }
    }

    if (MinHeight == MAX_flt)
    {
        return;
    }

    const float Span = FMath::Max(1.0f, MaxHeight - MinHeight);
    for (int32 Index = 0; Index < PreviewHeightMeters.Num(); ++Index)
    {
        const float Height = PreviewHeightMeters[Index];
        if (Height == MAX_flt)
        {
            PreviewPixels[Index] = FColor(8, 8, 8, 255);
            continue;
        }

        const float T = FMath::Clamp((Height - MinHeight) / Span, 0.0f, 1.0f);
        const FLinearColor Low(0.055f, 0.12f, 0.07f, 1.0f);
        const FLinearColor Mid(0.42f, 0.36f, 0.22f, 1.0f);
        const FLinearColor High(0.93f, 0.95f, 0.96f, 1.0f);
        const FLinearColor Color = T < 0.6f
            ? FMath::Lerp(Low, Mid, T / 0.6f)
            : FMath::Lerp(Mid, High, (T - 0.6f) / 0.4f);
        PreviewPixels[Index] = Color.ToFColorSRGB();
    }
}
