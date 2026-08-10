#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Actors/CubusWorldVegetationActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Meshing/CubusDensityLod.h"
#include "CubusCore/Persistence/OrakaiPersistenceSubsystem.h"
#include "CubusCore/Persistence/OrakaiPersistenceTypes.h"
#include "CubusCore/Storage/CubusChunkStore.h"
#include "CubusCore/Weather/CubusWeatherMaterialUtilities.h"
#include "HAL/PlatformTime.h"

#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

namespace CubusBlockWorldActor
{
    const FIntVector NeighbourOffsets[] =
    {
        FIntVector(1, 0, 0),
        FIntVector(-1, 0, 0),
        FIntVector(0, 1, 0),
        FIntVector(0, -1, 0),
        FIntVector(0, 0, 1),
        FIntVector(0, 0, -1)
    };
}

ACubusBlockWorldActor::ACubusBlockWorldActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    WorldRoot =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("WorldRoot")
        );

    SetRootComponent(WorldRoot);
    WorldRoot->SetMobility(EComponentMobility::Static);
}

void ACubusBlockWorldActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    GridDimensions.X = FMath::Max(1, GridDimensions.X);
    GridDimensions.Y = FMath::Max(1, GridDimensions.Y);
    GridDimensions.Z = FMath::Max(1, GridDimensions.Z);

    GeneratedVoxelSize = FMath::Max(1.0f, GeneratedVoxelSize);
    DensityNearSampleSpacing = FMath::Clamp(
        DensityNearSampleSpacing,
        1.0f,
        GeneratedVoxelSize
    );
    DensityMiddleSampleSpacing = FMath::Clamp(
        DensityMiddleSampleSpacing,
        DensityNearSampleSpacing,
        GeneratedVoxelSize
    );
    DensityFarSampleSpacing = FMath::Clamp(
        DensityFarSampleSpacing,
        DensityMiddleSampleSpacing,
        GeneratedVoxelSize
    );
    DensityNearChunkRadius = FMath::Max(0, DensityNearChunkRadius);
    DensityMiddleChunkRadius = FMath::Max(
        DensityNearChunkRadius,
        DensityMiddleChunkRadius
    );

    TerrainContinentAmplitude = FMath::Max(0.0f, TerrainContinentAmplitude);
    TerrainContinentFrequency = FMath::Max(0.000001f, TerrainContinentFrequency);
    TerrainHillAmplitude = FMath::Max(0.0f, TerrainHillAmplitude);
    TerrainHillFrequency = FMath::Max(0.000001f, TerrainHillFrequency);
    TerrainDetailAmplitude = FMath::Max(0.0f, TerrainDetailAmplitude);
    TerrainDetailFrequency = FMath::Max(0.000001f, TerrainDetailFrequency);
    TerrainRidgeAmplitude = FMath::Max(0.0f, TerrainRidgeAmplitude);
    TerrainRidgeFrequency = FMath::Max(0.000001f, TerrainRidgeFrequency);
    TerrainValleyDepth = FMath::Max(0.0f, TerrainValleyDepth);
    TerrainValleyFrequency = FMath::Max(0.000001f, TerrainValleyFrequency);
    TerrainValleyWidth = FMath::Clamp(TerrainValleyWidth, 0.0f, 1.0f);
    TerrainValleyFalloff = FMath::Clamp(TerrainValleyFalloff, 0.001f, 1.0f);
    TerrainValleyWarpAmplitude = FMath::Max(0.0f, TerrainValleyWarpAmplitude);
    TerrainValleyWarpFrequency = FMath::Max(0.000001f, TerrainValleyWarpFrequency);
    TerrainRegionFrequency = FMath::Max(0.000001f, TerrainRegionFrequency);
    TerrainPlainsThreshold = FMath::Clamp(TerrainPlainsThreshold, -1.0f, 1.0f);
    TerrainPlainsBlend = FMath::Clamp(TerrainPlainsBlend, 0.001f, 1.0f);
    TerrainMountainThreshold = FMath::Clamp(
        TerrainMountainThreshold,
        TerrainPlainsThreshold,
        1.0f
    );
    TerrainMountainBlend = FMath::Clamp(TerrainMountainBlend, 0.001f, 1.0f);

    TerrainRockMaterialId = FMath::Max(1, TerrainRockMaterialId);
    TerrainSnowMaterialId = FMath::Max(1, TerrainSnowMaterialId);
    TerrainRockSlopeThreshold = FMath::Max(0.0f, TerrainRockSlopeThreshold);
    TerrainSurfaceMaterialId = FMath::Max(1, TerrainSurfaceMaterialId);
    TerrainSubsurfaceMaterialId = FMath::Max(1, TerrainSubsurfaceMaterialId);
    TerrainWaterMaterialId = FMath::Max(1, TerrainWaterMaterialId);

    InitialLoadRadius = FMath::Max(0, InitialLoadRadius);
    HorizontalViewRadius = FMath::Max(InitialLoadRadius, HorizontalViewRadius);
    VerticalViewRadius = FMath::Max(1, VerticalViewRadius);
    MaxChunksGeneratedPerTick = FMath::Max(1, MaxChunksGeneratedPerTick);
    MaxChunksRemovedPerTick = FMath::Max(1, MaxChunksRemovedPerTick);
    MaxDirtyChunksRebuiltPerTick = FMath::Max(1, MaxDirtyChunksRebuiltPerTick);
    StreamingUpdateInterval = FMath::Max(0.05f, StreamingUpdateInterval);
    WeatherMaterialUpdateInterval =
        FMath::Max(0.01f, WeatherMaterialUpdateInterval);
    WeatherWettingRate = FMath::Max(0.0f, WeatherWettingRate);
    WeatherDryingRate = FMath::Max(0.0f, WeatherDryingRate);
    WeatherWetDarkening = FMath::Clamp(WeatherWetDarkening, 0.0f, 1.0f);
    WeatherWetRoughness = FMath::Clamp(WeatherWetRoughness, 0.0f, 1.0f);
    WeatherRainIntensityOverride =
        FMath::Clamp(WeatherRainIntensityOverride, 0.0f, 1.0f);

    RefreshChunkRegistry();
}

void ACubusBlockWorldActor::BeginPlay()
{
    Super::BeginPlay();

    if (bConnectToSpacetimeDB)
    {
        if (UOrakaiPersistenceSubsystem* Persistence =
                UOrakaiPersistenceSubsystem::Get(this))
        {
            Persistence->ConnectToSpacetimeDB(
                SpacetimeServerUri,
                SpacetimeDatabaseName,
                SpacetimeTokenFilePath
            );
        }
    }

    PublishWorldConfig();
    RestoreDensityEdits();

    CurrentMaterialWetness = 0.0f;
    CurrentWeatherRainIntensity = 0.0f;
    TimeUntilWeatherMaterialUpdate = 0.0f;
    WeatherMaterialElapsedSeconds = 0.0f;
    if (IsValid(MaterialRegistry))
    {
        MaterialRegistry->SetWeatherMaterialState(
            CurrentMaterialWetness,
            WeatherWetDarkening,
            WeatherWetRoughness
        );
    }

    EnsureWorldVegetationActor();

    // Level-authored fixed chunks and editor-generated chunks need the same
    // baseline -> persisted deltas -> mesh load path as streamed chunks.
    RefreshChunkRegistry();
    for (const TPair<FIntVector, TWeakObjectPtr<ACubusVoxelVolumeActor>>& Pair
         : ChunksByCoordinate)
    {
        if (ACubusVoxelVolumeActor* Chunk = Pair.Value.Get())
        {
            ApplyPersistedEditsToChunk(*Chunk);
            Chunk->RebuildVolume();
        }
    }

    if (!bEnableRuntimeStreaming)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus runtime streaming settings: initial=%d horizontal=%d vertical=%d genPerTick=%d interval=%.2fs"),
        InitialLoadRadius,
        HorizontalViewRadius,
        VerticalViewRadius,
        MaxChunksGeneratedPerTick,
        StreamingUpdateInterval
    );

    APawn* PlayerPawn =
        UGameplayStatics::GetPlayerPawn(this, 0);

    if (IsValid(PlayerPawn))
    {
        TrackedPawn = PlayerPawn;
        HeldPawnLocation = PlayerPawn->GetActorLocation();

        if (bHoldPawnUntilInitialAreaReady)
        {
            HoldPawnForInitialStreaming();
        }
    }

    UpdateRuntimeStreaming(true);
}

void ACubusBlockWorldActor::EnsureWorldVegetationActor()
{
    if (!bEnableWorldVegetation)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    ACubusWorldVegetationActor* VegetationActor = WorldVegetationActor.Get();

    if (!IsValid(VegetationActor))
    {
        for (TActorIterator<ACubusWorldVegetationActor> Iterator(World); Iterator; ++Iterator)
        {
            if (IsValid(*Iterator))
            {
                VegetationActor = *Iterator;
                break;
            }
        }
    }

    if (!IsValid(VegetationActor))
    {
        TSubclassOf<ACubusWorldVegetationActor> ResolvedClass =
            WorldVegetationActorClass;

        if (!ResolvedClass)
        {
            ResolvedClass = ACubusWorldVegetationActor::StaticClass();
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = this;
        SpawnParameters.OverrideLevel = GetLevel();
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        if (World->IsGameWorld())
        {
            SpawnParameters.ObjectFlags |= RF_Transient;
        }

        VegetationActor =
            World->SpawnActor<ACubusWorldVegetationActor>(
                ResolvedClass,
                GetActorLocation(),
                FRotator::ZeroRotator,
                SpawnParameters
            );

        if (IsValid(VegetationActor))
        {
            UE_LOG(LogTemp, Display, TEXT("Cubus spawned world vegetation actor: %s"), *VegetationActor->GetName());
        }
    }

    if (IsValid(VegetationActor))
    {
        WorldVegetationActor = VegetationActor;
        VegetationActor->ConfigureForWorld(this);
    }
}

void ACubusBlockWorldActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateWeatherMaterials(DeltaSeconds);

    /*
    * Density edits publish as one visual transaction.
    *
    * This must run before the ordinary dirty queue so a chunk participating
    * in an edit cannot be independently rebuilt immediately before the
    * transaction.
    */
    if (
        bAtomicDensityBatchActive ||
        !AtomicDensityDirtyChunkCoordinates.IsEmpty()
    )
    {
        ProcessAtomicDensityEditBatch();
    }

    // Editing is valid in both fixed-grid and streamed worlds.
    if (!DirtyChunkCoordinates.IsEmpty())
    {
        ProcessDirtyChunkQueue();
    }

    if (!bEnableRuntimeStreaming)
    {
        return;
    }

    APawn* PlayerPawn = TrackedPawn.Get();

    if (!IsValid(PlayerPawn))
    {
        PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

        if (IsValid(PlayerPawn))
        {
            TrackedPawn = PlayerPawn;
            HeldPawnLocation = PlayerPawn->GetActorLocation();

            if (
                bHoldPawnUntilInitialAreaReady &&
                !bInitialSpawnAreaReady
            )
            {
                HoldPawnForInitialStreaming();
            }
        }
    }

    if (bPawnHeldForStreaming && IsValid(PlayerPawn))
    {
        HeldPawnElapsedSeconds += DeltaSeconds;

        PlayerPawn->SetActorLocation(
            HeldPawnLocation,
            false,
            nullptr,
            ETeleportType::TeleportPhysics
        );
    }

    TimeUntilStreamingUpdate -= DeltaSeconds;

    if (TimeUntilStreamingUpdate <= 0.0f)
    {
        TimeUntilStreamingUpdate = StreamingUpdateInterval;
        UpdateRuntimeStreaming(false);
    }

    if (
        !PendingChunkGeneration.IsEmpty() ||
        !PendingChunkRemoval.IsEmpty()
    )
    {
        ProcessRuntimeQueues();
    }

    TryReleasePawnToTerrain();

    RecordTrackedPawnCoordinate();
}

