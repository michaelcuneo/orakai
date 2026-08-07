#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

ECubusVoxelRenderMode
ACubusBlockWorldActor::GetVoxelRenderMode() const
{
    TSubclassOf<ACubusVoxelVolumeActor> ResolvedChunkClass =
        ChunkActorClass;

    if (!ResolvedChunkClass)
    {
        ResolvedChunkClass =
            ACubusVoxelVolumeActor::StaticClass();
    }

    const ACubusVoxelVolumeActor* ChunkDefault =
        ResolvedChunkClass.GetDefaultObject();

    return IsValid(ChunkDefault)
        ? ChunkDefault->GetEffectiveRenderMode()
        : ECubusVoxelRenderMode::Blocks;
}
