#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"

#include "CubusWorldSpawnPoint.generated.h"

/**
 * Designer-authored spawn marker for a generated Cubus world.
 *
 * Place this actor anywhere in the gameplay level to choose the player's
 * horizontal spawn location. The Cubus block world holds the spawned pawn while
 * the surrounding density chunks stream, then releases it onto the generated
 * terrain surface at this marker's XY position.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup="Cubus", meta=(DisplayName="Cubus World Spawn Point"))
class ORAKAI_API ACubusWorldSpawnPoint : public APlayerStart
{
    GENERATED_BODY()

public:
    ACubusWorldSpawnPoint(const FObjectInitializer& ObjectInitializer);
};
