#include "CubusCore/Actors/CubusWorldVegetationActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

ACubusWorldVegetationActor::ACubusWorldVegetationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    VegetationPCG = CreateDefaultSubobject<UPCGComponent>(
        TEXT("WorldVegetationPCG")
    );

    if (IsValid(VegetationPCG))
    {
        VegetationPCG->SetIsPartitioned(false);
        VegetationPCG->bParseActorComponents = true;
        VegetationPCG->bOnlyTrackItself = true;
    }
}

void ACubusWorldVegetationActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    ResolveBlockWorld();
    EnsurePointCarriers();
    ConfigurePCG();

    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::BeginPlay()
{
    Super::BeginPlay();

    ResolveBlockWorld();
    EnsurePointCarriers();
    ConfigurePCG();
    TimeUntilRefresh = 0.0f;
}

void ACubusWorldVegetationActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bEnableRuntimeVegetation)
    {
        return;
    }

    TimeUntilRefresh -= DeltaSeconds;

    if (TimeUntilRefresh > 0.0f)
    {
        return;
    }

    TimeUntilRefresh = FMath::Max(0.1f, RefreshInterval);

    ResolveBlockWorld();

    int32 CurrentLoadedChunkCount = 0;
    const uint32 CurrentHash =
        CalculateLoadedPlacementHash(CurrentLoadedChunkCount);

    if (
        CurrentHash != PublishedPlacementHash ||
        CurrentLoadedChunkCount != LoadedChunkCount ||
        LastConfiguredGraph != VegetationGraph
    )
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (IsValid(VegetationPCG))
    {
        VegetationPCG->CleanupLocal(true);
    }

    ClearWorldVegetation();
    Super::EndPlay(EndPlayReason);
}

void ACubusWorldVegetationActor::RebuildWorldVegetation()
{
    ResolveBlockWorld();
    EnsurePointCarriers();
    ConfigurePCG();
    ClearWorldVegetation();

    if (!IsValid(BlockWorld))
    {
        return;
    }

    const int32 PointLimit =
        MaximumPublishedPoints > 0
            ? MaximumPublishedPoints
            : MAX_int32;

    uint32 NewHash = 0;
    int32 NewLoadedChunkCount = 0;

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    for (
        TActorIterator<ACubusVoxelVolumeActor> Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        ACubusVoxelVolumeActor* Chunk = *Iterator;

        if (
            !IsValid(Chunk) ||
            Chunk->GetOwner() != BlockWorld
        )
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData =
            Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        ++NewLoadedChunkCount;

        for (
            const FCubusVegetationInstance& Instance :
            ChunkData->GetVegetationInstances()
        )
        {
            NewHash = HashCombineFast(
                NewHash,
                GetTypeHash(Instance.WorldVoxel)
            );
            NewHash = HashCombineFast(
                NewHash,
                GetTypeHash(Instance.TypeId)
            );
            NewHash = HashCombineFast(
                NewHash,
                GetTypeHash(Instance.RotationYaw)
            );
            NewHash = HashCombineFast(
                NewHash,
                GetTypeHash(Instance.Scale)
            );

            if (PublishedPointCount >= PointLimit)
            {
                continue;
            }

            UInstancedStaticMeshComponent* TargetCarrier =
                ResolveCarrierForType(Instance.TypeId);

            if (!IsValid(TargetCarrier))
            {
                continue;
            }

            const float SafeVoxelSize =
                FMath::Max(1.0f, Chunk->GetVoxelSize());

            const FVector WorldLocation(
                (static_cast<double>(Instance.WorldVoxel.X) + 0.5) *
                    SafeVoxelSize,
                (static_cast<double>(Instance.WorldVoxel.Y) + 0.5) *
                    SafeVoxelSize,
                static_cast<double>(Instance.WorldVoxel.Z) *
                    SafeVoxelSize
            );

            const FTransform WorldTransform(
                FRotator(0.0f, Instance.RotationYaw, 0.0f),
                WorldLocation,
                FVector(Instance.Scale)
            );

            TargetCarrier->AddInstance(
                WorldTransform,
                true
            );

            ++PublishedPointCount;
        }
    }

    UInstancedStaticMeshComponent* Carriers[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (UInstancedStaticMeshComponent* Carrier : Carriers)
    {
        if (IsValid(Carrier))
        {
            Carrier->MarkRenderStateDirty();
        }
    }

    LoadedChunkCount = NewLoadedChunkCount;
    PublishedPlacementHash = NewHash;

    if (
        bEnableRuntimeVegetation &&
        IsValid(VegetationPCG) &&
        IsValid(VegetationGraph)
    )
    {
        VegetationPCG->CleanupLocal(true);
        VegetationPCG->GenerateLocal(true);
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus world vegetation: %d chunks, %d shared points"
        ),
        LoadedChunkCount,
        PublishedPointCount
    );
}

