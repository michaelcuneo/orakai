#include "CubusCore/Rendering/CubusVegetationRendererComponent.h"

#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

UCubusVegetationRendererComponent::UCubusVegetationRendererComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UCubusVegetationRendererComponent::OnRegister()
{
    Super::OnRegister();

    if (GrowthStartTimeSeconds < 0.0)
    {
        GrowthStartTimeSeconds =
            IsValid(GetWorld())
                ? GetWorld()->GetTimeSeconds()
                : 0.0;
    }

    EnsurePointComponents();
    EnsureTreeInstanceComponents();
    RebuildVegetation();
}

void UCubusVegetationRendererComponent::OnUnregister()
{
    ClearVegetation();
    Super::OnUnregister();
}

void UCubusVegetationRendererComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TimeUntilNextCheck -= DeltaTime;

    if (TimeUntilNextCheck > 0.0f)
    {
        return;
    }

    TimeUntilNextCheck = FMath::Max(0.1f, ChangeCheckInterval);

    const int32 GrowthStep = GetCurrentGrowthStep();
    const uint32 PlacementHash = CalculatePlacementHash();

    if (
        PlacementHash != LastPlacementHash ||
        GrowthStep != LastGrowthStep
    )
    {
        RebuildVegetation();
    }
}

void UCubusVegetationRendererComponent::RebuildVegetation()
{
    EnsurePointComponents();
    EnsureTreeInstanceComponents();
    ClearVegetation();

    ACubusVoxelVolumeActor* ChunkActor =
        Cast<ACubusVoxelVolumeActor>(GetOwner());

    if (!IsValid(ChunkActor))
    {
        return;
    }

    const FCubusBlockChunkData* ChunkData =
        ChunkActor->GetChunkData();

    if (ChunkData == nullptr)
    {
        return;
    }

    const float SafeVoxelSize =
        FMath::Max(1.0f, ChunkActor->GetVoxelSize());

    const FIntVector ChunkOriginVoxel =
        ChunkActor->GetChunkCoordinate() * Cubus::ChunkSize;

    const double HalfChunkWorldSize =
        static_cast<double>(Cubus::ChunkSize) *
        static_cast<double>(SafeVoxelSize) *
        0.5;

    TArray<FTransform> StageTransforms[4];
    TArray<int32> StageAnimationIndices[4];

    const int32 TreeLimit =
        MaxTreeInstancesPerChunk > 0
            ? MaxTreeInstancesPerChunk
            : MAX_int32;

    const int32 CurrentGrowthStep = GetCurrentGrowthStep();
    int32 AcceptedTreeCount = 0;

    for (
        const FCubusVegetationInstance& Instance :
        ChunkData->GetVegetationInstances()
    )
    {
        const FIntVector LocalVoxel =
            Instance.WorldVoxel - ChunkOriginVoxel;

        const FVector LocalLocation(
            (static_cast<double>(LocalVoxel.X) + 0.5) * SafeVoxelSize -
                HalfChunkWorldSize,
            (static_cast<double>(LocalVoxel.Y) + 0.5) * SafeVoxelSize -
                HalfChunkWorldSize,
            static_cast<double>(LocalVoxel.Z) * SafeVoxelSize -
                HalfChunkWorldSize
        );

        const FTransform LocalTransform(
            FRotator(0.0f, Instance.RotationYaw, 0.0f),
            LocalLocation,
            FVector(Instance.Scale)
        );

        if (
            UInstancedStaticMeshComponent* TargetComponent =
                ResolvePointComponentForType(Instance.TypeId)
        )
        {
            TargetComponent->AddInstance(LocalTransform, false);
            ++PublishedPointCount;
        }

        if (
            Instance.TypeId != 3 ||
            !bRenderInstancedTrees ||
            AcceptedTreeCount >= TreeLimit
        )
        {
            continue;
        }

        uint32 TreeHash = GetTypeHash(Instance.WorldVoxel);
        TreeHash = HashCombineFast(
            TreeHash,
            GetTypeHash(Instance.RotationYaw)
        );

        const int32 StartingStage =
            bSimulateTreeGrowth
                ? static_cast<int32>(TreeHash % 4u)
                : 3;

        const int32 StageIndex =
            bSimulateTreeGrowth
                ? FMath::Min(3, StartingStage + CurrentGrowthStep)
                : 3;

        StageTransforms[StageIndex].Add(LocalTransform);
        StageAnimationIndices[StageIndex].Add(0);
        ++AcceptedTreeCount;
    }

    UInstancedSkinnedMeshComponent* StageComponents[4] =
    {
        SeedlingTreeInstances,
        SaplingTreeInstances,
        YoungTreeInstances,
        MatureTreeInstances
    };

    USkeletalMesh* StageMeshes[4] =
    {
        SeedlingTreeMesh,
        SaplingTreeMesh,
        YoungTreeMesh,
        MatureTreeMesh
    };

    for (int32 StageIndex = 0; StageIndex < 4; ++StageIndex)
    {
        UInstancedSkinnedMeshComponent* StageComponent =
            StageComponents[StageIndex];
        USkeletalMesh* StageMesh = StageMeshes[StageIndex];

        if (
            !IsValid(StageComponent) ||
            !IsValid(StageMesh) ||
            StageTransforms[StageIndex].IsEmpty()
        )
        {
            continue;
        }

        StageComponent->SetSkinnedAssetAndUpdate(StageMesh);
        StageComponent->SetCullDistances(
            FMath::Max(0, TreeStartCullDistance),
            FMath::Max(TreeStartCullDistance, TreeEndCullDistance)
        );
        StageComponent->AddInstances(
            StageTransforms[StageIndex],
            StageAnimationIndices[StageIndex],
            false,
            false
        );
        StageComponent->OptimizeInstanceData(false);

        BatchedTreeInstanceCount +=
            StageTransforms[StageIndex].Num();
    }

    const UInstancedStaticMeshComponent* PointComponents[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (const UInstancedStaticMeshComponent* PointComponent : PointComponents)
    {
        if (IsValid(PointComponent))
        {
            const_cast<UInstancedStaticMeshComponent*>(PointComponent)
                ->MarkRenderStateDirty();
        }
    }

    LastGrowthStep = CurrentGrowthStep;
    LastPlacementHash = CalculatePlacementHash();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus vegetation source %s: published %d points, batched %d PVE trees, growth step %d"
        ),
        *ChunkActor->GetName(),
        PublishedPointCount,
        BatchedTreeInstanceCount,
        CurrentGrowthStep
    );
}

