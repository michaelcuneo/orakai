#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"
#include "CubusCore/Storage/CubusChunkStore.h"

#include "ProceduralMeshComponent.h"

namespace CubusVoxelVolumeActorStorage
{
    FCubusChunkStoreContext MakeContext(
        const FCubusBlockChunkData& ChunkData
    )
    {
        FCubusChunkStoreContext Context;
        Context.WorldSeed = ChunkData.GetGenerationSeeds().World;
        Context.GenerationVersion =
            FCubusGenerationSeeds::CurrentGenerationVersion;
        return Context;
    }
}

bool ACubusVoxelVolumeActor::TryLoadCachedChunk()
{
    EnsureChunkData();

    if (!ChunkData.IsValid())
    {
        return false;
    }

    const FCubusChunkStoreContext Context =
        CubusVoxelVolumeActorStorage::MakeContext(*ChunkData);

    if (!FCubusChunkStore::HasChunk(ChunkCoordinate, Context))
    {
        return false;
    }

    const bool bLoaded = FCubusChunkStore::LoadChunk(
        *ChunkData,
        Context
    );

    if (bLoaded)
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus chunk cache hit (%d, %d, %d), seed %lld, generation %u"),
            ChunkCoordinate.X,
            ChunkCoordinate.Y,
            ChunkCoordinate.Z,
            static_cast<long long>(Context.WorldSeed),
            Context.GenerationVersion
        );
    }

    return bLoaded;
}

bool ACubusVoxelVolumeActor::SaveCachedChunk() const
{
    if (!ChunkData.IsValid())
    {
        return false;
    }

    const FCubusChunkStoreContext Context =
        CubusVoxelVolumeActorStorage::MakeContext(*ChunkData);

    const bool bSaved = FCubusChunkStore::SaveChunk(
        *ChunkData,
        Context
    );

    if (bSaved)
    {
        UE_LOG(
            LogTemp,
            Verbose,
            TEXT("Cubus chunk cache saved (%d, %d, %d), seed %lld, generation %u"),
            ChunkCoordinate.X,
            ChunkCoordinate.Y,
            ChunkCoordinate.Z,
            static_cast<long long>(Context.WorldSeed),
            Context.GenerationVersion
        );
    }

    return bSaved;
}

void ACubusVoxelVolumeActor::RegenerateVegetationData()
{
    EnsureChunkData();

    if (!ChunkData.IsValid())
    {
        return;
    }

    FCubusBlockVegetationGenerator::Generate(
        *ChunkData,
        GeologyProfile
    );
}

void ACubusVoxelVolumeActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (
        UProceduralMeshComponent* Mesh =
            Cast<UProceduralMeshComponent>(GetRootComponent())
    )
    {
        Mesh->SetVisibleInRayTracing(false);
        Mesh->MarkRenderStateDirty();
    }

    SaveCachedChunk();
    Super::EndPlay(EndPlayReason);
}