void ACubusBlockWorldActor::UpdateWeatherMaterials(const float DeltaSeconds)
{
    if (!bEnableWeatherMaterialBridge || !IsValid(MaterialRegistry))
    {
        bWeatherMaterialBridgeConnected = false;
        return;
    }

    WeatherMaterialElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
    TimeUntilWeatherMaterialUpdate -= DeltaSeconds;

    if (TimeUntilWeatherMaterialUpdate > 0.0f)
    {
        return;
    }

    TimeUntilWeatherMaterialUpdate =
        FMath::Max(0.01f, WeatherMaterialUpdateInterval);
    const float MaterialDeltaSeconds = WeatherMaterialElapsedSeconds;
    WeatherMaterialElapsedSeconds = 0.0f;

    if (!IsValid(CachedWeatherActor))
    {
        CachedWeatherActor =
            FCubusWeatherMaterialUtilities::ResolveWeatherActor(GetWorld());
    }

    FCubusWeatherMaterialSample WeatherSample;
    if (IsValid(CachedWeatherActor))
    {
        WeatherSample = FCubusWeatherMaterialUtilities::Sample(
            GetWorld(),
            CachedWeatherActor
        );
    }

    if (bOverrideWeatherRainIntensity)
    {
        WeatherSample.bHasSurfaceWetness = false;
        WeatherSample.bHasRainIntensity = true;
        WeatherSample.RainIntensity =
            FMath::Clamp(WeatherRainIntensityOverride, 0.0f, 1.0f);
    }

    bWeatherMaterialBridgeConnected =
        bOverrideWeatherRainIntensity ||
        (
            IsValid(CachedWeatherActor) &&
            (WeatherSample.bHasRainIntensity || WeatherSample.bHasSurfaceWetness)
        );

    CurrentWeatherRainIntensity = WeatherSample.bHasRainIntensity
        ? FMath::Clamp(WeatherSample.RainIntensity, 0.0f, 1.0f)
        : 0.0f;

    CurrentMaterialWetness =
        FCubusWeatherMaterialUtilities::AdvanceWetness(
            CurrentMaterialWetness,
            WeatherSample,
            MaterialDeltaSeconds,
            WeatherWettingRate,
            WeatherDryingRate
        );

    MaterialRegistry->SetWeatherMaterialState(
        CurrentMaterialWetness,
        WeatherWetDarkening,
        WeatherWetRoughness
    );
}

void ACubusBlockWorldActor::RegisterChunk(
    ACubusVoxelVolumeActor* ChunkActor
)
{
    if (!IsValid(ChunkActor))
    {
        return;
    }

    RemoveInvalidChunks();

    for (auto Iterator = ChunksByCoordinate.CreateIterator(); Iterator; ++Iterator)
    {
        if (Iterator.Value().Get() == ChunkActor)
        {
            Iterator.RemoveCurrent();
        }
    }

    const FIntVector Coordinate = ChunkActor->GetChunkCoordinate();
    ChunksByCoordinate.Add(Coordinate, ChunkActor);
    RegisteredChunkCount = ChunksByCoordinate.Num();

    // A newly available block neighbour changes which boundary faces should
    // exist. Density chunks are independent, but sharing this cheap queue keeps
    // hybrid mode border-safe as well.
    for (const FIntVector& Offset : CubusBlockWorldActor::NeighbourOffsets)
    {
        QueueChunkForRebuild(Coordinate + Offset);
    }
}

void ACubusBlockWorldActor::UnregisterChunk(
    ACubusVoxelVolumeActor* ChunkActor
)
{
    if (ChunkActor == nullptr)
    {
        return;
    }

    TArray<FIntVector> RemovedCoordinates;

    for (auto Iterator = ChunksByCoordinate.CreateIterator(); Iterator; ++Iterator)
    {
        if (Iterator.Value().Get() == ChunkActor)
        {
            RemovedCoordinates.Add(Iterator.Key());
            Iterator.RemoveCurrent();
        }
    }

    RegisteredChunkCount = ChunksByCoordinate.Num();

    // When a block chunk disappears, adjacent chunks must expose their border
    // faces. Queue after removal so their neighbourhood snapshot sees null.
    for (const FIntVector& Coordinate : RemovedCoordinates)
    {
        for (const FIntVector& Offset : CubusBlockWorldActor::NeighbourOffsets)
        {
            QueueChunkForRebuild(Coordinate + Offset);
        }
    }
}

ACubusVoxelVolumeActor* ACubusBlockWorldActor::FindChunk(
    const FIntVector& ChunkCoordinate
) const
{
    const TWeakObjectPtr<ACubusVoxelVolumeActor>* FoundChunk =
        ChunksByCoordinate.Find(ChunkCoordinate);

    if (FoundChunk == nullptr)
    {
        return nullptr;
    }

    ACubusVoxelVolumeActor* ChunkActor = FoundChunk->Get();
    return IsValid(ChunkActor) ? ChunkActor : nullptr;
}

void ACubusBlockWorldActor::RebuildChunkAtCoordinate(
    const FIntVector& ChunkCoordinate
)
{
    ACubusVoxelVolumeActor* ChunkActor = FindChunk(ChunkCoordinate);

    if (IsValid(ChunkActor))
    {
        ChunkActor->RebuildVolume();
    }
}

void ACubusBlockWorldActor::RebuildChunkAndNeighbours(
    const FIntVector& ChunkCoordinate
)
{
    QueueChunkAndFaceNeighboursForRebuild(ChunkCoordinate);
}

void ACubusBlockWorldActor::QueueChunkForRebuild(
    const FIntVector& ChunkCoordinate
)
{
    DirtyChunkCoordinates.Add(ChunkCoordinate);
}

void ACubusBlockWorldActor::QueueChunkAndFaceNeighboursForRebuild(
    const FIntVector& ChunkCoordinate
)
{
    QueueChunkForRebuild(ChunkCoordinate);

    for (const FIntVector& Offset : CubusBlockWorldActor::NeighbourOffsets)
    {
        QueueChunkForRebuild(ChunkCoordinate + Offset);
    }
}

void ACubusBlockWorldActor::QueueDensityEditDependenciesForRebuild(
    const FIntVector& ChunkCoordinate
)
{
    ++DensityEditRevision;

    /*
     * Density edit dependencies are transactional.
     *
     * Do not send these through the ordinary dirty queue because that
     * publishes chunks individually and can expose different density
     * revisions on opposite sides of a shared boundary.
     */
    for (int32 Z = -1; Z <= 1; ++Z)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            for (int32 X = -1; X <= 1; ++X)
            {
                const FIntVector DependencyCoordinate =
                    ChunkCoordinate +
                    FIntVector(X, Y, Z);

                /*
                 * Unloaded chunks need no transaction. When streamed in later
                 * they will build directly from the current authoritative
                 * DensityEdits map.
                 */
                if (!IsValid(FindChunk(DependencyCoordinate)))
                {
                    continue;
                }

                AtomicDensityDirtyChunkCoordinates.Add(
                    DependencyCoordinate
                );
            }
        }
    }
}

FCubusDensityEditMap
ACubusBlockWorldActor::BuildDensityEditSnapshot(
    const FIntVector& ChunkCoordinate
) const
{
    FCubusDensityEditMap Snapshot;

    /*
    * Adaptive density gradients sample one fine step beyond the chunk.
    * SampleContinuous() then evaluates a four-point cubic B-spline,
    * requiring one additional integer edit sample beyond that.
    *
    * For a 32-voxel chunk this means the edit snapshot must cover
    * local integer coordinates -2..34, not merely the density
    * buffer's -1..33 range.
    */
    constexpr int32 DensityEditInterpolationHalo = 1;

    const int32 MinimumEditSample =
        FCubusDensitySamplingBuffer::MinimumLocalSample -
        DensityEditInterpolationHalo;

    const int32 MaximumEditSample =
        FCubusDensitySamplingBuffer::MaximumLocalSample +
        DensityEditInterpolationHalo;

    const FIntVector SampleMinimum =
        ChunkCoordinate * Cubus::ChunkSize +
        FIntVector(
            MinimumEditSample,
            MinimumEditSample,
            MinimumEditSample
        );

    const FIntVector SampleMaximum =
        ChunkCoordinate * Cubus::ChunkSize +
        FIntVector(
            MaximumEditSample,
            MaximumEditSample,
            MaximumEditSample
        );

    /*
    * The interpolation halo extends two integer samples beyond
    * the owned chunk, which still lies entirely within the
    * immediately adjacent 26 chunk buckets.
    */
    for (int32 Z = -1; Z <= 1; ++Z)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            for (int32 X = -1; X <= 1; ++X)
            {
                const FIntVector EditChunk =
                    ChunkCoordinate +
                    FIntVector(X, Y, Z);

                const FCubusDensityEditMap*
                    ChunkEdits =
                        DensityEditsByChunk.Find(
                            EditChunk
                        );

                if (ChunkEdits == nullptr)
                {
                    continue;
                }

                for (
                    const TPair<
                        FIntVector,
                        FCubusDensityEdit
                    >& Entry
                    : *ChunkEdits
                )
                {
                    const FIntVector& Coordinate =
                        Entry.Key;

                    if (
                        Coordinate.X <
                            SampleMinimum.X ||
                        Coordinate.X >
                            SampleMaximum.X ||
                        Coordinate.Y <
                            SampleMinimum.Y ||
                        Coordinate.Y >
                            SampleMaximum.Y ||
                        Coordinate.Z <
                            SampleMinimum.Z ||
                        Coordinate.Z >
                            SampleMaximum.Z
                    )
                    {
                        continue;
                    }

                    Snapshot.Add(
                        Coordinate,
                        Entry.Value
                    );
                }
            }
        }
    }

    return Snapshot;
}

