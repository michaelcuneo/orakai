#include "CubusCore/Rendering/CubusVegetationRendererComponent.h"

#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

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

    EnsureInstanceComponents();
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
    EnsureInstanceComponents();
    ClearVegetation();

    ACubusVoxelVolumeActor* ChunkActor = Cast<ACubusVoxelVolumeActor>(GetOwner());

    if (!IsValid(ChunkActor))
    {
        return;
    }

    const FCubusBlockChunkData* ChunkData = ChunkActor->GetChunkData();

    if (ChunkData == nullptr)
    {
        return;
    }

    const FTransform OwnerTransform = ChunkActor->GetActorTransform();
    const float SafeVoxelSize = FMath::Max(1.0f, VoxelSize);

    for (const FCubusVegetationInstance& Instance : ChunkData->GetVegetationInstances())
    {
        UInstancedStaticMeshComponent* TargetComponent = ResolveComponentForType(
            Instance.TypeId
        );

        if (!IsValid(TargetComponent) || !IsValid(TargetComponent->GetStaticMesh()))
        {
            continue;
        }

        const FVector WorldLocation(
            static_cast<double>(Instance.WorldVoxel.X) * SafeVoxelSize,
            static_cast<double>(Instance.WorldVoxel.Y) * SafeVoxelSize,
            static_cast<double>(Instance.WorldVoxel.Z) * SafeVoxelSize
        );

        const FVector LocalLocation = OwnerTransform.InverseTransformPosition(
            WorldLocation
        );

        const FTransform LocalTransform(
            FRotator(0.0f, Instance.RotationYaw, 0.0f),
            LocalLocation,
            FVector(Instance.Scale)
        );

        TargetComponent->AddInstance(LocalTransform, false);
        ++RenderedInstanceCount;
    }

    if (IsValid(GrassInstances))
    {
        GrassInstances->MarkRenderStateDirty();
    }
    if (IsValid(ShrubInstances))
    {
        ShrubInstances->MarkRenderStateDirty();
    }
    if (IsValid(TreeInstances))
    {
        TreeInstances->MarkRenderStateDirty();
    }
    if (IsValid(ReedsInstances))
    {
        ReedsInstances->MarkRenderStateDirty();
    }
    if (IsValid(AlpineInstances))
    {
        AlpineInstances->MarkRenderStateDirty();
    }

    LastPlacementHash = CalculatePlacementHash();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus vegetation renderer %s: rendered %d ISM instances"),
        *ChunkActor->GetName(),
        RenderedInstanceCount
    );
}

void UCubusVegetationRendererComponent::ClearVegetation()
{
    RenderedInstanceCount = 0;

    if (IsValid(GrassInstances))
    {
        GrassInstances->ClearInstances();
    }
    if (IsValid(ShrubInstances))
    {
        ShrubInstances->ClearInstances();
    }
    if (IsValid(TreeInstances))
    {
        TreeInstances->ClearInstances();
    }
    if (IsValid(ReedsInstances))
    {
        ReedsInstances->ClearInstances();
    }
    if (IsValid(AlpineInstances))
    {
        AlpineInstances->ClearInstances();
    }
}

void UCubusVegetationRendererComponent::EnsureInstanceComponents()
{
    AActor* Owner = GetOwner();

    if (!IsValid(Owner))
    {
        return;
    }

    if (!IsValid(GrassInstances))
    {
        GrassInstances = CreateInstanceComponent(
            TEXT("CubusGrassInstances"),
            GrassMesh,
            false,
            bCastSmallVegetationShadows
        );
    }
    else
    {
        GrassInstances->SetStaticMesh(GrassMesh);
    }

    if (!IsValid(ShrubInstances))
    {
        ShrubInstances = CreateInstanceComponent(
            TEXT("CubusShrubInstances"),
            ShrubMesh,
            false,
            bCastSmallVegetationShadows
        );
    }
    else
    {
        ShrubInstances->SetStaticMesh(ShrubMesh);
    }

    if (!IsValid(TreeInstances))
    {
        TreeInstances = CreateInstanceComponent(
            TEXT("CubusTreeInstances"),
            TreeMesh,
            bEnableTreeCollision,
            bCastTreeShadows
        );
    }
    else
    {
        TreeInstances->SetStaticMesh(TreeMesh);
        TreeInstances->SetCollisionEnabled(
            bEnableTreeCollision
                ? ECollisionEnabled::QueryAndPhysics
                : ECollisionEnabled::NoCollision
        );
    }

    if (!IsValid(ReedsInstances))
    {
        ReedsInstances = CreateInstanceComponent(
            TEXT("CubusReedsInstances"),
            ReedsMesh,
            false,
            bCastSmallVegetationShadows
        );
    }
    else
    {
        ReedsInstances->SetStaticMesh(ReedsMesh);
    }

    if (!IsValid(AlpineInstances))
    {
        AlpineInstances = CreateInstanceComponent(
            TEXT("CubusAlpineInstances"),
            AlpineMesh,
            false,
            bCastSmallVegetationShadows
        );
    }
    else
    {
        AlpineInstances->SetStaticMesh(AlpineMesh);
    }
}

uint32 UCubusVegetationRendererComponent::CalculatePlacementHash() const
{
    const ACubusVoxelVolumeActor* ChunkActor = Cast<ACubusVoxelVolumeActor>(GetOwner());

    if (!IsValid(ChunkActor))
    {
        return 0;
    }

    const FCubusBlockChunkData* ChunkData = ChunkActor->GetChunkData();

    if (ChunkData == nullptr)
    {
        return 0;
    }

    uint32 Hash = GetTypeHash(ChunkData->GetVegetationInstances().Num());

    for (const FCubusVegetationInstance& Instance : ChunkData->GetVegetationInstances())
    {
        Hash = HashCombineFast(Hash, GetTypeHash(Instance.WorldVoxel));
        Hash = HashCombineFast(Hash, GetTypeHash(Instance.TypeId));
        Hash = HashCombineFast(Hash, GetTypeHash(Instance.RotationYaw));
        Hash = HashCombineFast(Hash, GetTypeHash(Instance.Scale));
    }

    Hash = HashCombineFast(Hash, GetTypeHash(GrassMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(ShrubMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(TreeMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(ReedsMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(AlpineMesh));
    Hash = HashCombineFast(Hash, GetTypeHash(VoxelSize));

    return Hash;
}

UInstancedStaticMeshComponent* UCubusVegetationRendererComponent::CreateInstanceComponent(
    const FName ComponentName,
    UStaticMesh* Mesh,
    const bool bEnableCollision,
    const bool bCastShadow
)
{
    AActor* Owner = GetOwner();

    if (!IsValid(Owner))
    {
        return nullptr;
    }

    UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(
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
    Component->SetStaticMesh(Mesh);
    Component->SetCastShadow(bCastShadow);
    Component->SetCollisionEnabled(
        bEnableCollision
            ? ECollisionEnabled::QueryAndPhysics
            : ECollisionEnabled::NoCollision
    );
    Component->SetGenerateOverlapEvents(false);
    Component->RegisterComponent();
    Owner->AddInstanceComponent(Component);

    return Component;
}

UInstancedStaticMeshComponent* UCubusVegetationRendererComponent::ResolveComponentForType(
    const int32 TypeId
) const
{
    switch (TypeId)
    {
        case 1:
            return GrassInstances;
        case 2:
            return ShrubInstances;
        case 3:
            return TreeInstances;
        case 4:
            return ReedsInstances;
        case 5:
            return AlpineInstances;
        default:
            return nullptr;
    }
}