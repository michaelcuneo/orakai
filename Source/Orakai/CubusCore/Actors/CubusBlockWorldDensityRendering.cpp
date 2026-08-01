#include "CubusCore/Actors/CubusBlockWorldActor.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

void ACubusBlockWorldActor::SetVoxelRenderMode(
    const ECubusVoxelRenderMode InRenderMode,
    const bool bRebuildChunks
)
{
    VoxelRenderMode = InRenderMode;

    if (bRebuildChunks)
    {
        RebuildAllChunks();
    }
}

#if WITH_EDITOR
void ACubusBlockWorldActor::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent
)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (
        PropertyChangedEvent.GetPropertyName() ==
        GET_MEMBER_NAME_CHECKED(
            ACubusBlockWorldActor,
            VoxelRenderMode
        )
    )
    {
        RebuildAllChunks();
    }
}
#endif