bool ACubusBlockWorldActor::BuildBlockEditOverlayChunk(
    const FIntVector& ChunkCoordinate,
    FCubusBlockChunkData& OutChunk
) const
{
    OutChunk.SetChunkCoordinate(ChunkCoordinate);
    OutChunk.Clear();

    const UOrakaiPersistenceSubsystem* Persistence =
        UOrakaiPersistenceSubsystem::Get(this);
    if (Persistence == nullptr)
    {
        return false;
    }

    TArray<FOrakaiVoxelEdit> StoredEdits;
    Persistence->GetVoxelEditsForChunk(ChunkCoordinate, StoredEdits);

    for (const FOrakaiVoxelEdit& Edit : StoredEdits)
    {
        if (Edit.MaterialId <= 0)
        {
            continue;
        }

        FCubusBlockVoxel Voxel;
        Voxel.MaterialId = FMath::Clamp(Edit.MaterialId, 1, 65535);
        Voxel.SetWater(Edit.bIsWater);
        OutChunk.SetVoxel(Edit.LocalCoordinate, Voxel);
    }

    return OutChunk.HasAnyOccupiedVoxel();
}

void ACubusBlockWorldActor::PublishWorldConfig()
{
    if (UOrakaiPersistenceSubsystem* Persistence =
            UOrakaiPersistenceSubsystem::Get(this))
    {
        Persistence->SetWorldConfig(
            WorldSeed,
            FCubusGenerationSeeds::CurrentGenerationVersion
        );
    }
}

void ACubusBlockWorldActor::RebuildDensityEditIndex()
{
    DensityEditsByChunk.Reset();

    for (
        const TPair<
            FIntVector,
            FCubusDensityEdit
        >& Pair : DensityEdits
    )
    {
        const FIntVector ChunkCoordinate =
            OrakaiPersistence::WorldVoxelToChunk(
                Pair.Key
            );

        DensityEditsByChunk
            .FindOrAdd(ChunkCoordinate)
            .Add(
                Pair.Key,
                Pair.Value
            );
    }
}

void ACubusBlockWorldActor::ReindexDensityEditSample(
    const FIntVector& WorldSample
)
{
    const FIntVector ChunkCoordinate =
        OrakaiPersistence::WorldVoxelToChunk(
            WorldSample
        );

    FCubusDensityEditMap* ChunkEdits =
        DensityEditsByChunk.Find(
            ChunkCoordinate
        );

    const FCubusDensityEdit* Edit =
        DensityEdits.Find(
            WorldSample
        );

    if (Edit == nullptr)
    {
        if (ChunkEdits != nullptr)
        {
            ChunkEdits->Remove(
                WorldSample
            );

            if (ChunkEdits->IsEmpty())
            {
                DensityEditsByChunk.Remove(
                    ChunkCoordinate
                );
            }
        }

        return;
    }

    DensityEditsByChunk
        .FindOrAdd(ChunkCoordinate)
        .Add(
            WorldSample,
            *Edit
        );
}

void ACubusBlockWorldActor::RestoreDensityEdits()
{
    DensityEdits.Reset();
    DensityEditsByChunk.Reset();

    UOrakaiPersistenceSubsystem* Persistence =
        UOrakaiPersistenceSubsystem::Get(this);

    if (Persistence == nullptr)
    {
        return;
    }

    TArray<FOrakaiDensityEdit> StoredEdits;
    Persistence->GetDensityEdits(StoredEdits);

    for (
        const FOrakaiDensityEdit& StoredEdit
        : StoredEdits
    )
    {
        if (
            FMath::IsNearlyZero(
                StoredEdit.DensityDelta
            )
        )
        {
            continue;
        }

        FCubusDensityEdit Edit;

        Edit.DensityDelta =
            StoredEdit.DensityDelta;

        Edit.MaterialId =
            StoredEdit.MaterialId;

        DensityEdits.Add(
            StoredEdit.WorldSample,
            Edit
        );
    }

    RebuildDensityEditIndex();
}

void ACubusBlockWorldActor::ApplyPersistedEditsToChunk(
    ACubusVoxelVolumeActor& ChunkActor
)
{
    UOrakaiPersistenceSubsystem* Persistence =
        UOrakaiPersistenceSubsystem::Get(this);
    FCubusBlockChunkData* ChunkData = ChunkActor.GetMutableChunkData();

    if (Persistence == nullptr || ChunkData == nullptr)
    {
        return;
    }

    const FIntVector ChunkCoordinate = ChunkActor.GetChunkCoordinate();
    TArray<FOrakaiVoxelEdit> VoxelEdits;
    Persistence->GetVoxelEditsForChunk(ChunkCoordinate, VoxelEdits);

    for (const FOrakaiVoxelEdit& Edit : VoxelEdits)
    {
        FCubusBlockVoxel Voxel;
        Voxel.MaterialId = FMath::Clamp(Edit.MaterialId, 0, 65535);
        Voxel.SetWater(Edit.bIsWater);
        ChunkData->SetVoxel(Edit.LocalCoordinate, Voxel);
    }

    TArray<FOrakaiFoliageEdit> FoliageEdits;
    Persistence->GetFoliageEditsForChunk(ChunkCoordinate, FoliageEdits);

    for (const FOrakaiFoliageEdit& Edit : FoliageEdits)
    {
        if (Edit.bRemoved)
        {
            ChunkData->RemoveVegetationAtWorldVoxel(Edit.WorldVoxel);
            continue;
        }

        FCubusVegetationInstance Instance;
        Instance.WorldVoxel = Edit.WorldVoxel;
        Instance.TypeId = Edit.TypeId;
        Instance.RotationYaw = Edit.RotationYaw;
        Instance.Scale = Edit.Scale;
        ChunkData->AddOrReplaceVegetationInstance(Instance);
    }
}

void ACubusBlockWorldActor::RecordTrackedPawnCoordinate()
{
    APawn* PlayerPawn = TrackedPawn.Get();

    if (!IsValid(PlayerPawn))
    {
        return;
    }

    UOrakaiPersistenceSubsystem* Persistence =
        UOrakaiPersistenceSubsystem::Get(this);

    if (Persistence == nullptr)
    {
        return;
    }

    const FVector Location = PlayerPawn->GetActorLocation();
    const FRotator ViewRotation = PlayerPawn->GetViewRotation();

    Persistence->RecordPlayerCoordinate(
        Location,
        static_cast<float>(ViewRotation.Yaw),
        static_cast<float>(ViewRotation.Pitch)
    );
}

bool ACubusBlockWorldActor::EditVoxelAtWorldVoxel(
    const FIntVector WorldVoxel,
    const int32 MaterialId,
    const bool bIsWater
)
{
    return EditBlockSphereAtWorldVoxel(
        WorldVoxel,
        0,
        MaterialId,
        bIsWater
    ) > 0;
}

int32 ACubusBlockWorldActor::EditBlockSphereAtWorldVoxel(
    const FIntVector CentreWorldVoxel,
    const int32 BrushRadius,
    const int32 MaterialId,
    const bool bIsWater
)
{
    if (MaterialId < 0)
    {
        return 0;
    }

    const int32 SafeRadius = FMath::Max(0, BrushRadius);
    const int32 RadiusSquared = SafeRadius * SafeRadius;
    TSet<FIntVector> TouchedChunks;
    int32 ChangedVoxelCount = 0;

    UOrakaiPersistenceSubsystem* Persistence =
        UOrakaiPersistenceSubsystem::Get(this);

    for (int32 Z = -SafeRadius; Z <= SafeRadius; ++Z)
    {
        for (int32 Y = -SafeRadius; Y <= SafeRadius; ++Y)
        {
            for (int32 X = -SafeRadius; X <= SafeRadius; ++X)
            {
                if (X * X + Y * Y + Z * Z > RadiusSquared)
                {
                    continue;
                }

                const FIntVector WorldVoxel =
                    CentreWorldVoxel + FIntVector(X, Y, Z);

                const FIntVector ChunkCoordinate =
                    OrakaiPersistence::WorldVoxelToChunk(WorldVoxel);

                const FIntVector LocalCoordinate =
                    WorldVoxel - ChunkCoordinate * Cubus::ChunkSize;

                ACubusVoxelVolumeActor* Chunk =
                    FindChunk(ChunkCoordinate);

                if (!IsValid(Chunk))
                {
                    continue;
                }

                FCubusBlockChunkData* Data =
                    Chunk->GetMutableChunkData();

                if (Data == nullptr)
                {
                    continue;
                }

                FCubusBlockVoxel Voxel;
                Voxel.MaterialId = MaterialId;
                Voxel.SetWater(bIsWater);

                const FCubusBlockVoxel* Existing =
                    Data->GetVoxel(LocalCoordinate);

                if (Existing == nullptr || *Existing == Voxel)
                {
                    continue;
                }

                if (!Data->SetVoxel(LocalCoordinate, Voxel))
                {
                    continue;
                }

                TouchedChunks.Add(ChunkCoordinate);
                ++ChangedVoxelCount;

                if (Persistence != nullptr)
                {
                    Persistence->RecordVoxelEdit(
                        ChunkCoordinate,
                        LocalCoordinate,
                        MaterialId,
                        bIsWater
                    );
                }
            }
        }
    }

    for (const FIntVector& ChunkCoordinate : TouchedChunks)
    {
        QueueChunkAndFaceNeighboursForRebuild(ChunkCoordinate);
    }

    return ChangedVoxelCount;
}

int32 ACubusBlockWorldActor::EditDensitySphereAtWorldSample(
    const FIntVector CentreWorldSample,
    const int32 BrushRadius,
    const float DensityDelta,
    const int32 MaterialId
)
{
    if (FMath::IsNearlyZero(DensityDelta))
    {
        return 0;
    }

    const int32 SafeRadius = FMath::Max(0, BrushRadius);
    const int32 RadiusSquared = SafeRadius * SafeRadius;
    TSet<FIntVector> TouchedChunks;
    int32 ChangedSampleCount = 0;

    for (int32 Z = -SafeRadius; Z <= SafeRadius; ++Z)
    {
        for (int32 Y = -SafeRadius; Y <= SafeRadius; ++Y)
        {
            for (int32 X = -SafeRadius; X <= SafeRadius; ++X)
            {
                if (X * X + Y * Y + Z * Z > RadiusSquared)
                {
                    continue;
                }

                const FIntVector WorldSample =
                    CentreWorldSample + FIntVector(X, Y, Z);

                FCubusDensityEdit& Edit =
                    DensityEdits.FindOrAdd(WorldSample);

                Edit.DensityDelta += DensityDelta;

                if (DensityDelta > 0.0f && MaterialId > 0)
                {
                    Edit.MaterialId = MaterialId;
                }

                if (FMath::IsNearlyZero(Edit.DensityDelta))
                {
                    DensityEdits.Remove(
                        WorldSample
                    );

                    ReindexDensityEditSample(
                        WorldSample
                    );

                    if (
                        UOrakaiPersistenceSubsystem*
                            Persistence =
                                UOrakaiPersistenceSubsystem::Get(
                                    this
                                )
                    )
                    {
                        Persistence->ClearDensityEdit(
                            WorldSample
                        );
                    }
                }
                else
                {
                    ReindexDensityEditSample(
                        WorldSample
                    );

                    if (
                        UOrakaiPersistenceSubsystem*
                            Persistence =
                                UOrakaiPersistenceSubsystem::Get(
                                    this
                                )
                    )
                    {
                        Persistence->RecordDensityEdit(
                            WorldSample,
                            Edit.DensityDelta,
                            Edit.MaterialId
                        );
                    }
                }

                TouchedChunks.Add(
                    OrakaiPersistence::WorldVoxelToChunk(WorldSample)
                );
                ++ChangedSampleCount;
            }
        }
    }

    for (const FIntVector& ChunkCoordinate : TouchedChunks)
    {
        QueueDensityEditDependenciesForRebuild(ChunkCoordinate);
    }

    return ChangedSampleCount;
}

