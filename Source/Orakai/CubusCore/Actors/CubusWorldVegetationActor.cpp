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
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "UObject/SoftObjectPath.h"

namespace
{
    constexpr int32 ShrubType = 2;
    constexpr int32 BroadleafType = 3;
    constexpr int32 ConiferType = 6;
}

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
    ACubusBlockWorldActor* InBlockWorld,
    UPCGGraphInterface* InVegetationGraph,
    const bool bInEnableRuntimeVegetation
)
{
    BlockWorld = InBlockWorld;
    VegetationGraph = InVegetationGraph;
    bEnableRuntimeVegetation = bInEnableRuntimeVegetation;
    TimeUntilRefresh = 0.0f;
    ConfigurePCG();
}

void ACubusWorldVegetationActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    ResolveBlockWorld();
    EnsurePointCarriers();
    EnsurePlantBatches();
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
    EnsurePlantBatches();
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
    EnsurePlantBatches();
    ConfigurePCG();
    ClearWorldVegetation();

    if (!IsValid(BlockWorld))
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

        if (!IsValid(Chunk) || Chunk->GetOwner() != BlockWorld)
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        ++NewLoadedChunkCount;
        const float SafeVoxelSize = FMath::Max(1.0f, Chunk->GetVoxelSize());

        for (
            const FCubusVegetationInstance& Instance :
            ChunkData->GetVegetationInstances()
        )
        {
            NewHash = HashCombineFast(
                NewHash,
                GetTypeHash(Instance.WorldVoxel)
            );
            NewHash = HashCombineFast(NewHash, GetTypeHash(Instance.TypeId));
            NewHash = HashCombineFast(
                NewHash,
                GetTypeHash(Instance.RotationYaw)
            );
            NewHash = HashCombineFast(NewHash, GetTypeHash(Instance.Scale));

            const FVector WorldLocation(
                (static_cast<double>(Instance.WorldVoxel.X) + 0.5) *
                    SafeVoxelSize,
                (static_cast<double>(Instance.WorldVoxel.Y) + 0.5) *
                    SafeVoxelSize,
                static_cast<double>(Instance.WorldVoxel.Z) * SafeVoxelSize
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

            // The original tree remains available through the existing PCG
            // path. If an explicit world-level mesh is assigned, half of the
            // broadleaf records move into that batch; otherwise Elder receives
            // all broadleaf records without duplicating the PCG tree.
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

    auto PublishBatch = [this](
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
            TEXT("Cubus.Vegetation.Tree")
        );

        if (IsValid(TreePoints))
        {
            TreePoints->ComponentTags.AddUnique(
                TEXT("Cubus.Vegetation.Tree.Broadleaf")
            );
        }
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
        TObjectPtr<UInstancedSkinnedMeshComponent>& Component,
        const FName ComponentName,
        TSoftObjectPtr<USkeletalMesh>& MeshReference
    )
    {
        USkeletalMesh* Mesh = MeshReference.LoadSynchronous();

        if (!IsValid(Mesh))
        {
            return;
        }

        if (!IsValid(Component))
        {
            Component = CreatePlantBatch(ComponentName);
        }

        if (IsValid(Component) && Component->GetSkinnedAsset() != Mesh)
        {
            Component->SetSkinnedAssetAndUpdate(Mesh);
        }
    };

    EnsureBatch(
        ExistingTreeInstances,
        TEXT("CubusWorldExistingTrees"),
        ExistingTreeMesh
    );
    EnsureBatch(ElderInstances, TEXT("CubusWorldElder"), ElderMesh);
    EnsureBatch(
        NorwaySpruceInstances,
        TEXT("CubusWorldNorwaySpruce"),
        NorwaySpruceMesh
    );
    EnsureBatch(
        GreasewoodInstances,
        TEXT("CubusWorldGreasewood"),
        GreasewoodMesh
    );
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

        if (!IsValid(Chunk) || Chunk->GetOwner() != BlockWorld)
        {
            continue;
        }

        const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();

        if (ChunkData == nullptr)
        {
            continue;
        }

        ++OutLoadedChunkCount;
        Hash = HashCombineFast(Hash, GetTypeHash(Chunk->GetChunkCoordinate()));
        Hash = HashCombineFast(
            Hash,
            GetTypeHash(ChunkData->GetVegetationInstances().Num())
        );

        for (
            const FCubusVegetationInstance& Instance :
            ChunkData->GetVegetationInstances()
        )
        {
            Hash = HashCombineFast(Hash, GetTypeHash(Instance.WorldVoxel));
            Hash = HashCombineFast(Hash, GetTypeHash(Instance.TypeId));
        }
    }

    Hash = HashCombineFast(Hash, GetTypeHash(bRenderWorldPlantBatches));
    Hash = HashCombineFast(Hash, GetTypeHash(MaximumRenderedPlants));
    Hash = HashCombineFast(Hash, GetTypeHash(ExistingTreeMesh.ToSoftObjectPath()));
    Hash = HashCombineFast(Hash, GetTypeHash(ElderMesh.ToSoftObjectPath()));
    Hash = HashCombineFast(Hash, GetTypeHash(NorwaySpruceMesh.ToSoftObjectPath()));
    Hash = HashCombineFast(Hash, GetTypeHash(GreasewoodMesh.ToSoftObjectPath()));

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
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
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
        case 6:
            return ConiferTreePoints;
        default:
            return nullptr;
    }
}
