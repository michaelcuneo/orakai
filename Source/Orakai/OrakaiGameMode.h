// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OrakaiGameMode.generated.h"

class UOrakaiMainMenuWidget;

/**
 * Simple GameMode for a third person game.
 */
UCLASS(abstract)
class AOrakaiGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOrakaiGameMode();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOrakaiMainMenuWidget> MainMenuWidget = nullptr;
};
