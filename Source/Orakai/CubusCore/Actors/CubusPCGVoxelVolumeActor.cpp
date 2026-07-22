#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Rendering/CubusVegetationRendererComponent.h"

#include "PCGComponent.h"
#include "PCGGraph.h"

ACubusPCGVoxelVolumeActor::ACubusPCGVoxelVolumeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    VegetationPointSource = CreateDefaultSubobject<
        UCubusVegetationRendererComponent
    >(TEXT("CubusMegaplantPointSource"));

    VegetationPCG = CreateDefaultSubobject<UPCGComponent>(
        TEXT("CubusVegetationPCG")
    );

    if (IsValid(VegetationPCG))
    {
        VegetationPCG->SetIsPartitioned(false);
        VegetationPCG->bParseActorComponents = true;
        VegetationPCG->bOnlyTrackItself = true;
    }
}

void ACubusPCGVoxelVolumeActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    ConfigurePCGComponent();

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->RebuildVegetation();
    }

    LastVegetationPlacementHash = CalculateVegetationPlacementHash();
}

void ACubusPCGVoxelVolumeActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    TimeUntilVegetationRefresh -= DeltaSeconds;

    if (TimeUntilVegetationRefresh > 0.0f)
    {
        return;
    }

    TimeUntilVegetationRefresh = FMath::Max(
        0.05f,
        VegetationRefreshInterval
    );

    ConfigurePCGComponent();

    const uint32 CurrentPlacementHash =
        CalculateVegetationPlacementHash();

    const bool bGraphChanged =
        LastConfiguredGraph != VegetationGraph;

    if (
        CurrentPlacementHash == LastVegetationPlacementHash &&
        !bGraphChanged
    )
    {
        return;
    }

    RegenerateVegetationPCG();
}

void ACubusPCGVoxelVolumeActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    CleanupVegetationPCG();
    Super::EndPlay(EndPlayReason);
}

void ACubusPCGVoxelVolumeActor::RegenerateVegetationPCG()
{
    ConfigurePCGComponent();

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->RebuildVegetation();
    }

    LastVegetationPlacementHash =
        CalculateVegetationPlacementHash();

    if (
        !bGenerateVegetationPCG ||
        !IsValid(VegetationPCG) ||
        !IsValid(VegetationGraph)
    )
    {
        CleanupVegetationPCG();
        return;
    }

    /*
     * Remove the previous chunk-owned output before rebuilding from the latest
     * deterministic Cubus point carriers. GenerateLocal keeps the operation
     * local to this chunk's component and does not create a second world grid.
     */
    VegetationPCG->CleanupLocal(true);
    VegetationPCG->GenerateLocal(true);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus PCG vegetation %s: regeneration requested for chunk (%d, %d, %d)"),
        *GetName(),
        GetChunkCoordinate().X,
        GetChunkCoordinate().Y,
        GetChunkCoordinate().Z
    );
}

void ACubusPCGVoxelVolumeActor::CleanupVegetationPCG()
{
    if (IsValid(VegetationPCG))
    {
        VegetationPCG->CleanupLocal(true);
    }
}

uint32 ACubusPCGVoxelVolumeActor::CalculateVegetationPlacementHash() const
{
    const FCubusBlockChunkData* CurrentChunkData = GetChunkData();

    if (CurrentChunkData == nullptr)
    {
        return 0;
    }

    uint32 Hash = GetTypeHash(
        CurrentChunkData->GetVegetationInstances().Num()
    );

    for (
        const FCubusVegetationInstance& Instance :
        CurrentChunkData->GetVegetationInstances()
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

    return Hash;
}

void ACubusPCGVoxelVolumeActor::ConfigurePCGComponent()
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