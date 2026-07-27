#include "CubusCore/Actors/CubusWorldVegetationActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/SoftObjectPath.h"

namespace
{
    constexpr int32 GrassType = 1;
    constexpr int32 ShrubType = 2;
    constexpr int32 BroadleafType = 3;
    constexpr int32 ReedsType = 4;
    constexpr int32 AlpineType = 5;
    constexpr int32 ConiferType = 6;
}

ACubusWorldVegetationActor::ACubusWorldVegetationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    ElderMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D.Tree_Elder_01_D")
    ));

    NorwaySpruceMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D.Tree_Norway_Spruce_01_D")
    ));

    GreasewoodMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_D.Shrub_Greasewood_01_D")
    ));
}

void ACubusWorldVegetationActor::ConfigureForWorld(
    ACubusBlockWorldActor* InBlockWorld
)
{
    BlockWorld = InBlockWorld;
    PublishedPlacementHash = 0;
    TimeUntilRefresh = 0.0f;

    if (HasActorBegunPlay())
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::ConfigureForWorld(
    ACubusBlockWorldActor* InBlockWorld,
    UPCGGraphInterface* InVegetationGraph,
    const bool bInEnableRuntimeVegetation
)
{
    ConfigureForWorld(InBlockWorld);
}

void ACubusWorldVegetationActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    ResolveBlockWorld();
    EnsurePointCarriers();

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
    EnsurePlantBatches();
    TimeUntilRefresh = 0.0f;
}

void ACubusWorldVegetationActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

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
        CurrentHash != static_cast<uint32>(PublishedPlacementHash) ||
        CurrentLoadedChunkCount != LoadedChunkCount
    )
    {
        RebuildWorldVegetation();
    }
}

void ACubusWorldVegetationActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    ClearWorldVegetation();
    Super::EndPlay(EndPlayReason);
}

void ACubusWorldVegetationActor::RebuildWorldVegetation()
{
    ResolveBlockWorld();
    EnsurePointCarriers();
    EnsurePlantBatches();
    ClearWorldVegetation();

    if (!IsValid(BlockWorld))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    const int32 PointLimit = MaximumPublishedPoints > 0
        ? MaximumPublishedPoints
        : MAX_int32;

    const int32 PlantLimit = MaximumRenderedPlants > 0
        ? MaximumRenderedPlants
        : MAX_int32;

    uint32 NewHash = 0;
    int32 NewLoadedChunkCount = 0;

    TArray<FTransform> ExistingTransforms;
    TArray<FTransform> ElderTransforms;
    TArray<FTransform> SpruceTransforms;
    TArray<FTransform> GreasewoodTransforms;

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

        const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        ++NewLoadedChunkCount;

        const float SafeVoxelSize =
            FMath::Max(1.0f, Chunk->GetVoxelSize());

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

            if (PublishedPointCount < PointLimit)
            {
                UInstancedStaticMeshComponent* TargetCarrier =
                    ResolveCarrierForType(Instance.TypeId);

                if (IsValid(TargetCarrier))
                {
                    TargetCarrier->AddInstance(WorldTransform, true);
                    ++PublishedPointCount;
                }
            }

            if (
                !bRenderWorldPlantBatches ||
                RenderedPlantCount >= PlantLimit
            )
            {
                continue;
            }

            const FTransform LocalTransform =
                WorldTransform.GetRelativeTransform(GetActorTransform());

            if (Instance.TypeId == ShrubType)
            {
                if (IsValid(GreasewoodInstances))
                {
                    GreasewoodTransforms.Add(LocalTransform);
                    ++RenderedPlantCount;
                }

                continue;
            }

            if (Instance.TypeId == ConiferType)
            {
                if (IsValid(NorwaySpruceInstances))
                {
                    SpruceTransforms.Add(LocalTransform);
                    ++RenderedPlantCount;
                }

                continue;
            }

            if (Instance.TypeId != BroadleafType)
            {
                continue;
            }

            uint32 SpeciesHash = GetTypeHash(Instance.WorldVoxel);
            SpeciesHash = HashCombineFast(
                SpeciesHash,
                GetTypeHash(Instance.RotationYaw)
            );

            if (
                IsValid(ExistingTreeInstances) &&
                (SpeciesHash & 1u) == 0u
            )
            {
                ExistingTransforms.Add(LocalTransform);
                ++RenderedPlantCount;
            }
            else if (IsValid(ElderInstances))
            {
                ElderTransforms.Add(LocalTransform);
                ++RenderedPlantCount;
            }
        }
    }

    auto PublishBatch = [](
        UInstancedSkinnedMeshComponent* Component,
        const TArray<FTransform>& Transforms
    )
    {
        if (!IsValid(Component) || Transforms.IsEmpty())
        {
            return;
        }

        TArray<int32> AnimationIndices;
        AnimationIndices.Init(0, Transforms.Num());

        Component->AddInstances(
            Transforms,
            AnimationIndices,
            false,
            false
        );
        Component->OptimizeInstanceData(false);
    };

    PublishBatch(ExistingTreeInstances, ExistingTransforms);
    PublishBatch(ElderInstances, ElderTransforms);
    PublishBatch(NorwaySpruceInstances, SpruceTransforms);
    PublishBatch(GreasewoodInstances, GreasewoodTransforms);

    UInstancedStaticMeshComponent* Carriers[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ConiferTreePoints,
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
    PublishedPlacementHash = static_cast<int64>(NewHash);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus world vegetation: %d chunks, %d points, %d world-batched plants"
        ),
        LoadedChunkCount,
        PublishedPointCount,
        RenderedPlantCount
    );
}

