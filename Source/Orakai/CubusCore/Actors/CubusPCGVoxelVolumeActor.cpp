#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

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
    PrimaryActorTick.bCanEverTick = false;

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
        VegetationPointSource->SetComponentTickEnabled(
            bGenerateVegetationPCG
        );

        if (!bGenerateVegetationPCG)
        {
            VegetationPointSource->ClearVegetation();
        }
    }

    if (bRuntimeWorld && IsValid(VegetationPCG))
    {
        VegetationPCG->Deactivate();
        VegetationPCG->SetComponentTickEnabled(false);
    }
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

    // Runtime mode controls which rendering path is used. It must not override
    // the world streamer's decision to delay or disable vegetation generation.
    bGenerateVegetationPCG = bInGenerateVegetationPCG;

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->SetComponentTickEnabled(
            bGenerateVegetationPCG
        );

        if (!bGenerateVegetationPCG)
        {
            VegetationPointSource->ClearVegetation();
        }
    }

    if (bRuntimeWorld)
    {
        LastConfiguredGraph = nullptr;

        if (IsValid(VegetationPCG))
        {
            VegetationPCG->Deactivate();
            VegetationPCG->SetComponentTickEnabled(false);
        }

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
