#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

#include "CubusCore/Rendering/CubusDensityMeshComponent.h"

ACubusPCGVoxelVolumeActor::ACubusPCGVoxelVolumeActor()
{
    // BP_CubusVoxelPCGChunk was saved during the earlier density pass while a
    // native default subobject named "DensityMesh" existed. Its serialized
    // BodySetup export still names that subobject as its Outer. Recreate an
    // inert object with the same class and name so Unreal can load the package
    // without CreateExport failures. It does not render terrain; all block and
    // density sections are submitted to the inherited root ProceduralMesh.
    UCubusDensityMeshComponent* LegacyDensityMesh =
        CreateDefaultSubobject<UCubusDensityMeshComponent>(
            TEXT("DensityMesh")
        );

    LegacyDensityMesh->SetupAttachment(
        GetRootComponent()
    );

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