void ACubusWorldVegetationActor::ClearWorldVegetation()
{
    PublishedPointCount = 0;
    RenderedPlantCount = 0;

    UInstancedStaticMeshComponent* Carriers[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ConiferTreePoints,
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

    UInstancedSkinnedMeshComponent* PlantBatches[] =
    {
        ExistingTreeInstances,
        ElderInstances,
        NorwaySpruceInstances,
        GreasewoodInstances
    };

    for (UInstancedSkinnedMeshComponent* PlantBatch : PlantBatches)
    {
        if (IsValid(PlantBatch))
        {
            PlantBatch->ClearInstances();
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
            TEXT("Cubus.Vegetation.Tree.Broadleaf")
        );
    }

    if (!IsValid(ConiferTreePoints))
    {
        ConiferTreePoints = CreatePointCarrier(
            TEXT("CubusWorldConiferTreePoints"),
            TEXT("Cubus.Vegetation.Tree.Conifer")
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
        ConiferTreePoints,
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

void ACubusWorldVegetationActor::EnsurePlantBatches()
{
    if (!bRenderWorldPlantBatches)
    {
        return;
    }

    auto EnsureBatch = [this](
        TSoftObjectPtr<USkeletalMesh>& MeshReference,
        TObjectPtr<UInstancedSkinnedMeshComponent>& Component,
        const FName ComponentName
    )
    {
        if (MeshReference.IsNull())
        {
            return;
        }

        USkeletalMesh* Mesh = MeshReference.LoadSynchronous();

        if (!IsValid(Mesh))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus vegetation could not load mesh %s"),
                *MeshReference.ToSoftObjectPath().ToString()
            );
            return;
        }

        if (!IsValid(Component))
        {
            Component = CreatePlantBatch(ComponentName);
        }

        if (IsValid(Component))
        {
            Component->SetSkinnedAssetAndUpdate(Mesh);
            Component->SetCullDistances(
                FMath::Max(0, PlantStartCullDistance),
                FMath::Max(PlantStartCullDistance, PlantEndCullDistance)
            );
        }
    };

    EnsureBatch(
        ExistingTreeMesh,
        ExistingTreeInstances,
        TEXT("CubusWorldExistingTreeInstances")
    );
    EnsureBatch(
        ElderMesh,
        ElderInstances,
        TEXT("CubusWorldElderInstances")
    );
    EnsureBatch(
        NorwaySpruceMesh,
        NorwaySpruceInstances,
        TEXT("CubusWorldNorwaySpruceInstances")
    );
    EnsureBatch(
        GreasewoodMesh,
        GreasewoodInstances,
        TEXT("CubusWorldGreasewoodInstances")
    );
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

        const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();

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
            GetTypeHash(ChunkData->GetVegetationInstances().Num())
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
            Hash = HashCombineFast(
                Hash,
                GetTypeHash(Instance.RotationYaw)
            );
            Hash = HashCombineFast(
                Hash,
                GetTypeHash(Instance.Scale)
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
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(false);
    Component->SetMobility(EComponentMobility::Movable);
    Component->ComponentTags.AddUnique(ComponentTag);
    Component->RegisterComponent();
    AddInstanceComponent(Component);

    return Component;
}

UInstancedSkinnedMeshComponent*
ACubusWorldVegetationActor::CreatePlantBatch(
    const FName ComponentName
)
{
    UInstancedSkinnedMeshComponent* Component =
        NewObject<UInstancedSkinnedMeshComponent>(
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
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCullDistances(
        FMath::Max(0, PlantStartCullDistance),
        FMath::Max(PlantStartCullDistance, PlantEndCullDistance)
    );
    Component->RegisterComponent();
    AddInstanceComponent(Component);

    return Component;
}

UInstancedStaticMeshComponent*
ACubusWorldVegetationActor::ResolveCarrierForType(
    const int32 TypeId
) const
{
    switch (TypeId)
    {
        case GrassType:
            return GrassPoints;
        case ShrubType:
            return ShrubPoints;
        case BroadleafType:
            return TreePoints;
        case ReedsType:
            return ReedsPoints;
        case AlpineType:
            return AlpinePoints;
        case ConiferType:
            return ConiferTreePoints;
        default:
            return nullptr;
    }
}
