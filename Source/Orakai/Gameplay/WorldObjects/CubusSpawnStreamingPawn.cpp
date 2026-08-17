#include "Gameplay/WorldObjects/CubusSpawnStreamingPawn.h"

ACubusSpawnStreamingPawn::ACubusSpawnStreamingPawn()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    bAddDefaultMovementBindings = false;
}