bool ACubusBlockWorldActor::ClearVoxelEditAtWorldVoxel(const FIntVector WorldVoxel)
{
    const FIntVector ChunkCoordinate =
        OrakaiPersistence::WorldVoxelToChunk(WorldVoxel);
    const FIntVector LocalCoordinate =
        WorldVoxel - ChunkCoordinate * Cubus::ChunkSize;

    if (UOrakaiPersistenceSubsystem* Persistence =
            UOrakaiPersistenceSubsystem::Get(this))
    {
        Persistence->ClearVoxelEdit(ChunkCoordinate, LocalCoordinate);
        return true;
    }

    return false;
}

void ACubusBlockWorldActor::RecordFoliageEditAtWorldVoxel(
    const FIntVector WorldVoxel,
    const int32 TypeId,
    const float RotationYaw,
    const float Scale
)
{
    if (UOrakaiPersistenceSubsystem* Persistence =
            UOrakaiPersistenceSubsystem::Get(this))
    {
        Persistence->RecordFoliageEdit(
            WorldVoxel,
            /*bRemoved*/ false,
            TypeId,
            RotationYaw,
            Scale
        );
    }
}

void ACubusBlockWorldActor::RemoveFoliageAtWorldVoxel(const FIntVector WorldVoxel)
{
    if (UOrakaiPersistenceSubsystem* Persistence =
            UOrakaiPersistenceSubsystem::Get(this))
    {
        Persistence->RecordFoliageEdit(
            WorldVoxel,
            /*bRemoved*/ true,
            0,
            0.0f,
            1.0f
        );
    }

    const FIntVector ChunkCoordinate =
        OrakaiPersistence::WorldVoxelToChunk(WorldVoxel);

    if (ACubusVoxelVolumeActor* Chunk = FindChunk(ChunkCoordinate))
    {
        if (FCubusBlockChunkData* Data = Chunk->GetMutableChunkData())
        {
            Data->RemoveVegetationAtWorldVoxel(WorldVoxel);
        }
    }

    if (ACubusWorldVegetationActor* VegetationActor =
            WorldVegetationActor.Get())
    {
        VegetationActor->RebuildWorldVegetation();
    }
}

void ACubusBlockWorldActor::RecordGeneratedTreeTombstone(
    const FIntVector& WorldVoxel
)
{
    if (UOrakaiPersistenceSubsystem* Persistence =
            UOrakaiPersistenceSubsystem::Get(this))
    {
        // Trees remain rendered by the existing foliage system. This parallel
        // object tombstone gives the same generated tree a stable identity for
        // later replication, loot, regrowth and object-specific state.
        Persistence->TombstoneGeneratedWorldObject(
            WorldSeed,
            TEXT("Tree"),
            WorldVoxel,
            OrakaiPersistence::WorldVoxelToChunk(WorldVoxel),
            FTransform::Identity
        );
    }
}

bool ACubusBlockWorldActor::HarvestTreeAlongRay(
    const FVector TraceStart,
    const FVector TraceEnd,
    const float SelectionRadius,
    FIntVector& OutTreeWorldVoxel
)
{
    OutTreeWorldVoxel = FIntVector::ZeroValue;

    if (ACubusWorldVegetationActor* VegetationActor =
            WorldVegetationActor.Get())
    {
        if (
            !VegetationActor->FindInteractiveTreeAlongRay(
                TraceStart,
                TraceEnd,
                SelectionRadius,
                OutTreeWorldVoxel
            )
        )
        {
            return false;
        }

        RecordGeneratedTreeTombstone(
            OutTreeWorldVoxel
        );

        RemoveFoliageAtWorldVoxel(
            OutTreeWorldVoxel
        );

        return true;
    }

    /*
    * Legacy fallback for worlds where the world vegetation actor
    * is deliberately unavailable.
    */
    const FVector Segment =
        TraceEnd - TraceStart;
    const double SegmentLengthSquared = Segment.SizeSquared();

    if (SegmentLengthSquared <= static_cast<double>(SMALL_NUMBER))
    {
        return false;
    }

    const double SafeRadiusSquared =
        FMath::Square(FMath::Max(1.0f, SelectionRadius));
    double BestAlongSegment = TNumericLimits<double>::Max();
    bool bFound = false;

    for (const TPair<FIntVector, TWeakObjectPtr<ACubusVoxelVolumeActor>>& Pair
         : ChunksByCoordinate)
    {
        const ACubusVoxelVolumeActor* Chunk = Pair.Value.Get();
        if (!IsValid(Chunk) || Chunk->GetChunkData() == nullptr)
        {
            continue;
        }

        const float SafeVoxelSize = FMath::Max(1.0f, Chunk->GetVoxelSize());
        const double ChunkHalfWorldExtent =
            static_cast<double>(Cubus::ChunkSize) * SafeVoxelSize * 0.5;

        for (const FCubusVegetationInstance& Instance
             : Chunk->GetChunkData()->GetVegetationInstances())
        {
            if (Instance.TypeId != 3 && Instance.TypeId != 6)
            {
                continue;
            }

            const FVector TreeLocation(
                (static_cast<double>(Instance.WorldVoxel.X) + 0.5) * SafeVoxelSize -
                    ChunkHalfWorldExtent,
                (static_cast<double>(Instance.WorldVoxel.Y) + 0.5) * SafeVoxelSize -
                    ChunkHalfWorldExtent,
                static_cast<double>(Instance.WorldVoxel.Z) * SafeVoxelSize -
                    ChunkHalfWorldExtent + SafeVoxelSize
            );

            const double Along = FVector::DotProduct(
                TreeLocation - TraceStart,
                Segment
            ) / SegmentLengthSquared;

            if (Along < 0.0 || Along > 1.0)
            {
                continue;
            }

            const FVector ClosestPoint = TraceStart + Segment * Along;
            if (FVector::DistSquared(TreeLocation, ClosestPoint) > SafeRadiusSquared)
            {
                continue;
            }

            if (Along < BestAlongSegment)
            {
                BestAlongSegment = Along;
                OutTreeWorldVoxel = Instance.WorldVoxel;
                bFound = true;
            }
        }
    }

    if (!bFound)
    {
        return false;
    }

    RecordGeneratedTreeTombstone(OutTreeWorldVoxel);
    RemoveFoliageAtWorldVoxel(OutTreeWorldVoxel);
    return true;
}

ACubusVoxelVolumeActor* ACubusBlockWorldActor::SpawnChunkAtCoordinate(
    const FIntVector& Coordinate,
    const bool bGenerateVegetation
)
{
    if (ACubusVoxelVolumeActor* ExistingChunk = FindChunk(Coordinate))
    {
        return ExistingChunk;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return nullptr;
    }

    TSubclassOf<ACubusVoxelVolumeActor> ResolvedChunkClass = ChunkActorClass;

    if (!ResolvedChunkClass)
    {
        ResolvedChunkClass = ACubusVoxelVolumeActor::StaticClass();
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.OverrideLevel = GetLevel();
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (World->IsGameWorld())
    {
        SpawnParameters.ObjectFlags |= RF_Transient;
    }

    const double ChunkWorldSize =
        static_cast<double>(Cubus::ChunkSize) *
        static_cast<double>(FMath::Max(1.0f, GeneratedVoxelSize));

    const FVector SpawnLocation(
        static_cast<double>(Coordinate.X) * ChunkWorldSize,
        static_cast<double>(Coordinate.Y) * ChunkWorldSize,
        static_cast<double>(Coordinate.Z) * ChunkWorldSize
    );

    ACubusVoxelVolumeActor* ChunkActor =
        World->SpawnActor<ACubusVoxelVolumeActor>(
            ResolvedChunkClass,
            SpawnLocation,
            FRotator::ZeroRotator,
            SpawnParameters
        );

    if (!IsValid(ChunkActor))
    {
        return nullptr;
    }

    if (World->IsGameWorld())
    {
        ChunkActor->SetFlags(RF_Transient);
        ChunkActor->ClearFlags(RF_Transactional);
    }

    GeneratedChunks.Add(ChunkActor);

    ChunkActor->ConfigureGeneratedChunk(
        Coordinate,
        GeneratedVoxelSize,
        this
    );

    ChunkActor->ConfigureDensityResolution(
        ResolveDensitySubdivisions(Coordinate)
    );

    ChunkActor->ConfigureRendering(
        MaterialRegistry
    );

    ChunkActor->ConfigureGeology(GeologyProfile);

    ChunkActor->ConfigureTerrain(
        bUseHeightTerrain,
        TerrainSurfaceWorldZ,
        TerrainBaseHeight,
        TerrainContinentAmplitude,
        TerrainContinentFrequency,
        TerrainHillAmplitude,
        TerrainHillFrequency,
        TerrainDetailAmplitude,
        TerrainDetailFrequency,
        TerrainRidgeAmplitude,
        TerrainRidgeFrequency,
        TerrainValleyDepth,
        TerrainValleyFrequency,
        TerrainValleyWidth,
        TerrainValleyFalloff,
        TerrainValleyWarpAmplitude,
        TerrainValleyWarpFrequency,
        TerrainRegionFrequency,
        TerrainPlainsThreshold,
        TerrainPlainsBlend,
        TerrainMountainThreshold,
        TerrainMountainBlend,
        TerrainSurfaceMaterialId,
        TerrainSubsurfaceMaterialId,
        TerrainRockMaterialId,
        TerrainSnowMaterialId,
        TerrainRockSlopeThreshold,
        TerrainSnowMinimumHeight,
        bGenerateWater,
        TerrainWaterLevel,
        TerrainWaterMaterialId
    );

    ChunkActor->SetOwner(this);
    ChunkActor->AttachToComponent(
        WorldRoot,
        FAttachmentTransformRules::KeepWorldTransform
    );

    RegisterChunk(ChunkActor);

    ChunkActor->GenerateTerrainData();
    ApplyPersistedEditsToChunk(*ChunkActor);
    ChunkActor->RebuildVolume();

    GeneratedChunkCount = GeneratedChunks.Num();
    return ChunkActor;
}

void ACubusBlockWorldActor::GenerateChunkGrid()
{
    ClearGeneratedChunks();

    GridDimensions.X =
        FMath::Max(1, GridDimensions.X);

    GridDimensions.Y =
        FMath::Max(1, GridDimensions.Y);

    GridDimensions.Z =
        FMath::Max(1, GridDimensions.Z);

    for (
        int32 Z = 0;
        Z < GridDimensions.Z;
        ++Z
    )
    {
        for (
            int32 Y = 0;
            Y < GridDimensions.Y;
            ++Y
        )
        {
            for (
                int32 X = 0;
                X < GridDimensions.X;
                ++X
            )
            {
                SpawnChunkAtCoordinate(
                    GridOrigin +
                        FIntVector(X, Y, Z),
                    true
                );
            }
        }
    }
}

void ACubusBlockWorldActor::ClearGeneratedChunks()
{
    for (ACubusVoxelVolumeActor* ChunkActor : GeneratedChunks)
    {
        if (!IsValid(ChunkActor))
        {
            continue;
        }

        UnregisterChunk(ChunkActor);
        ChunkActor->Destroy();
    }

    GeneratedChunks.Reset();
    PendingChunkGeneration.Reset();
    PendingChunkRemoval.Reset();
    DirtyChunkCoordinates.Reset();
    RequiredChunkCoordinates.Reset();
    InitialRequiredCoordinates.Reset();

    GeneratedChunkCount = 0;
    PendingRuntimeChunkCount = 0;
    bInitialSpawnAreaReady = false;

    RefreshChunkRegistry();
}

void ACubusBlockWorldActor::RefreshChunkRegistry()
{
    ChunksByCoordinate.Reset();

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        RegisteredChunkCount = 0;
        return;
    }

    for (
        TActorIterator<ACubusVoxelVolumeActor> Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        ACubusVoxelVolumeActor* ChunkActor =
            *Iterator;

        if (!IsValid(ChunkActor))
        {
            continue;
        }

        const bool bOwnedByThisWorld =
            ChunkActor->GetOwner() == this;

        const bool bAttachedToThisWorld =
            ChunkActor->GetAttachParentActor() == this;

        if (
            !bOwnedByThisWorld &&
            !bAttachedToThisWorld
        )
        {
            continue;
        }

        ChunkActor->SetOwningBlockWorld(this);

        ChunksByCoordinate.Add(
            ChunkActor->GetChunkCoordinate(),
            ChunkActor
        );
    }

    RemoveInvalidChunks();

    RegisteredChunkCount =
        ChunksByCoordinate.Num();
}

