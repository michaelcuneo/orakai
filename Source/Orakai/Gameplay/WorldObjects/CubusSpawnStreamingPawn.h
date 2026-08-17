#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CubusSpawnStreamingPawn.generated.h"

class USceneComponent;

/**
 * Invisible, non-colliding focus used only while a generated spawn location is
 * being promoted to gameplay LOD. It is never the player's gameplay character.
 */
UCLASS(NotBlueprintable, Transient)
class ORAKAI_API ACubusSpawnStreamingPawn : public APawn
{
	GENERATED_BODY()

public:
	ACubusSpawnStreamingPawn();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
};