void UCubusVegetationRendererComponent::ClearVegetation()
{
    PublishedPointCount = 0;
    BatchedTreeInstanceCount = 0;

    UInstancedSkinnedMeshComponent* TreeComponents[] =
    {
        SeedlingTreeInstances,
        SaplingTreeInstances,
        YoungTreeInstances,
        MatureTreeInstances
    };

    for (UInstancedSkinnedMeshComponent* TreeComponent : TreeComponents)
    {
        if (IsValid(TreeComponent))
        {
            TreeComponent->ClearInstances();
        }
    }

    UInstancedStaticMeshComponent* PointComponents[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (UInstancedStaticMeshComponent* PointComponent : PointComponents)
    {
        if (IsValid(PointComponent))
        {
            PointComponent->ClearInstances();
        }
    }
}

void UCubusVegetationRendererComponent::EnsurePointComponents()
{
    if (!IsValid(GetOwner()))
    {
        return;
    }

    if (!IsValid(GrassPoints))
    {
        GrassPoints = CreatePointComponent(
            TEXT("CubusGrassPoints"),
            TEXT("Cubus.Vegetation.Grass")
        );
    }

    if (!IsValid(ShrubPoints))
    {
        ShrubPoints = CreatePointComponent(
            TEXT("CubusShrubPoints"),
            TEXT("Cubus.Vegetation.Shrub")
        );
    }

    if (!IsValid(TreePoints))
    {
        TreePoints = CreatePointComponent(
            TEXT("CubusTreePoints"),
            TEXT("Cubus.Vegetation.Tree")
        );
    }

    if (!IsValid(ReedsPoints))
    {
        ReedsPoints = CreatePointComponent(
            TEXT("CubusReedsPoints"),
            TEXT("Cubus.Vegetation.Reeds")
        );
    }

    if (!IsValid(AlpinePoints))
    {
        AlpinePoints = CreatePointComponent(
            TEXT("CubusAlpinePoints"),
            TEXT("Cubus.Vegetation.Alpine")
        );
    }

    UInstancedStaticMeshComponent* PointComponents[] =
    {
        GrassPoints,
        ShrubPoints,
        TreePoints,
        ReedsPoints,
        AlpinePoints
    };

    for (UInstancedStaticMeshComponent* PointComponent : PointComponents)
    {
        if (!IsValid(PointComponent))
        {
            continue;
        }

        PointComponent->SetStaticMesh(MarkerMesh);
        PointComponent->SetVisibility(bShowDebugMarkers, true);
        PointComponent->SetHiddenInGame(!bShowDebugMarkers, true);
    }
}

void UCubusVegetationRendererComponent::EnsureTreeInstanceComponents()
{
    if (!IsValid(SeedlingTreeInstances))
    {
        SeedlingTreeInstances =
            CreateTreeStageComponent(TEXT("CubusPVESeedlingTrees"));
    }

    if (!IsValid(SaplingTreeInstances))
    {
        SaplingTreeInstances =
            CreateTreeStageComponent(TEXT("CubusPVESaplingTrees"));
    }

    if (!IsValid(YoungTreeInstances))
    {
        YoungTreeInstances =
            CreateTreeStageComponent(TEXT("CubusPVEYoungTrees"));
    }

    if (!IsValid(MatureTreeInstances))
    {
        MatureTreeInstances =
            CreateTreeStageComponent(TEXT("CubusPVEMatureTrees"));
    }
}

int32 UCubusVegetationRendererComponent::GetCurrentGrowthStep() const
{
    if (!bSimulateTreeGrowth || GrowthStartTimeSeconds < 0.0)
    {
        return 0;
    }

    const UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return 0;
    }

    const double ElapsedSeconds =
        FMath::Max(
            0.0,
            World->GetTimeSeconds() - GrowthStartTimeSeconds
        );

    return FMath::Clamp(
        FMath::FloorToInt(
            ElapsedSeconds /
            FMath::Max(1.0f, GrowthStageDurationSeconds)
        ),
        0,
        3
    );
}