void ACubusBlockWorldActor::RebuildAllChunks()
{
    RefreshChunkRegistry();

    for (const auto& Entry : ChunksByCoordinate)
    {
        ACubusVoxelVolumeActor* ChunkActor = Entry.Value.Get();

        if (IsValid(ChunkActor))
        {
            ChunkActor->RebuildVolume();
        }
    }
}

void ACubusBlockWorldActor::RegenerateTerrain()
{
    RefreshChunkRegistry();

    for (const auto& Entry : ChunksByCoordinate)
    {
        ACubusVoxelVolumeActor* ChunkActor = Entry.Value.Get();

        if (!IsValid(ChunkActor))
        {
            continue;
        }

        ChunkActor->ConfigureRendering(MaterialRegistry);
        ChunkActor->ConfigureGeology(GeologyProfile);
        ChunkActor->ConfigureTerrain(
            bUseHeightTerrain,
            TerrainSurfaceWorldZ,
            TerrainBaseHeight,
            TerrainContinentAmplitude,
            TerrainContinentFrequency,
            TerrainHillAmplitude,
            TerrainHillFrequency,
            TerrainDetailAmplitude,
            TerrainDetailFrequency,
            TerrainRidgeAmplitude,
            TerrainRidgeFrequency,
            TerrainValleyDepth,
            TerrainValleyFrequency,
            TerrainValleyWidth,
            TerrainValleyFalloff,
            TerrainValleyWarpAmplitude,
            TerrainValleyWarpFrequency,
            TerrainRegionFrequency,
            TerrainPlainsThreshold,
            TerrainPlainsBlend,
            TerrainMountainThreshold,
            TerrainMountainBlend,
            TerrainSurfaceMaterialId,
            TerrainSubsurfaceMaterialId,
            TerrainRockMaterialId,
            TerrainSnowMaterialId,
            TerrainRockSlopeThreshold,
            TerrainSnowMinimumHeight,
            bGenerateWater,
            TerrainWaterLevel,
            TerrainWaterMaterialId
        );

        ChunkActor->GenerateTerrainData();
        ChunkActor->RebuildVolume();
    }
}

void ACubusBlockWorldActor::BuildRequiredCoordinates(
    const FIntVector& CentreCoordinate,
    const int32 HorizontalRadius,
    const int32 VerticalRadius,
    TSet<FIntVector>& OutCoordinates
) const
{
    OutCoordinates.Reset();

    const int32 SafeHorizontalRadius =
        FMath::Max(
            0,
            HorizontalRadius
        );

    const int32 SafeVerticalRadius =
        FMath::Max(
            0,
            VerticalRadius
        );

    /*
     * Radius + 0.5 treats the outer chunk cells as volumes rather than
     * zero-size points.
     *
     * That keeps the requested edge chunks while removing the large number
     * of useless cube-corner chunks produced by the old rectangular volume.
     */
    const double HorizontalExtent =
        static_cast<double>(
            SafeHorizontalRadius
        ) +
        0.5;

    const double VerticalExtent =
        static_cast<double>(
            SafeVerticalRadius
        ) +
        0.5;

    const double InverseHorizontalExtentSquared =
        HorizontalExtent > 0.0
            ? 1.0 /
                (
                    HorizontalExtent *
                    HorizontalExtent
                )
            : 0.0;

    const double InverseVerticalExtentSquared =
        VerticalExtent > 0.0
            ? 1.0 /
                (
                    VerticalExtent *
                    VerticalExtent
                )
            : 0.0;

    for (
        int32 Z = -SafeVerticalRadius;
        Z <= SafeVerticalRadius;
        ++Z
    )
    {
        const double NormalizedVerticalDistanceSquared =
            SafeVerticalRadius > 0
                ? static_cast<double>(
                    Z * Z
                ) *
                    InverseVerticalExtentSquared
                : 0.0;

        for (
            int32 Y = -SafeHorizontalRadius;
            Y <= SafeHorizontalRadius;
            ++Y
        )
        {
            for (
                int32 X = -SafeHorizontalRadius;
                X <= SafeHorizontalRadius;
                ++X
            )
            {
                if (
                    SafeHorizontalRadius == 0 &&
                    (
                        X != 0 ||
                        Y != 0
                    )
                )
                {
                    continue;
                }

                if (
                    SafeVerticalRadius == 0 &&
                    Z != 0
                )
                {
                    continue;
                }

                const double
                    NormalizedHorizontalDistanceSquared =
                        SafeHorizontalRadius > 0
                            ? static_cast<double>(
                                X * X +
                                Y * Y
                            ) *
                                InverseHorizontalExtentSquared
                            : 0.0;

                /*
                 * Ellipsoidal streaming volume:
                 *
                 *     horizontal^2 / H^2
                 *   + vertical^2   / V^2
                 *   <= 1
                 *
                 * The centre chunk is always retained.
                 */
                if (
                    NormalizedHorizontalDistanceSquared +
                        NormalizedVerticalDistanceSquared >
                    1.0
                )
                {
                    continue;
                }

                OutCoordinates.Add(
                    CentreCoordinate +
                    FIntVector(
                        X,
                        Y,
                        Z
                    )
                );
            }
        }
    }

    /*
     * Defensive guarantee for zero-radius or unusual configuration cases.
     */
    OutCoordinates.Add(
        CentreCoordinate
    );
}

FIntVector ACubusBlockWorldActor::WorldLocationToChunkCoordinate(
    const FVector& WorldLocation
) const
{
    const double ChunkWorldSize =
        static_cast<double>(Cubus::ChunkSize) *
        static_cast<double>(FMath::Max(1.0f, GeneratedVoxelSize));

    const double HalfChunkWorldSize = ChunkWorldSize * 0.5;

    const FVector RelativeLocation =
        WorldLocation - GetActorLocation();

    return FIntVector(
        FMath::FloorToInt(
            (RelativeLocation.X + HalfChunkWorldSize) / ChunkWorldSize
        ),
        FMath::FloorToInt(
            (RelativeLocation.Y + HalfChunkWorldSize) / ChunkWorldSize
        ),
        FMath::FloorToInt(
            (RelativeLocation.Z + HalfChunkWorldSize) / ChunkWorldSize
        )
    );
}

void ACubusBlockWorldActor::UpdateRuntimeStreaming(const bool bForce)
{
    APawn* PlayerPawn = TrackedPawn.Get();

    FVector ViewLocation = FVector::ZeroVector;
    bool bHasViewLocation = false;

    if (APlayerController* PlayerController =
            UGameplayStatics::GetPlayerController(this, 0))
    {
        if (APlayerCameraManager* CameraManager =
                PlayerController->PlayerCameraManager)
        {
            ViewLocation = CameraManager->GetCameraLocation();
            bHasViewLocation = true;
        }
    }

    const FVector TrackingLocation =
        bPawnHeldForStreaming
            ? HeldPawnLocation
            : (bHasViewLocation
                ? ViewLocation
                : (IsValid(PlayerPawn)
                    ? PlayerPawn->GetActorLocation()
                    : GetActorLocation()));

    const FIntVector CentreCoordinate =
        WorldLocationToChunkCoordinate(TrackingLocation);

    if (!bForce && CentreCoordinate == LastTrackedChunk)
    {
        return;
    }

    LastTrackedChunk = CentreCoordinate;

    UpdateDensityLods();

    BuildRequiredCoordinates(
        CentreCoordinate,
        HorizontalViewRadius,
        VerticalViewRadius,
        RequiredChunkCoordinates
    );

    if (!bInitialSpawnAreaReady)
    {
        BuildRequiredCoordinates(
            CentreCoordinate,
            InitialLoadRadius,
            VerticalViewRadius,
            InitialRequiredCoordinates
        );
    }

    PendingChunkGeneration.Reset();
    PendingChunkRemoval.Reset();

    for (const FIntVector& Coordinate : RequiredChunkCoordinates)
    {
        if (!IsValid(FindChunk(Coordinate)))
        {
            PendingChunkGeneration.Add(Coordinate);
        }
    }

    PendingChunkGeneration.Sort(
        [CentreCoordinate](const FIntVector& A, const FIntVector& B)
        {
            const int32 DistanceA =
                FMath::Abs(A.X - CentreCoordinate.X) +
                FMath::Abs(A.Y - CentreCoordinate.Y) +
                FMath::Abs(A.Z - CentreCoordinate.Z);

            const int32 DistanceB =
                FMath::Abs(B.X - CentreCoordinate.X) +
                FMath::Abs(B.Y - CentreCoordinate.Y) +
                FMath::Abs(B.Z - CentreCoordinate.Z);

            // Sort far-to-near so Pop() returns nearest-first in O(1).
            return DistanceA > DistanceB;
        }
    );

    for (const auto& Entry : ChunksByCoordinate)
    {
        if (!RequiredChunkCoordinates.Contains(Entry.Key))
        {
            PendingChunkRemoval.Add(Entry.Key);
        }
    }

    PendingRuntimeChunkCount = PendingChunkGeneration.Num();
}

