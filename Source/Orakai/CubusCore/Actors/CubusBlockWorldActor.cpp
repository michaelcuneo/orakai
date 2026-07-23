#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
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
    VerticalViewRadius = FMath::Max(0, VerticalViewRadius);
    MaxChunksGeneratedPerTick = FMath::Max(1, MaxChunksGeneratedPerTick);
    MaxChunksRemovedPerTick = FMath::Max(1, MaxChunksRemovedPerTick);
    StreamingUpdateInterval = FMath::Max(0.05f, StreamingUpdateInterval);

    RefreshChunkRegistry();
}

void ACubusBlockWorldActor::BeginPlay()
{
    Super::BeginPlay();

    if (!bEnableRuntimeStreaming)
    {
        return;
    }

    RefreshChunkRegistry();

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

void ACubusBlockWorldActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

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
}

void ACubusBlockWorldActor::UnregisterChunk(
    ACubusVoxelVolumeActor* ChunkActor
)
{
    if (ChunkActor == nullptr)
    {
        return;
    }

    for (auto Iterator = ChunksByCoordinate.CreateIterator(); Iterator; ++Iterator)
    {
        if (Iterator.Value().Get() == ChunkActor)
        {
            Iterator.RemoveCurrent();
        }
    }

    RegisteredChunkCount = ChunksByCoordinate.Num();
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
    RebuildChunkAtCoordinate(ChunkCoordinate);

    for (const FIntVector& Offset : CubusBlockWorldActor::NeighbourOffsets)
    {
        RebuildChunkAtCoordinate(ChunkCoordinate + Offset);
    }
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

    ACubusVoxelVolumeActor* ChunkActor =
        World->SpawnActor<ACubusVoxelVolumeActor>(
            ResolvedChunkClass,
            FVector::ZeroVector,
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
        MaterialRegistry,
        FallbackVoxelMaterial
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

    ChunkActor->GenerateTestShapeData();
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

        ChunkActor->ConfigureRendering(
            MaterialRegistry,
            FallbackVoxelMaterial
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
                bGenerateVegetationPCG
            );
        }

        ChunkActor->GenerateTestShapeData();
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

    const FVector RelativeLocation =
        WorldLocation - GetActorLocation();

    return FIntVector(
        FMath::FloorToInt(RelativeLocation.X / ChunkWorldSize),
        FMath::FloorToInt(RelativeLocation.Y / ChunkWorldSize),
        FMath::FloorToInt(RelativeLocation.Z / ChunkWorldSize)
    );
}

void ACubusBlockWorldActor::UpdateRuntimeStreaming(const bool bForce)
{
    APawn* PlayerPawn = TrackedPawn.Get();

    const FVector TrackingLocation =
        IsValid(PlayerPawn)
            ? (bPawnHeldForStreaming ? HeldPawnLocation : PlayerPawn->GetActorLocation())
            : GetActorLocation();

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

            return DistanceA < DistanceB;
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
        const FIntVector Coordinate = PendingChunkRemoval[0];
        PendingChunkRemoval.RemoveAt(0, 1, EAllowShrinking::No);

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
        const FIntVector Coordinate = PendingChunkGeneration[0];
        PendingChunkGeneration.RemoveAt(0, 1, EAllowShrinking::No);

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

void ACubusBlockWorldActor::HoldPawnForInitialStreaming()
{
    APawn* PlayerPawn = TrackedPawn.Get();

    if (!IsValid(PlayerPawn) || bPawnHeldForStreaming)
    {
        return;
    }

    HeldPawnLocation = PlayerPawn->GetActorLocation();
    PlayerPawn->SetActorEnableCollision(false);
    PlayerPawn->SetActorTickEnabled(false);
    bPawnHeldForStreaming = true;
}

void ACubusBlockWorldActor::TryReleasePawnToTerrain()
{
    if (!bPawnHeldForStreaming || !bInitialSpawnAreaReady)
    {
        return;
    }

    APawn* PlayerPawn = TrackedPawn.Get();
    UWorld* World = GetWorld();

    if (!IsValid(PlayerPawn) || !IsValid(World))
    {
        return;
    }

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

    FHitResult HitResult;
    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(CubusSpawnSurfaceTrace),
        false,
        PlayerPawn
    );

    const bool bHitTerrain = World->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        ECC_Visibility,
        QueryParams
    );

    if (!bHitTerrain)
    {
        return;
    }

    PlayerPawn->SetActorLocation(
        HitResult.ImpactPoint + FVector(0.0f, 0.0f, SpawnHeightOffset),
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    PlayerPawn->SetActorEnableCollision(true);
    PlayerPawn->SetActorTickEnabled(true);
    bPawnHeldForStreaming = false;

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus released player at runtime terrain surface Z=%.2f"),
        HitResult.ImpactPoint.Z
    );

    UpdateRuntimeStreaming(true);
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