UInstancedSkinnedMeshComponent*
UCubusVegetationRendererComponent::CreateTreeStageComponent(
    const FName ComponentName
)
{
    AActor* Owner = GetOwner();

    if (!IsValid(Owner))
    {
        return nullptr;
    }

    UInstancedSkinnedMeshComponent* Component =
        NewObject<UInstancedSkinnedMeshComponent>(
            Owner,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Owner->GetRootComponent());
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(true);
    Component->SetCullDistances(
        FMath::Max(0, TreeStartCullDistance),
        FMath::Max(TreeStartCullDistance, TreeEndCullDistance)
    );
    Component->RegisterComponent();
    Owner->AddInstanceComponent(Component);

    return Component;
}

uint32 UCubusVegetationRendererComponent::CalculatePlacementHash() const
{
    const ACubusVoxelVolumeActor* ChunkActor =
        Cast<ACubusVoxelVolumeActor>(GetOwner());

    if (!IsValid(ChunkActor))
    {
        return 0;
    }

    const FCubusBlockChunkData* ChunkData =
        ChunkActor->GetChunkData();

    if (ChunkData == nullptr)
    {
        return 0;
    }

    uint32 Hash =
        GetTypeHash(ChunkData->GetVegetationInstances().Num());

    for (
        const FCubusVegetationInstance& Instance :
        ChunkData->GetVegetationInstances()
    )
    {
        Hash = HashCombineFast(Hash, GetTypeHash(Instance.WorldVoxel));
        Hash = HashCombineFast(Hash, GetTypeHash(Instance.TypeId));
        Hash = HashCombineFast(Hash, GetTypeHash(Instance.RotationYaw));
        Hash = HashCombineFast(Hash, GetTypeHash(Instance.Scale));
    }

    Hash = HashCombineFast(Hash, GetTypeHash(MarkerMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(VoxelSize));
    Hash = HashCombineFast(Hash, GetTypeHash(bShowDebugMarkers));
    Hash = HashCombineFast(Hash, GetTypeHash(bRenderInstancedTrees));
    Hash = HashCombineFast(Hash, GetTypeHash(SeedlingTreeMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(SaplingTreeMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(YoungTreeMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(MatureTreeMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(bSimulateTreeGrowth));
    Hash = HashCombineFast(Hash, GetTypeHash(GrowthStageDurationSeconds));
    Hash = HashCombineFast(Hash, GetTypeHash(MaxTreeInstancesPerChunk));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeStartCullDistance));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeEndCullDistance));

    return Hash;
}

UInstancedStaticMeshComponent*
UCubusVegetationRendererComponent::CreatePointComponent(
    const FName ComponentName,
    const FName ComponentTag
)
{
    AActor* Owner = GetOwner();

    if (!IsValid(Owner))
    {
        return nullptr;
    }

    UInstancedStaticMeshComponent* Component =
        NewObject<UInstancedStaticMeshComponent>(
            Owner,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Owner->GetRootComponent());
    Component->SetMobility(EComponentMobility::Static);
    Component->SetStaticMesh(MarkerMesh);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCastShadow(false);
    Component->SetCanEverAffectNavigation(false);
    Component->ComponentTags.AddUnique(ComponentTag);
    Component->SetVisibility(bShowDebugMarkers, true);
    Component->SetHiddenInGame(!bShowDebugMarkers, true);
    Component->RegisterComponent();
    Owner->AddInstanceComponent(Component);

    return Component;
}

UInstancedStaticMeshComponent*
UCubusVegetationRendererComponent::ResolvePointComponentForType(
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
