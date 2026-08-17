// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OrakaiGameMode.generated.h"

/**
 * Simple GameMode for a third person game.
 *
 * Generated Cubus worlds prefer a designer-authored Cubus World Spawn Point
 * when one is present in the gameplay level. The block world subsequently
 * resolves the pawn vertically onto the streamed terrain surface.
 */
UCLASS(abstract)
class AOrakaiGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOrakaiGameMode();

protected:
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
