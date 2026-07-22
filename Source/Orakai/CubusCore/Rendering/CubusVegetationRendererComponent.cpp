#include "CubusCore/Rendering/CubusVegetationRendererComponent.h"

#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

UCubusVegetationRendererComponent::UCubusVegetationRendererComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

void UCubusVegetationRendererComponent::OnRegister()
{
    Super::OnRegister();

    EnsurePointComponents();
    RebuildVegetation();
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

    const uint32 PlacementHash = CalculatePlacementHash();

    if (PlacementHash != LastPlacementHash)
    {
        RebuildVegetation();
    }
}

void UCubusVegetationRendererComponent::RebuildVegetation()
{
    EnsurePointComponents();
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
    FMath::Max(
        1.0f,
        ChunkActor->GetVoxelSize()
    );

    const FIntVector ChunkOriginVoxel =
        ChunkActor->GetChunkCoordinate() *
        Cubus::ChunkSize;

    for (
        const FCubusVegetationInstance& Instance :
        ChunkData->GetVegetationInstances()
    )
    {
        UInstancedStaticMeshComponent* TargetComponent =
            ResolvePointComponentForType(Instance.TypeId);

        if (!IsValid(TargetComponent))
        {
            continue;
        }

        const FIntVector LocalVoxel =
            Instance.WorldVoxel -
            ChunkOriginVoxel;

        const FVector LocalLocation(
            (
                static_cast<double>(LocalVoxel.X) +
                0.5
            ) * SafeVoxelSize,
            (
                static_cast<double>(LocalVoxel.Y) +
                0.5
            ) * SafeVoxelSize,
            static_cast<double>(LocalVoxel.Z) *
                SafeVoxelSize
        );

        const FTransform LocalTransform(
            FRotator(0.0f, Instance.RotationYaw, 0.0f),
            LocalLocation,
            FVector(Instance.Scale)
        );

        TargetComponent->AddInstance(LocalTransform, false);
        ++PublishedPointCount;
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

    LastPlacementHash = CalculatePlacementHash();

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus Megaplant PCG source %s: published %d points"
        ),
        *ChunkActor->GetName(),
        PublishedPointCount
    );
}

void UCubusVegetationRendererComponent::ClearVegetation()
{
    PublishedPointCount = 0;

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
            RF_Transactional
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