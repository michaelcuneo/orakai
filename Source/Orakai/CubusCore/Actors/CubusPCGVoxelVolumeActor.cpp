#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

ACubusPCGVoxelVolumeActor::ACubusPCGVoxelVolumeActor()
{
    // Chunk actors no longer own PCG or skinned vegetation components.
    // The world vegetation actor owns all vegetation render batches.
}

void ACubusPCGVoxelVolumeActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);
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
