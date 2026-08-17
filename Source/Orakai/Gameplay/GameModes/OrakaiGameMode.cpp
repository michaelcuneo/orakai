// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiGameMode.h"

#include "Gameplay/WorldObjects/CubusWorldSpawnPoint.h"
#include "EngineUtils.h"

AOrakaiGameMode::AOrakaiGameMode()
{
	// Gameplay defaults are authored by the Blueprint subclass.
}

AActor* AOrakaiGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ACubusWorldSpawnPoint> Iterator(World); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator))
			{
				return *Iterator;
			}
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}