int32 ACubusBlockWorldActor::ResolveDensitySubdivisions(
    const FIntVector& ChunkCoordinate
) const
{
    if (!bEnableDensityLod)
    {
        return 1;
    }

    const bool bHasTrackedChunk =
        LastTrackedChunk.X != MAX_int32 &&
        LastTrackedChunk.Y != MAX_int32 &&
        LastTrackedChunk.Z != MAX_int32;

    float TargetSpacing = DensityFarSampleSpacing;

    if (bHasTrackedChunk)
    {
        const int32 Distance = FCubusDensityLod::ChunkDistance(
            ChunkCoordinate,
            LastTrackedChunk
        );

        if (Distance <= DensityNearChunkRadius)
        {
            TargetSpacing = DensityNearSampleSpacing;
        }
        else if (Distance <= DensityMiddleChunkRadius)
        {
            TargetSpacing = DensityMiddleSampleSpacing;
        }
    }

    return FCubusDensityLod::ResolveSubdivisionsForSpacing(
        GeneratedVoxelSize,
        TargetSpacing
    );
}

void ACubusBlockWorldActor::UpdateDensityLods()
{
    const ECubusVoxelRenderMode RenderMode = GetVoxelRenderMode();
    if (
        RenderMode != ECubusVoxelRenderMode::Density &&
        RenderMode != ECubusVoxelRenderMode::Hybrid
    )
    {
        return;
    }

    for (const auto& Entry : ChunksByCoordinate)
    {
        ACubusVoxelVolumeActor* ChunkActor = Entry.Value.Get();
        if (!IsValid(ChunkActor))
        {
            continue;
        }

        if (ChunkActor->ConfigureDensityResolution(
            ResolveDensitySubdivisions(Entry.Key)
        ))
        {
            QueueChunkForRebuild(Entry.Key);
        }
    }
}

void ACubusBlockWorldActor::ProcessRuntimeQueues()
{
    int32 RemovedCount = 0;

    while (
        RemovedCount < MaxChunksRemovedPerTick &&
        !PendingChunkRemoval.IsEmpty()
    )
    {
        const FIntVector Coordinate = PendingChunkRemoval.Last();
        PendingChunkRemoval.Pop(EAllowShrinking::No);

        ACubusVoxelVolumeActor* ChunkActor = FindChunk(Coordinate);

        if (IsValid(ChunkActor))
        {
            UnregisterChunk(ChunkActor);
            GeneratedChunks.Remove(ChunkActor);
            ChunkActor->Destroy();
            ++RemovedCount;
        }
    }

    int32 GeneratedCount = 0;

    while (
        GeneratedCount < MaxChunksGeneratedPerTick &&
        !PendingChunkGeneration.IsEmpty()
    )
    {
        const FIntVector Coordinate = PendingChunkGeneration.Last();
        PendingChunkGeneration.Pop(EAllowShrinking::No);

        const bool bInitialTerrainStillLoading =
            !bInitialSpawnAreaReady;

        if (
            IsValid(
                SpawnChunkAtCoordinate(
                    Coordinate,
                    !bInitialTerrainStillLoading
                )
            )
        )
        {
            ++GeneratedCount;
        }
    }

    PendingRuntimeChunkCount = PendingChunkGeneration.Num();
    GeneratedChunkCount = GeneratedChunks.Num();

    if (!bInitialSpawnAreaReady && !InitialRequiredCoordinates.IsEmpty())
    {
        bool bAllInitialChunksPresent = true;

        for (const FIntVector& Coordinate : InitialRequiredCoordinates)
        {
            if (!IsValid(FindChunk(Coordinate)))
            {
                bAllInitialChunksPresent = false;
                break;
            }
        }

        if (bAllInitialChunksPresent)
        {
            bInitialSpawnAreaReady = true;

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus runtime initial spawn area is ready around chunk (%d, %d, %d)"),
                LastTrackedChunk.X,
                LastTrackedChunk.Y,
                LastTrackedChunk.Z
            );
        }
    }
}

void ACubusBlockWorldActor::DiscardActiveAtomicDensityBuilds()
{
    /*
     * Worker tasks own copied density-build inputs only.
     *
     * Dropping these handles does not touch Actors or mesh components and
     * does not require the game thread to wait for obsolete work to finish.
     */
    ActiveAtomicDensityAsyncBuilds.Reset();
}

