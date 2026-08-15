#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Storage/CubusDensityChunkStore.h"

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

    const ECubusVoxelRenderMode RenderMode = IsValid(ChunkDefault)
        ? ChunkDefault->GetEffectiveRenderMode()
        : ECubusVoxelRenderMode::Blocks;

    /*
     * Deliberately visible diagnostic for stale-editor/stale-DLL problems.
     * If PIE does not report generation=21 and cacheFormat=2 after the v21
     * terrain change, the running module is not the source currently checked
     * out on disk and no amount of deleting Saved/Cubus can fix that.
     */
    static bool bLoggedDensityGenerationIdentity = false;
    if (!bLoggedDensityGenerationIdentity && RenderMode == ECubusVoxelRenderMode::Density)
    {
        bLoggedDensityGenerationIdentity = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus density runtime identity: generation=%u cacheFormat=%u"),
            FCubusGenerationSeeds::CurrentGenerationVersion,
            FCubusDensityChunkStore::CurrentFormatVersion
        );
    }

    return RenderMode;
}
