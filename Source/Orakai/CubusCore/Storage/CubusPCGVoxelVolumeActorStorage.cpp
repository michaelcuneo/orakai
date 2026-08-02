#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"

void ACubusPCGVoxelVolumeActor::GenerateTerrainData()
{
    // Runtime chunks are configured by the world immediately after SpawnActor.
    // Apply the authoritative seed explicitly before cache lookup or terrain
    // sampling instead of depending on the global actor-spawn delegate order.
    if (ACubusBlockWorldActor* BlockWorld = GetOwningBlockWorld())
    {
        ConfigureGenerationSeeds(
            BlockWorld->GetGenerationSeeds()
        );
    }

    // The procedural mesh is about to be replaced. Remove it from the ray
    // tracing scene first; the near-field manager will restore it after the
    // world actor has rebuilt the completed mesh.
    SetTerrainRayTracingEnabled(false);

    const FIntVector Coordinate = GetChunkCoordinate();

    if (TryLoadCachedChunk())
    {
        // Vegetation is deterministic derived data and is deliberately not
        // serialized in the voxel cache.
        RegenerateVegetationData();
        RegenerateVegetationPCG();

        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("Cubus chunk cache used before generation (%d, %d, %d)"),
            Coordinate.X,
            Coordinate.Y,
            Coordinate.Z
        );
        return;
    }

    Super::GenerateTerrainData();
    RegenerateVegetationPCG();

    if (SaveCachedChunk())
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("Cubus chunk cache miss (%d, %d, %d): generated data saved"),
            Coordinate.X,
            Coordinate.Y,
            Coordinate.Z
        );
    }
}
