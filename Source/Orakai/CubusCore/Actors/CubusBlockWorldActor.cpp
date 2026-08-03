#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Actors/CubusWorldVegetationActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Persistence/OrakaiPersistenceSubsystem.h"
#include "CubusCore/Persistence/OrakaiPersistenceTypes.h"
#include "CubusCore/Storage/CubusChunkStore.h"

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

    // Editing is valid in both fixed-grid and streamed worlds.
    ProcessDirtyChunkQueue();

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

    ProcessRuntimeQueues();
    TryReleasePawnToTerrain();

    RecordTrackedPawnCoordinate();
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
    // The density sampling buffer has a one-sample gradient halo. A sample on
    // a corner can therefore affect normals in all 26 surrounding chunks.
    for (int32 Z = -1; Z <= 1; ++Z)
    {
        for (int32 Y = -1; Y <= 1; ++Y)
        {
            for (int32 X = -1; X <= 1; ++X)
            {
                QueueChunkForRebuild(
                    ChunkCoordinate + FIntVector(X, Y, Z)
                );
            }
        }
    }
}

FCubusDensityEditMap ACubusBlockWorldActor::BuildDensityEditSnapshot(
    const FIntVector& ChunkCoordinate
) const
{
    FCubusDensityEditMap Snapshot;

    const FIntVector SampleMinimum =
        ChunkCoordinate * Cubus::ChunkSize +
        FIntVector(
            FCubusDensitySamplingBuffer::MinimumLocalSample,
            FCubusDensitySamplingBuffer::MinimumLocalSample,
            FCubusDensitySamplingBuffer::MinimumLocalSample
        );

    const FIntVector SampleMaximum =
        ChunkCoordinate * Cubus::ChunkSize +
        FIntVector(
            FCubusDensitySamplingBuffer::MaximumLocalSample,
            FCubusDensitySamplingBuffer::MaximumLocalSample,
            FCubusDensitySamplingBuffer::MaximumLocalSample
        );

    for (const auto& Entry : DensityEdits)
    {
        const FIntVector& Coordinate = Entry.Key;

        if (
            Coordinate.X >= SampleMinimum.X && Coordinate.X <= SampleMaximum.X &&
            Coordinate.Y >= SampleMinimum.Y && Coordinate.Y <= SampleMaximum.Y &&
            Coordinate.Z >= SampleMinimum.Z && Coordinate.Z <= SampleMaximum.Z
        )
        {
            Snapshot.Add(Coordinate, Entry.Value);
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

void ACubusBlockWorldActor::RestoreDensityEdits()
{
    DensityEdits.Reset();

    UOrakaiPersistenceSubsystem* Persistence =
        UOrakaiPersistenceSubsystem::Get(this);

    if (Persistence == nullptr)
    {
        return;
    }

    TArray<FOrakaiDensityEdit> StoredEdits;
    Persistence->GetDensityEdits(StoredEdits);

    for (const FOrakaiDensityEdit& StoredEdit : StoredEdits)
    {
        if (FMath::IsNearlyZero(StoredEdit.DensityDelta))
        {
            continue;
        }

        FCubusDensityEdit Edit;
        Edit.DensityDelta = StoredEdit.DensityDelta;
        Edit.MaterialId = StoredEdit.MaterialId;
        DensityEdits.Add(StoredEdit.WorldSample, Edit);
    }
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
                    DensityEdits.Remove(WorldSample);

                    if (UOrakaiPersistenceSubsystem* Persistence =
                            UOrakaiPersistenceSubsystem::Get(this))
                    {
                        Persistence->ClearDensityEdit(WorldSample);
                    }
                }
                else if (UOrakaiPersistenceSubsystem* Persistence =
                             UOrakaiPersistenceSubsystem::Get(this))
                {
                    Persistence->RecordDensityEdit(
                        WorldSample,
                        Edit.DensityDelta,
                        Edit.MaterialId
                    );
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
            VegetationActor->FindInteractiveTreeAlongRay(
                TraceStart,
                TraceEnd,
                SelectionRadius,
                OutTreeWorldVoxel
            )
        )
        {
            RecordGeneratedTreeTombstone(OutTreeWorldVoxel);
            RemoveFoliageAtWorldVoxel(OutTreeWorldVoxel);
            return true;
        }
    }

    const FVector Segment = TraceEnd - TraceStart;
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

    if (
        ACubusPCGVoxelVolumeActor* PCGChunk =
            Cast<ACubusPCGVoxelVolumeActor>(ChunkActor)
    )
    {
        PCGChunk->ConfigureVegetationPCG(
            VegetationPCGGraph,
            bGenerateVegetationPCG && bGenerateVegetation
        );
    }

    ChunkActor->SetOwner(this);
    ChunkActor->AttachToComponent(
        WorldRoot,
        FAttachmentTransformRules::KeepWorldTransform
    );

    RegisterChunk(ChunkActor);

    ChunkActor->GenerateTerrainData();
    ApplyPersistedEditsToChunk(*ChunkActor);
    ChunkActor->RebuildVolume();

    if (
        bGenerateVegetation &&
        bGenerateVegetationPCG
    )
    {
        if (
            ACubusPCGVoxelVolumeActor* PCGChunk =
                Cast<ACubusPCGVoxelVolumeActor>(ChunkActor)
        )
        {
            PCGChunk->RegenerateVegetationPCG();
        }
    }

    GeneratedChunkCount = GeneratedChunks.Num();
    return ChunkActor;
}

void ACubusBlockWorldActor::GenerateChunkGrid()
{
    ClearGeneratedChunks();

    GridDimensions.X = FMath::Max(1, GridDimensions.X);
    GridDimensions.Y = FMath::Max(1, GridDimensions.Y);
    GridDimensions.Z = FMath::Max(1, GridDimensions.Z);

    for (int32 Z = 0; Z < GridDimensions.Z; ++Z)
    {
        for (int32 Y = 0; Y < GridDimensions.Y; ++Y)
        {
            for (int32 X = 0; X < GridDimensions.X; ++X)
            {
                SpawnChunkAtCoordinate(
                    GridOrigin + FIntVector(X, Y, Z),
                    true
                );
            }
        }
    }

    RefreshChunkRegistry();
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

    for (TActorIterator<ACubusVoxelVolumeActor> Iterator(World); Iterator; ++Iterator)
    {
        ACubusVoxelVolumeActor* ChunkActor = *Iterator;

        if (!IsValid(ChunkActor))
        {
            continue;
        }

        const bool bOwnedByThisWorld = ChunkActor->GetOwner() == this;
        const bool bAttachedToThisWorld =
            ChunkActor->GetAttachParentActor() == this;

        if (!bOwnedByThisWorld && !bAttachedToThisWorld)
        {
            continue;
        }

        ChunkActor->SetOwningBlockWorld(this);
        RegisterChunk(ChunkActor);
    }

    RemoveInvalidChunks();
    RegisteredChunkCount = ChunksByCoordinate.Num();
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

        if (
            ACubusPCGVoxelVolumeActor* PCGChunk =
                Cast<ACubusPCGVoxelVolumeActor>(ChunkActor)
        )
        {
            PCGChunk->ConfigureVegetationPCG(
                VegetationPCGGraph,
                bGenerateVegetationPCG
            );
        }

        ChunkActor->GenerateTerrainData();
        ChunkActor->RebuildVolume();

        if (
            ACubusPCGVoxelVolumeActor* PCGChunk =
                Cast<ACubusPCGVoxelVolumeActor>(ChunkActor)
        )
        {
            PCGChunk->RegenerateVegetationPCG();
        }
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

    for (int32 Z = -VerticalRadius; Z <= VerticalRadius; ++Z)
    {
        for (int32 Y = -HorizontalRadius; Y <= HorizontalRadius; ++Y)
        {
            for (int32 X = -HorizontalRadius; X <= HorizontalRadius; ++X)
            {
                OutCoordinates.Add(
                    CentreCoordinate + FIntVector(X, Y, Z)
                );
            }
        }
    }
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