void ACubusBlockWorldActor::ProcessAtomicDensityEditBatch()
{
    /*
     * PHASE 0: INVALIDATE AN OBSOLETE TRANSACTION
     *
     * If any density edit occurred after this transaction started, every
     * staged mesh and every worker result belongs to an obsolete revision.
     *
     * The currently visible terrain remains untouched.
     */
    if (
        bAtomicDensityBatchActive &&
        ActiveAtomicDensityRevision != DensityEditRevision
    )
    {
        DiscardActiveAtomicDensityBuilds();

        for (
            const TWeakObjectPtr<ACubusVoxelVolumeActor>& WeakChunk
            : ActiveAtomicDensityStagedChunks
        )
        {
            if (
                ACubusVoxelVolumeActor* Chunk =
                    WeakChunk.Get()
            )
            {
                Chunk->DiscardStagedVolume();
            }
        }

        /*
         * Requeue the entire abandoned batch so the newest authoritative
         * density state is rebuilt consistently.
         */
        for (
            const FIntVector& Coordinate
            : ActiveAtomicDensityBatchCoordinates
        )
        {
            if (IsValid(FindChunk(Coordinate)))
            {
                AtomicDensityDirtyChunkCoordinates.Add(
                    Coordinate
                );
            }
        }

        ActiveAtomicDensityBatchCoordinates.Reset();
        ActiveAtomicDensityStagedChunks.Reset();
        ActiveAtomicDensityPendingUploads.Reset();

        ActiveAtomicDensityBuildIndex = 0;
        ActiveAtomicDensityRevision = 0;

        ActiveAtomicDensityWorkerMilliseconds = 0.0;
        ActiveAtomicDensityUploadMilliseconds = 0.0;
        ActiveAtomicDensityMaxUploadTickMilliseconds = 0.0;

        ActiveAtomicDensityCompletedBuildCount = 0;
        ActiveAtomicDensityUploadTickCount = 0;

        bAtomicDensityBatchActive = false;
    }

    /*
     * PHASE 1: START A NEW TRANSACTION
     */
    if (!bAtomicDensityBatchActive)
    {
        if (AtomicDensityDirtyChunkCoordinates.IsEmpty())
        {
            return;
        }

        ActiveAtomicDensityBatchCoordinates =
            AtomicDensityDirtyChunkCoordinates.Array();

        AtomicDensityDirtyChunkCoordinates.Reset();

        ActiveAtomicDensityBatchCoordinates.Sort(
            [](const FIntVector& A, const FIntVector& B)
            {
                if (A.Z != B.Z)
                {
                    return A.Z < B.Z;
                }

                if (A.Y != B.Y)
                {
                    return A.Y < B.Y;
                }

                return A.X < B.X;
            }
        );

        ActiveAtomicDensityStagedChunks.Reset();

        ActiveAtomicDensityStagedChunks.Reserve(
            ActiveAtomicDensityBatchCoordinates.Num()
        );

        ActiveAtomicDensityAsyncBuilds.Reset();
        ActiveAtomicDensityPendingUploads.Reset();

        ActiveAtomicDensityBuildIndex = 0;

        ActiveAtomicDensityRevision =
            DensityEditRevision;

        ActiveAtomicDensityWorkerMilliseconds = 0.0;
        ActiveAtomicDensityUploadMilliseconds = 0.0;
        ActiveAtomicDensityMaxUploadTickMilliseconds = 0.0;

        ActiveAtomicDensityCompletedBuildCount = 0;
        ActiveAtomicDensityUploadTickCount = 0;

        bAtomicDensityBatchActive = true;
    }

    /*
     * PHASE 2: COLLECT COMPLETED WORKER RESULTS
     *
     * Iterate backwards because completed jobs are removed from the array.
     */
    for (
        int32 AsyncIndex =
            ActiveAtomicDensityAsyncBuilds.Num() - 1;
        AsyncIndex >= 0;
        --AsyncIndex
    )
    {
        FAtomicDensityAsyncBuild& AsyncBuild =
            *ActiveAtomicDensityAsyncBuilds[
                AsyncIndex
            ];

        if (!AsyncBuild.Task.IsCompleted())
        {
            continue;
        }

        /*
         * Defensive stale-result protection.
         *
         * Normally the revision mismatch at the top of the function catches
         * this before we arrive here.
         */
        if (
            AsyncBuild.Revision != DensityEditRevision ||
            AsyncBuild.Revision != ActiveAtomicDensityRevision
        )
        {
            DiscardActiveAtomicDensityBuilds();

            for (
                const TWeakObjectPtr<ACubusVoxelVolumeActor>& WeakChunk
                : ActiveAtomicDensityStagedChunks
            )
            {
                if (
                    ACubusVoxelVolumeActor* Chunk =
                        WeakChunk.Get()
                )
                {
                    Chunk->DiscardStagedVolume();
                }
            }

            for (
                const FIntVector& Coordinate
                : ActiveAtomicDensityBatchCoordinates
            )
            {
                if (IsValid(FindChunk(Coordinate)))
                {
                    AtomicDensityDirtyChunkCoordinates.Add(
                        Coordinate
                    );
                }
            }

            ActiveAtomicDensityBatchCoordinates.Reset();
            ActiveAtomicDensityStagedChunks.Reset();
            ActiveAtomicDensityAsyncBuilds.Reset();
            ActiveAtomicDensityPendingUploads.Reset();

            ActiveAtomicDensityBuildIndex = 0;
            ActiveAtomicDensityRevision = 0;
            bAtomicDensityBatchActive = false;

            return;
        }

        const FIntVector CompletedCoordinate =
            AsyncBuild.ChunkCoordinate;

        if (
            IsValid(
                FindChunk(
                    CompletedCoordinate
                )
            )
        )
        {
            TUniquePtr<FAtomicDensityPendingUpload>
                PendingUpload =
                    MakeUnique<
                        FAtomicDensityPendingUpload
                    >();

            PendingUpload->ChunkCoordinate =
                CompletedCoordinate;

            PendingUpload->Revision =
                AsyncBuild.Revision;

            PendingUpload->BuildResult =
                MoveTemp(
                    AsyncBuild.Task.GetResult()
                );

            ActiveAtomicDensityWorkerMilliseconds +=
                PendingUpload
                    ->BuildResult
                    .BuildTimeMilliseconds;

            ++ActiveAtomicDensityCompletedBuildCount;

            ActiveAtomicDensityPendingUploads.Add(
                MoveTemp(PendingUpload)
            );
        }

        /*
         * Whether the chunk still exists or not, this worker job is finished.
         */
        ActiveAtomicDensityAsyncBuilds.RemoveAtSwap(
            AsyncIndex,
            1,
            EAllowShrinking::No
        );
    }

    /*
    * PHASE 2B: BOUNDED GAME-THREAD UPLOAD
    *
    * Worker-complete meshes are uploaded only into hidden staging components.
    * Limit the number uploaded per Tick so a 27-36 chunk transaction does not
    * dump all procedural-mesh creation work into one frame.
    */
    const int32 SafeUploadsPerTick =
        FMath::Clamp(
            MaxAtomicDensityUploadsPerTick,
            1,
            16
        );

    const double SafeUploadMillisecondsPerTick =
        static_cast<double>(
            FMath::Clamp(
                MaxAtomicDensityUploadMillisecondsPerTick,
                0.25f,
                16.0f
            )
        );

    const double UploadTickStartTime =
        FPlatformTime::Seconds();

    int32 UploadedThisTick = 0;

    while (
        UploadedThisTick < SafeUploadsPerTick &&
        !ActiveAtomicDensityPendingUploads.IsEmpty()
    )
    {
        /*
        * The upload time limit is deliberately soft.
        *
        * Always allow the first upload of a tick. After that, do not begin
        * another non-preemptible procedural-mesh upload once this tick has
        * already consumed its game-thread budget.
        */
        if (UploadedThisTick > 0)
        {
            const double ElapsedUploadMilliseconds =
                (
                    FPlatformTime::Seconds() -
                    UploadTickStartTime
                ) * 1000.0;

            if (
                ElapsedUploadMilliseconds >=
                SafeUploadMillisecondsPerTick
            )
            {
                break;
            }
        }       
        
        TUniquePtr<FAtomicDensityPendingUpload>
            PendingUpload =
                MoveTemp(
                    ActiveAtomicDensityPendingUploads.Last()
                );

        ActiveAtomicDensityPendingUploads.Pop(
            EAllowShrinking::No
        );

        if (!PendingUpload)
        {
            continue;
        }

        /*
        * Never upload stale mesh data into a staging component.
        */
        if (
            PendingUpload->Revision != DensityEditRevision ||
            PendingUpload->Revision != ActiveAtomicDensityRevision
        )
        {
            ActiveAtomicDensityPendingUploads.Reset();
            return;
        }

        ACubusVoxelVolumeActor* Chunk =
            FindChunk(
                PendingUpload->ChunkCoordinate
            );

        if (!IsValid(Chunk))
        {
            /*
            * Streaming removed this chunk while its worker was running.
            * It no longer participates in the visible transaction.
            */
            ++UploadedThisTick;
            continue;
        }

        const double UploadStartTime =
            FPlatformTime::Seconds();

        const bool bUploadSucceeded =
            Chunk->BuildStagedVolumeFromDensityMesh(
                PendingUpload->BuildResult
            );

        ActiveAtomicDensityUploadMilliseconds +=
            (
                FPlatformTime::Seconds() -
                UploadStartTime
            ) * 1000.0;

        if (!bUploadSucceeded)
        {
            /*
            * Never allow a partially staged transaction to publish.
            */
            for (
                const TWeakObjectPtr<
                    ACubusVoxelVolumeActor
                >& WeakChunk
                : ActiveAtomicDensityStagedChunks
            )
            {
                if (
                    ACubusVoxelVolumeActor* StagedChunk =
                        WeakChunk.Get()
                )
                {
                    StagedChunk->DiscardStagedVolume();
                }
            }

            for (
                const FIntVector& RetryCoordinate
                : ActiveAtomicDensityBatchCoordinates
            )
            {
                if (
                    IsValid(
                        FindChunk(
                            RetryCoordinate
                        )
                    )
                )
                {
                    AtomicDensityDirtyChunkCoordinates.Add(
                        RetryCoordinate
                    );
                }
            }

            DiscardActiveAtomicDensityBuilds();

            ActiveAtomicDensityPendingUploads.Reset();
            ActiveAtomicDensityBatchCoordinates.Reset();
            ActiveAtomicDensityStagedChunks.Reset();

            ActiveAtomicDensityBuildIndex = 0;
            ActiveAtomicDensityRevision = 0;

            ActiveAtomicDensityWorkerMilliseconds = 0.0;
            ActiveAtomicDensityUploadMilliseconds = 0.0;
            ActiveAtomicDensityMaxUploadTickMilliseconds = 0.0;

            ActiveAtomicDensityCompletedBuildCount = 0;
            ActiveAtomicDensityUploadTickCount = 0;

            bAtomicDensityBatchActive = false;

            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Cubus atomic density transaction failed "
                    "during staged mesh upload; previous terrain "
                    "revision remains visible."
                )
            );

            return;
        }

        ActiveAtomicDensityStagedChunks.Add(
            Chunk
        );

        ++UploadedThisTick;
    }

    if (UploadedThisTick > 0)
    {
        const double UploadTickMilliseconds =
            (
                FPlatformTime::Seconds() -
                UploadTickStartTime
            ) * 1000.0;

        ActiveAtomicDensityMaxUploadTickMilliseconds =
            FMath::Max(
                ActiveAtomicDensityMaxUploadTickMilliseconds,
                UploadTickMilliseconds
            );

        ++ActiveAtomicDensityUploadTickCount;

        UE_LOG(
            LogTemp,
            VeryVerbose,
            TEXT(
                "Cubus density upload tick: "
                "%d uploads, %.2f ms"
            ),
            UploadedThisTick,
            UploadTickMilliseconds
        );
    }

    /*
     * A density edit may theoretically have occurred during other game-thread
     * work before reaching this point. Never launch additional jobs for an
     * obsolete transaction.
     */
    if (
        ActiveAtomicDensityRevision !=
        DensityEditRevision
    )
    {
        return;
    }

    /*
     * PHASE 3: FILL THE WORKER POOL
     *
     * This is the major D3 change.
     *
     * Instead of:
     *
     *     chunk A -> wait -> chunk B -> wait -> chunk C
     *
     * we now maintain up to MaxConcurrentDensityBuilds jobs simultaneously.
     */
    const int32 SafeConcurrency =
        FMath::Clamp(
            MaxConcurrentDensityBuilds,
            1,
            16
        );

    while (
        ActiveAtomicDensityAsyncBuilds.Num() <
            SafeConcurrency &&
        ActiveAtomicDensityBuildIndex <
            ActiveAtomicDensityBatchCoordinates.Num()
    )
    {
        const FIntVector Coordinate =
            ActiveAtomicDensityBatchCoordinates[
                ActiveAtomicDensityBuildIndex
            ];

        ++ActiveAtomicDensityBuildIndex;

        /*
         * The density transaction supersedes ordinary dirty rebuilding for
         * this coordinate.
         */
        DirtyChunkCoordinates.Remove(
            Coordinate
        );

        ACubusVoxelVolumeActor* Chunk =
            FindChunk(
                Coordinate
            );

        if (!IsValid(Chunk))
        {
            /*
             * Streaming removed the chunk.
             *
             * If it returns later, its normal construction path will consume
             * the latest authoritative density edits.
             */
            continue;
        }

        /*
         * Everything UObject/Actor-derived is captured here on the game
         * thread.
         */
        const FCubusDensityMeshBuildInput BuildInput =
            Chunk->CaptureDensityMeshBuildInput();

        TUniquePtr<FAtomicDensityAsyncBuild> AsyncBuild =
            MakeUnique<FAtomicDensityAsyncBuild>();

        AsyncBuild->ChunkCoordinate =
            Coordinate;

        AsyncBuild->Revision =
            ActiveAtomicDensityRevision;

        /*
         * The worker lambda receives copied plain data only.
         *
         * No Actor, World or procedural mesh is touched here.
         */
        AsyncBuild->Task =
            UE::Tasks::Launch(
                TEXT("CubusDensityMesh"),
                [BuildInput]()
                {
                    return
                        ACubusVoxelVolumeActor::
                            BuildDensityMeshData(
                                BuildInput
                            );
                }
            );

        ActiveAtomicDensityAsyncBuilds.Add(
            MoveTemp(AsyncBuild)
        );
    }

    /*
     * If work is still running, keep the previous complete terrain revision
     * visible and return immediately.
     */
    if (!ActiveAtomicDensityAsyncBuilds.IsEmpty())
    {
        return;
    }

    /*
    * All worker tasks may be finished while completed meshes are still waiting
    * for their bounded game-thread staging upload.
    *
    * The old complete terrain revision remains visible during this period.
    */
    if (!ActiveAtomicDensityPendingUploads.IsEmpty())
    {
        return;
    }

    /*
    * If there are still coordinates left, the worker pool should have been
    * filled above. This is just a defensive guard.
    */
    if (
        ActiveAtomicDensityBuildIndex <
        ActiveAtomicDensityBatchCoordinates.Num()
    )
    {
        return;
    }

    /*
     * PHASE 4: FINAL REVISION VALIDATION
     *
     * All jobs have completed and all still-loaded participating chunks now
     * contain hidden staged replacement meshes.
     */
    if (
        ActiveAtomicDensityRevision !=
        DensityEditRevision
    )
    {
        /*
         * The next Tick will enter the invalidation path at the top.
         */
        return;
    }

    /*
     * PHASE 5: ATOMIC COMMIT
     *
     * Nothing becomes visible until this point.
     *
     * Every component swap occurs during this same game-thread invocation, so
     * there is no rendered frame in which neighbouring chunks belong to
     * different density revisions.
     */
    const double CommitStartTime =
        FPlatformTime::Seconds();

    for (
        const TWeakObjectPtr<ACubusVoxelVolumeActor>& WeakChunk
        : ActiveAtomicDensityStagedChunks
    )
    {
        if (
            ACubusVoxelVolumeActor* Chunk =
                WeakChunk.Get()
        )
        {
            Chunk->CommitStagedVolume();
        }
    }

    const double CommitMilliseconds =
        (
            FPlatformTime::Seconds() -
            CommitStartTime
        ) * 1000.0;

    const double AverageWorkerMilliseconds =
        ActiveAtomicDensityCompletedBuildCount > 0
            ? ActiveAtomicDensityWorkerMilliseconds /
                static_cast<double>(
                    ActiveAtomicDensityCompletedBuildCount
                )
            : 0.0;

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus density transaction: "
            "%d chunks, "
            "worker total %.2f ms, "
            "worker avg %.2f ms, "
            "upload total %.2f ms, "
            "upload max tick %.2f ms, "
            "upload ticks %d, "
            "commit %.2f ms"
        ),
        ActiveAtomicDensityCompletedBuildCount,
        ActiveAtomicDensityWorkerMilliseconds,
        AverageWorkerMilliseconds,
        ActiveAtomicDensityUploadMilliseconds,
        ActiveAtomicDensityMaxUploadTickMilliseconds,
        ActiveAtomicDensityUploadTickCount,
        CommitMilliseconds
    );

    /*
     * Transaction finished successfully.
     */
    ActiveAtomicDensityBatchCoordinates.Reset();
    ActiveAtomicDensityStagedChunks.Reset();
    ActiveAtomicDensityAsyncBuilds.Reset();
    ActiveAtomicDensityPendingUploads.Reset();

    ActiveAtomicDensityBuildIndex = 0;
    ActiveAtomicDensityRevision = 0;
    bAtomicDensityBatchActive = false;
}

