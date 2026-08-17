// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OrakaiGameMode.generated.h"

class APlayerController;
class ACubusSpawnStreamingPawn;

/**
 * Third-person game mode with an explicit generated-world spawn lifecycle.
 *
 * Ordinary authored levels retain the standard GameMode spawn path. When a
 * generated DEM handoff is active, the controller enters the gameplay map
 * without its character. A hidden streaming-focus pawn is created only after
 * the player confirms a DEM position; the real character is created with
 * RestartPlayerAtTransform after that location's density coverage is resident.
 */
UCLASS()
class AOrakaiGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AOrakaiGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
    void TickGeneratedWorldSpawn();

    TWeakObjectPtr<APlayerController> PendingGeneratedPlayer;
    TWeakObjectPtr<ACubusSpawnStreamingPawn> SpawnStreamingPawn;
    bool bGeneratedSpawnCompleted = false;
};