void ACubusWorldVegetationActor::ClearWorldVegetation()
{
    PublishedPointCount = 0;

    UInstancedStaticMeshComponent* Carriers[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (UInstancedStaticMeshComponent* Carrier : Carriers)
    {
        if (IsValid(Carrier))
        {
            Carrier->ClearInstances();
        }
    }
}

void ACubusWorldVegetationActor::ResolveBlockWorld()
{
    if (IsValid(BlockWorld))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    for (
        TActorIterator<ACubusBlockWorldActor> Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        BlockWorld = *Iterator;
        break;
    }
}

void ACubusWorldVegetationActor::EnsurePointCarriers()
{
    if (!IsValid(GrassPoints))
    {
        GrassPoints = CreatePointCarrier(
            TEXT("CubusWorldGrassPoints"),
            TEXT("Cubus.Vegetation.Grass")
        );
    }

    if (!IsValid(ShrubPoints))
    {
        ShrubPoints = CreatePointCarrier(
            TEXT("CubusWorldShrubPoints"),
            TEXT("Cubus.Vegetation.Shrub")
        );
    }

    if (!IsValid(TreePoints))
    {
        TreePoints = CreatePointCarrier(
            TEXT("CubusWorldTreePoints"),
            TEXT("Cubus.Vegetation.Tree")
        );
    }

    if (!IsValid(ReedsPoints))
    {
        ReedsPoints = CreatePointCarrier(
            TEXT("CubusWorldReedsPoints"),
            TEXT("Cubus.Vegetation.Reeds")
        );
    }

    if (!IsValid(AlpinePoints))
    {
        AlpinePoints = CreatePointCarrier(
            TEXT("CubusWorldAlpinePoints"),
            TEXT("Cubus.Vegetation.Alpine")
        );
    }

    UInstancedStaticMeshComponent* Carriers[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (UInstancedStaticMeshComponent* Carrier : Carriers)
    {
        if (!IsValid(Carrier))
        {
            continue;
        }

        Carrier->SetStaticMesh(MarkerMesh);
        Carrier->SetVisibility(bShowDebugMarkers, true);
        Carrier->SetHiddenInGame(!bShowDebugMarkers, true);
    }
}

void ACubusWorldVegetationActor::ConfigurePCG()
{
    if (!IsValid(VegetationPCG))
    {
        return;
    }

    VegetationPCG->SetIsPartitioned(false);
    VegetationPCG->bParseActorComponents = true;
    VegetationPCG->bOnlyTrackItself = true;

    if (LastConfiguredGraph == VegetationGraph)
    {
        return;
    }

    VegetationPCG->CleanupLocal(true);
    VegetationPCG->SetGraphLocal(VegetationGraph);
    LastConfiguredGraph = VegetationGraph;
}

uint32 ACubusWorldVegetationActor::CalculateLoadedPlacementHash(
    int32& OutLoadedChunkCount
) const
{
    OutLoadedChunkCount = 0;

    if (!IsValid(BlockWorld))
    {
        return 0;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return 0;
    }

    uint32 Hash = 0;

    for (
        TActorIterator<ACubusVoxelVolumeActor> Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        const ACubusVoxelVolumeActor* Chunk = *Iterator;

        if (
            !IsValid(Chunk) ||
            Chunk->GetOwner() != BlockWorld
        )
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData =
            Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        ++OutLoadedChunkCount;

        Hash = HashCombineFast(
            Hash,
            GetTypeHash(Chunk->GetChunkCoordinate())
        );

        Hash = HashCombineFast(
            Hash,
            GetTypeHash(
                ChunkData->GetVegetationInstances().Num()
            )
        );

        for (
            const FCubusVegetationInstance& Instance :
            ChunkData->GetVegetationInstances()
        )
        {
            Hash = HashCombineFast(
                Hash,
                GetTypeHash(Instance.WorldVoxel)
            );
            Hash = HashCombineFast(
                Hash,
                GetTypeHash(Instance.TypeId)
            );
        }
    }

    return Hash;
}

UInstancedStaticMeshComponent*
ACubusWorldVegetationActor::CreatePointCarrier(
    const FName ComponentName,
    const FName ComponentTag
)
{
    UInstancedStaticMeshComponent* Component =
        NewObject<UInstancedStaticMeshComponent>(
            this,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetMobility(EComponentMobility::Movable);
    Component->ComponentTags.AddUnique(ComponentTag);
    Component->RegisterComponent();

    return Component;
}

UInstancedStaticMeshComponent*
ACubusWorldVegetationActor::ResolveCarrierForType(
    const int32 TypeId
) const
{
    switch (TypeId)
    {
        case 1:
            return GrassPoints;

        case 2:
            return ShrubPoints;

        case 3:
            return TreePoints;

        case 4:
            return ReedsPoints;

        case 5:
            return AlpinePoints;

        default:
            return nullptr;
    }
}
