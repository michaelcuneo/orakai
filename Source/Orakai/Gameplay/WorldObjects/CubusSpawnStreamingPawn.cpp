#include "Gameplay/WorldObjects/CubusSpawnStreamingPawn.h"

#include "Components/SceneComponent.h"

ACubusSpawnStreamingPawn::ACubusSpawnStreamingPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	Root						  = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}
