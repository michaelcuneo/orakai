#include "Gameplay/WorldObjects/CubusWorldSpawnPoint.h"

ACubusWorldSpawnPoint::ACubusWorldSpawnPoint(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
    PlayerStartTag = TEXT("CubusWorldSpawn");
#endif
}
