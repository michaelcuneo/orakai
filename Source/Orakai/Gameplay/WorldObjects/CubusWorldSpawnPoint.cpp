#include "Gameplay/WorldObjects/CubusWorldSpawnPoint.h"

ACubusWorldSpawnPoint::ACubusWorldSpawnPoint(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PlayerStartTag = TEXT("CubusWorldSpawn");
}
