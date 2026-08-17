// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OrakaiGameMode.generated.h"

class APlayerController;
class ACubusSpawnStreamingPawn;

/**
 * Third-person GameMode with an explicit generated-world spawn lifecycle.
 *
 * When the world-generation Loader is present, the real player character is
 * withheld from map entry. The Loader remains in the current world, performs
 * DEM generation followed by support chunks, the full gameplay chunk pass and
 * LOD1-LOD6, and only then lets the existing WBP unlock spawn selection.
 * Confirming the WBP's SPAWN action creates and possesses the real character.
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

    // Retained for source compatibility with the earlier promotion path. The
    // Loader now owns the temporary streaming-focus pawn used before SPAWN.
    TWeakObjectPtr<ACubusSpawnStreamingPawn> SpawnStreamingPawn;
    bool bGeneratedSpawnCompleted = false;
};
