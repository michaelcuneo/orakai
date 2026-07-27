#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

#include "CubusCore/Rendering/CubusVegetationRendererComponent.h"

ACubusPCGVoxelVolumeActor::ACubusPCGVoxelVolumeActor()
{
    VegetationPointSource = CreateDefaultSubobject<
        UCubusVegetationRendererComponent
    >(TEXT("CubusMegaplantConfiguration"));

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->SetConfigurationOnly(true);
        VegetationPointSource->bAutoRegister = false;
        VegetationPointSource->SetAutoActivate(false);
        VegetationPointSource->SetComponentTickEnabled(false);
    }
}

void ACubusPCGVoxelVolumeActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    if (IsValid(VegetationPointSource))
    {
        VegetationPointSource->SetConfigurationOnly(true);
        VegetationPointSource->SetComponentTickEnabled(false);
    }
}

void ACubusPCGVoxelVolumeActor::ConfigureVegetationPCG(
    UPCGGraphInterface* InVegetationGraph,
    const bool bInGenerateVegetationPCG
)
{
    // Deprecated compatibility method. Intentionally ignored.
}

void ACubusPCGVoxelVolumeActor::RegenerateVegetationPCG()
{
    // Deprecated compatibility method. Intentionally ignored.
}

void ACubusPCGVoxelVolumeActor::CleanupVegetationPCG()
{
    // Deprecated compatibility method. Intentionally ignored.
}
