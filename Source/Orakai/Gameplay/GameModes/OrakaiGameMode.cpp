// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiGameMode.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusTerrainLodWorldActor.h"
#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Characters/OrakaiCharacter.h"
#include "Gameplay/Controllers/OrakaiPlayerController.h"
#include "Gameplay/WorldObjects/CubusSpawnStreamingPawn.h"

AOrakaiGameMode::AOrakaiGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    DefaultPawnClass = AOrakaiCharacter::StaticClass();
    PlayerControllerClass = AOrakaiPlayerController::StaticClass();
}

void AOrakaiGameMode::StartPlay()
{
    if (FCubusGeneratedTerrainRuntime::IsActive())
    {
        if (UWorld* World = GetWorld())
        {
            const int64 GeneratedSeed = FCubusGeneratedTerrainRuntime::GetWorldSeed();
            for (TActorIterator<ACubusBlockWorldActor> Iterator(World); Iterator; ++Iterator)
            {
                if (IsValid(*Iterator))
                {
                    Iterator->AdoptGeneratedWorldSeed(GeneratedSeed);
                    UE_LOG(LogTemp, Display,
                        TEXT("Cubus gameplay adopting authoritative generated DEM seed %lld before actor BeginPlay"),
                        GeneratedSeed);
                }
            }
        }
    }

    Super::StartPlay();
}

void AOrakaiGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    if (FCubusGeneratedTerrainRuntime::IsActive())
    {
        PendingGeneratedPlayer = NewPlayer;
        bGeneratedSpawnCompleted = false;
        return;
    }

    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AOrakaiGameMode::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    (void)DeltaSeconds;
    TickGeneratedWorldSpawn();
}

void AOrakaiGameMode::TickGeneratedWorldSpawn()
{
    if (bGeneratedSpawnCompleted || !FCubusGeneratedTerrainRuntime::IsActive())
    {
        return;
    }

    APlayerController* PlayerController = PendingGeneratedPlayer.Get();
    UWorld* World = GetWorld();
    if (!IsValid(PlayerController) || !IsValid(World))
    {
        return;
    }

    FVector2D SelectedWorldMeters;
    float SelectedHeightMeters = 0.0f;
    if (!FCubusGeneratedTerrainRuntime::HasConfirmedSpawn() ||
        !FCubusGeneratedTerrainRuntime::GetConfirmedSpawnWorldMeters(SelectedWorldMeters) ||
        !FCubusGeneratedTerrainRuntime::TryGetConfirmedSpawnSurfaceHeightMeters(SelectedHeightMeters))
    {
        return;
    }

    const FVector StreamingFocusLocation(
        SelectedWorldMeters.X * 100.0,
        SelectedWorldMeters.Y * 100.0,
        static_cast<double>(SelectedHeightMeters) * 100.0 + 500.0
    );

    ACubusSpawnStreamingPawn* StreamingPawn = SpawnStreamingPawn.Get();
    if (!IsValid(StreamingPawn))
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParameters.ObjectFlags |= RF_Transient;

        StreamingPawn = World->SpawnActor<ACubusSpawnStreamingPawn>(
            ACubusSpawnStreamingPawn::StaticClass(),
            StreamingFocusLocation,
            FRotator::ZeroRotator,
            SpawnParameters
        );

        if (!IsValid(StreamingPawn))
        {
            return;
        }

        SpawnStreamingPawn = StreamingPawn;
        PlayerController->Possess(StreamingPawn);

        UE_LOG(LogTemp, Display,
            TEXT("Cubus promoting selected generated spawn at %.1fm, %.1fm before character creation"),
            SelectedWorldMeters.X, SelectedWorldMeters.Y);
        return;
    }

    ACubusBlockWorldActor* BlockWorld = nullptr;
    for (TActorIterator<ACubusBlockWorldActor> Iterator(World); Iterator; ++Iterator)
    {
        if (IsValid(*Iterator))
        {
            BlockWorld = *Iterator;
            break;
        }
    }

    if (!IsValid(BlockWorld) || !BlockWorld->IsDensityStreamingCoverageReadyAtWorldLocation(StreamingFocusLocation))
    {
        return;
    }

    for (TActorIterator<ACubusTerrainLodWorldActor> Iterator(World); Iterator; ++Iterator)
    {
        if (IsValid(*Iterator) && !Iterator->IsPreSpawnVisualCoverageReady())
        {
            return;
        }
        break;
    }

    FVector FinalSpawnLocation = StreamingFocusLocation;
    FHitResult SurfaceHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CubusGeneratedSpawnSurface), true, StreamingPawn);
    const FVector TraceStart = StreamingFocusLocation + FVector(0.0, 0.0, 50000.0);
    const FVector TraceEnd = StreamingFocusLocation - FVector(0.0, 0.0, 50000.0);
    if (World->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        FinalSpawnLocation = SurfaceHit.ImpactPoint + FVector(0.0, 0.0, 200.0);
    }
    else
    {
        FinalSpawnLocation.Z = static_cast<double>(SelectedHeightMeters) * 100.0 + 200.0;
    }

    PlayerController->UnPossess();
    StreamingPawn->Destroy();
    SpawnStreamingPawn.Reset();

    RestartPlayerAtTransform(PlayerController, FTransform(FRotator::ZeroRotator, FinalSpawnLocation));

    if (IsValid(PlayerController->GetPawn()))
    {
        bGeneratedSpawnCompleted = true;
        PendingGeneratedPlayer.Reset();
        UE_LOG(LogTemp, Display,
            TEXT("Cubus generated player created after selected terrain became resident at %s"),
            *FinalSpawnLocation.ToCompactString());
    }
}
