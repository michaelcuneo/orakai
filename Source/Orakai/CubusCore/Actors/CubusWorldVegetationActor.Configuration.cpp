#include "CubusCore/Actors/CubusWorldVegetationActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "PCGGraph.h"

void ACubusWorldVegetationActor::ConfigureForWorld(
    ACubusBlockWorldActor* InBlockWorld,
    UPCGGraphInterface* InVegetationGraph,
    const bool bInEnableRuntimeVegetation
)
{
    BlockWorld = InBlockWorld;
    VegetationGraph = InVegetationGraph;
    bEnableRuntimeVegetation = bInEnableRuntimeVegetation;
    LastConfiguredGraph = nullptr;
    PublishedPlacementHash = 0;
    TimeUntilRefresh = 0.0f;

    if (HasActorBegunPlay())
    {
        RebuildWorldVegetation();
    }
}
