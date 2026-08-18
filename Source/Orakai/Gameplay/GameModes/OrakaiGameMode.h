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
 * The 500 km DEM remains a cheap, deterministic global source surface. It is
 * never materialised as 500 km of Cubus chunks. During spawn selection we keep
 * only a deliberately small local voxel window resident around the selected
 * location, plus the first couple of coarse visual LOD rings. After the real
 * character is possessed, the normal player view distance is restored and the
 * remaining coarse rings continue streaming outward in the background.
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
    void ConfigureGeneratedWorldBootstrap();
    void PromoteGeneratedWorldStreaming();

    TWeakObjectPtr<APlayerController> PendingGeneratedPlayer;
    TWeakObjectPtr<ACubusSpawnStreamingPawn> SpawnStreamingPawn;

    int32 GeneratedGameplayHorizontalViewRadius = 4;
    int32 GeneratedGameplayVerticalViewRadius = 2;
    bool bGeneratedBootstrapConfigured = false;
    bool bGeneratedSpawnCompleted = false;
};