void ACubusBlockWorldActor::ProcessDirtyChunkQueue()
{
    int32 RebuiltCount = 0;

    while (
        RebuiltCount < MaxDirtyChunksRebuiltPerTick &&
        !DirtyChunkCoordinates.IsEmpty()
    )
    {
        // Choose a stable coordinate so remesh order is deterministic across
        // runs rather than depending on TSet iteration order.
        FIntVector NextCoordinate(MAX_int32, MAX_int32, MAX_int32);

        for (const FIntVector& Coordinate : DirtyChunkCoordinates)
        {
            if (
                Coordinate.X < NextCoordinate.X ||
                (Coordinate.X == NextCoordinate.X && Coordinate.Y < NextCoordinate.Y) ||
                (Coordinate.X == NextCoordinate.X && Coordinate.Y == NextCoordinate.Y && Coordinate.Z < NextCoordinate.Z)
            )
            {
                NextCoordinate = Coordinate;
            }
        }

        DirtyChunkCoordinates.Remove(NextCoordinate);

        if (IsValid(FindChunk(NextCoordinate)))
        {
            RebuildChunkAtCoordinate(NextCoordinate);
            ++RebuiltCount;
        }
    }
}

void ACubusBlockWorldActor::HoldPawnForInitialStreaming()
{
    APawn* PlayerPawn = TrackedPawn.Get();

    if (!IsValid(PlayerPawn) || bPawnHeldForStreaming)
    {
        return;
    }

    HeldPawnLocation = PlayerPawn->GetActorLocation();
    HeldPawnElapsedSeconds = 0.0f;
    bSpawnTimeoutReported = false;
    PlayerPawn->SetActorEnableCollision(false);
    PlayerPawn->SetActorTickEnabled(false);
    bPawnHeldForStreaming = true;
}

void ACubusBlockWorldActor::TryReleasePawnToTerrain()
{
    const bool bSpawnHoldTimedOut =
        SpawnHoldTimeoutSeconds > 0.0f &&
        HeldPawnElapsedSeconds >= SpawnHoldTimeoutSeconds;

    if (!bPawnHeldForStreaming || (!bInitialSpawnAreaReady && !bSpawnHoldTimedOut))
    {
        return;
    }

    APawn* PlayerPawn = TrackedPawn.Get();
    UWorld* World = GetWorld();

    if (!IsValid(PlayerPawn) || !IsValid(World))
    {
        return;
    }

    auto ReleasePawnAtSurfaceZ =
        [this, PlayerPawn](const double SurfaceZ, const TCHAR* Reason)
        {
            PlayerPawn->SetActorLocation(
                FVector(
                    HeldPawnLocation.X,
                    HeldPawnLocation.Y,
                    SurfaceZ + static_cast<double>(SpawnHeightOffset)
                ),
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );

            PlayerPawn->SetActorEnableCollision(true);
            PlayerPawn->SetActorTickEnabled(true);
            bPawnHeldForStreaming = false;
            HeldPawnElapsedSeconds = 0.0f;

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus released player (%s) at runtime terrain surface Z=%.2f"),
                Reason,
                SurfaceZ
            );

            UpdateRuntimeStreaming(true);
        };

    auto TryFindSurfaceFromChunkData =
        [this](double& OutSurfaceZ)
        {
            bool bFoundSurface = false;
            double HighestSurfaceZ = -TNumericLimits<double>::Max();

            for (const auto& Entry : ChunksByCoordinate)
            {
                ACubusVoxelVolumeActor* ChunkActor = Entry.Value.Get();

                if (!IsValid(ChunkActor))
                {
                    continue;
                }

                if (!ChunkActor->HasBuiltTerrainCollision())
                {
                    continue;
                }

                const FCubusBlockChunkData* ChunkData =
                    ChunkActor->GetChunkData();

                if (ChunkData == nullptr || !ChunkData->HasAnyOccupiedVoxel())
                {
                    continue;
                }

                const double VoxelSize =
                    static_cast<double>(FMath::Max(1.0f, ChunkActor->GetVoxelSize()));

                const double HalfChunkWorldSize =
                    static_cast<double>(Cubus::ChunkSize) * VoxelSize * 0.5;

                const FVector ChunkLocation = ChunkActor->GetActorLocation();

                const int32 LocalX = FMath::FloorToInt(
                    (HeldPawnLocation.X - ChunkLocation.X + HalfChunkWorldSize) /
                    VoxelSize
                );

                const int32 LocalY = FMath::FloorToInt(
                    (HeldPawnLocation.Y - ChunkLocation.Y + HalfChunkWorldSize) /
                    VoxelSize
                );

                if (
                    LocalX < 0 || LocalX >= Cubus::ChunkSize ||
                    LocalY < 0 || LocalY >= Cubus::ChunkSize
                )
                {
                    continue;
                }

                for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
                {
                    if (ChunkData->IsEmpty(LocalX, LocalY, LocalZ))
                    {
                        continue;
                    }

                    const double SurfaceZ =
                        ChunkLocation.Z +
                        ((static_cast<double>(LocalZ) + 1.0) * VoxelSize) -
                        HalfChunkWorldSize;

                    if (!bFoundSurface || SurfaceZ > HighestSurfaceZ)
                    {
                        HighestSurfaceZ = SurfaceZ;
                        bFoundSurface = true;
                    }

                    break;
                }
            }

            if (bFoundSurface)
            {
                OutSurfaceZ = HighestSurfaceZ;
            }

            return bFoundSurface;
        };

    const float ChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) *
        FMath::Max(1.0f, GeneratedVoxelSize);

    const FVector TraceStart(
        HeldPawnLocation.X,
        HeldPawnLocation.Y,
        HeldPawnLocation.Z + ChunkWorldSize * 4.0f
    );

    const FVector TraceEnd(
        HeldPawnLocation.X,
        HeldPawnLocation.Y,
        HeldPawnLocation.Z - ChunkWorldSize * 8.0f
    );

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CubusSpawnSurfaceTrace),
        false,
        PlayerPawn
    );

    FHitResult HitResult;
    bool bHitTerrain = false;

    // Visibility is shared by gameplay objects, so keep tracing through
    // unrelated blockers until the hit is proven to be a collision mesh owned
    // by this terrain world. This prevents a prop, foliage actor or another
    // world instance from becoming the player's spawn surface.
    for (int32 TraceAttempt = 0; TraceAttempt < 64; ++TraceAttempt)
    {
        FHitResult CandidateHit;

        if (!World->LineTraceSingleByChannel(
            CandidateHit,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            QueryParams
        ))
        {
            break;
        }

        ACubusVoxelVolumeActor* HitChunk =
            Cast<ACubusVoxelVolumeActor>(CandidateHit.GetActor());

        if (
            IsValid(HitChunk) &&
            HitChunk->HasBuiltTerrainCollision() &&
            (
                HitChunk->GetOwningBlockWorld() == this ||
                HitChunk->GetOwner() == this
            )
        )
        {
            HitResult = CandidateHit;
            bHitTerrain = true;
            break;
        }

        AActor* BlockingActor = CandidateHit.GetActor();

        if (!IsValid(BlockingActor))
        {
            break;
        }

        QueryParams.AddIgnoredActor(BlockingActor);
    }

    if (!bHitTerrain)
    {
        const FIntVector HeldChunkCoordinate =
            WorldLocationToChunkCoordinate(HeldPawnLocation);

        const FCubusChunkStoreContext StoreContext
        {
            WorldSeed,
            FCubusGenerationSeeds::CurrentGenerationVersion
        };

        bool bRecoveredAnyChunk = false;

        auto RecoverChunkAtCoordinate =
            [this, &StoreContext, &bRecoveredAnyChunk](const FIntVector& Coordinate)
            {
                ACubusVoxelVolumeActor* ChunkActor = FindChunk(Coordinate);

                if (!IsValid(ChunkActor))
                {
                    ChunkActor = SpawnChunkAtCoordinate(Coordinate, false);
                }

                if (!IsValid(ChunkActor))
                {
                    return;
                }

                const FCubusBlockChunkData* ChunkData =
                    ChunkActor->GetChunkData();

                const bool bNeedsRegeneration =
                    ChunkData == nullptr ||
                    !ChunkData->HasAnyOccupiedVoxel();

                if (bNeedsRegeneration)
                {
                    FCubusChunkStore::DeleteChunk(Coordinate, StoreContext);
                    ChunkActor->GenerateTerrainData();
                    ChunkActor->RebuildVolume();
                    ChunkActor->SaveCachedChunk();
                    bRecoveredAnyChunk = true;
                }
            };

        const int32 RecoveryHorizontalRadius = FMath::Max(1, InitialLoadRadius);
        const int32 RecoveryVerticalRadius = FMath::Max(1, VerticalViewRadius);

        TSet<FIntVector> RecoveryCoordinates;
        BuildRequiredCoordinates(
            HeldChunkCoordinate,
            RecoveryHorizontalRadius,
            RecoveryVerticalRadius,
            RecoveryCoordinates
        );

        for (const FIntVector& Coordinate : RecoveryCoordinates)
        {
            RecoverChunkAtCoordinate(Coordinate);
        }

        RecoverChunkAtCoordinate(HeldChunkCoordinate + FIntVector(0, 0, -1));

        if (bRecoveredAnyChunk)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus spawn trace missed terrain; force-regenerated spawn neighborhood around (%d, %d, %d)"),
                HeldChunkCoordinate.X,
                HeldChunkCoordinate.Y,
                HeldChunkCoordinate.Z
            );

            UpdateRuntimeStreaming(true);
        }

        double DataSurfaceZ = 0.0;

        if (
            GetVoxelRenderMode() != ECubusVoxelRenderMode::Density &&
            TryFindSurfaceFromChunkData(DataSurfaceZ)
        )
        {
            ReleasePawnAtSurfaceZ(DataSurfaceZ, TEXT("voxel-data fallback"));
            return;
        }

        if (bSpawnHoldTimedOut && !bSpawnTimeoutReported)
        {
            bSpawnTimeoutReported = true;
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Cubus spawn hold timed out without collision from this terrain world; pawn remains held rather than being released into invalid space.")
            );
        }

        return;
    }

    ReleasePawnAtSurfaceZ(
        HitResult.ImpactPoint.Z,
        TEXT("terrain-line-trace")
    );
}

void ACubusBlockWorldActor::RemoveInvalidChunks()
{
    for (auto Iterator = ChunksByCoordinate.CreateIterator(); Iterator; ++Iterator)
    {
        if (!Iterator.Value().IsValid())
        {
            Iterator.RemoveCurrent();
        }
    }

    GeneratedChunks.RemoveAll(
        [](const TObjectPtr<ACubusVoxelVolumeActor>& ChunkActor)
        {
            return !IsValid(ChunkActor);
        }
    );

    RegisteredChunkCount = ChunksByCoordinate.Num();
    GeneratedChunkCount = GeneratedChunks.Num();
}
