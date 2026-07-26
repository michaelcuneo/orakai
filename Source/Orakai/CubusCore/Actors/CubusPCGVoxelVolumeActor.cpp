#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Rendering/CubusVegetationRendererComponent.h"

#include "Engine/World.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

namespace CubusPCGVoxelVolumeActor
{
    bool IsRuntimeWorld(const UObject* WorldContext)
    {
        const UWorld* World =
            IsValid(WorldContext)
                ? WorldContext->GetWorld()
                : nullptr;

        return IsValid(World) && World->IsGameWorld();
    }
}

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

    const bool bRuntimeWorld =
        CubusPCGVoxelVolumeActor::IsRuntimeWorld(this);

    if (!bRuntimeWorld)
    {
        ConfigurePCGComponent();
    }

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->SetComponentTickEnabled(true);
        VegetationPointSource->RebuildVegetation();
    }

    if (bRuntimeWorld && IsValid(VegetationPCG))
    {
        VegetationPCG->Deactivate();
        VegetationPCG->SetComponentTickEnabled(false);
    }

    LastVegetationPlacementHash =
        CalculateVegetationPlacementHash();
}

void ACubusPCGVoxelVolumeActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bGenerateVegetationPCG)
    {
        return;
    }

    TimeUntilVegetationRefresh -= DeltaSeconds;

    if (TimeUntilVegetationRefresh > 0.0f)
    {
        return;
    }

    TimeUntilVegetationRefresh = FMath::Max(
        0.05f,
        VegetationRefreshInterval
    );

    const uint32 CurrentPlacementHash =
        CalculateVegetationPlacementHash();

    const bool bRuntimeWorld =
        CubusPCGVoxelVolumeActor::IsRuntimeWorld(this);

    const bool bGraphChanged =
        !bRuntimeWorld &&
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
    if (!CubusPCGVoxelVolumeActor::IsRuntimeWorld(this))
    {
        CleanupVegetationPCG();
    }

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->ClearVegetation();
    }

    Super::EndPlay(EndPlayReason);
}

void ACubusPCGVoxelVolumeActor::ConfigureVegetationPCG(
    UPCGGraphInterface* InVegetationGraph,
    const bool bInGenerateVegetationPCG
)
{
    VegetationGraph = InVegetationGraph;

    const bool bRuntimeWorld =
        CubusPCGVoxelVolumeActor::IsRuntimeWorld(this);

    bGenerateVegetationPCG =
        bRuntimeWorld || bInGenerateVegetationPCG;

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->SetComponentTickEnabled(
            bGenerateVegetationPCG
        );

        if (bGenerateVegetationPCG)
        {
            VegetationPointSource->RebuildVegetation();
        }
        else
        {
            VegetationPointSource->ClearVegetation();
        }
    }

    SetActorTickEnabled(bGenerateVegetationPCG);

    if (bRuntimeWorld)
    {
        LastConfiguredGraph = nullptr;

        if (IsValid(VegetationPCG))
        {
            VegetationPCG->Deactivate();
            VegetationPCG->SetComponentTickEnabled(false);
        }

        LastVegetationPlacementHash = 0;
        return;
    }

    ConfigurePCGComponent();

    if (!bGenerateVegetationPCG)
    {
        CleanupVegetationPCG();
    }
}

void ACubusPCGVoxelVolumeActor::RegenerateVegetationPCG()
{
    if (!bGenerateVegetationPCG)
    {
        if (IsValid(VegetationPointSource))
        {
            VegetationPointSource->ClearVegetation();
        }

        if (!CubusPCGVoxelVolumeActor::IsRuntimeWorld(this))
        {
            CleanupVegetationPCG();
        }

        return;
    }

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->RebuildVegetation();
    }

    LastVegetationPlacementHash =
        CalculateVegetationPlacementHash();

    if (CubusPCGVoxelVolumeActor::IsRuntimeWorld(this))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus runtime vegetation %s: point renderer refreshed for chunk (%d, %d, %d)"),
            *GetName(),
            GetChunkCoordinate().X,
            GetChunkCoordinate().Y,
            GetChunkCoordinate().Z
        );
        return;
    }

    ConfigurePCGComponent();

    if (
        !IsValid(VegetationPCG) ||
        !IsValid(VegetationGraph)
    )
    {
        CleanupVegetationPCG();
        return;
    }

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
    if (
        CubusPCGVoxelVolumeActor::IsRuntimeWorld(this) ||
        !IsValid(VegetationPCG)
    )
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
