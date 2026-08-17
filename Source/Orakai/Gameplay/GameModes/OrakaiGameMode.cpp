// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiGameMode.h"

#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"
#include "CubusCore/Generation/CubusWorldGenerationLoaderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
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
    Super::StartPlay();
}

void AOrakaiGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    UWorld* World = GetWorld();
    if (IsValid(World))
    {
        // The world-generation screen owns the complete pre-spawn lifecycle.
        // Hold the real character even before StartGeneration configures the
        // generated DEM runtime, otherwise GameMode would create a character
        // underneath the WBP at map entry.
        for (TActorIterator<ACubusWorldGenerationLoaderActor> Iterator(World); Iterator; ++Iterator)
        {
            if (IsValid(*Iterator))
            {
                PendingGeneratedPlayer = NewPlayer;
                bGeneratedSpawnCompleted = false;
                return;
            }
        }
    }

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

    // The Loader's Complete stage is deliberately AFTER:
    //   DEM generation -> support chunks -> full gameplay chunks -> LOD1-LOD6.
    // Never let a confirmed proposal create the player before that state.
    ACubusWorldGenerationLoaderActor* Loader = nullptr;
    for (TActorIterator<ACubusWorldGenerationLoaderActor> Iterator(World); Iterator; ++Iterator)
    {
        if (IsValid(*Iterator))
        {
            Loader = *Iterator;
            break;
        }
    }
    if (IsValid(Loader) && !Loader->IsGeneratedSpawnSelectionReady())
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

    const FVector DemSpawnLocation(
        SelectedWorldMeters.X * 100.0,
        SelectedWorldMeters.Y * 100.0,
        static_cast<double>(SelectedHeightMeters) * 100.0 + 200.0
    );

    FVector FinalSpawnLocation = DemSpawnLocation;
    FHitResult SurfaceHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CubusGeneratedSpawnSurface), true, PlayerController->GetPawn());
    const FVector TraceStart = DemSpawnLocation + FVector(0.0, 0.0, 50000.0);
    const FVector TraceEnd = DemSpawnLocation - FVector(0.0, 0.0, 50000.0);
    if (World->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        FinalSpawnLocation = SurfaceHit.ImpactPoint + FVector(0.0, 0.0, 200.0);
    }

    APawn* PreviousPawn = PlayerController->GetPawn();

    FActorSpawnParameters CharacterSpawnParameters;
    CharacterSpawnParameters.Owner = PlayerController;
    CharacterSpawnParameters.Instigator = PreviousPawn;
    CharacterSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    APawn* GeneratedCharacter = World->SpawnActor<APawn>(
        DefaultPawnClass,
        FinalSpawnLocation,
        FRotator::ZeroRotator,
        CharacterSpawnParameters
    );

    if (!IsValid(GeneratedCharacter))
    {
        UE_LOG(LogTemp, Error,
            TEXT("Cubus failed to create OrakaiCharacter at confirmed generated spawn %s"),
            *FinalSpawnLocation.ToCompactString());
        return;
    }

    PlayerController->UnPossess();
    PlayerController->Possess(GeneratedCharacter);

    if (PlayerController->GetPawn() != GeneratedCharacter)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Cubus created generated character %s but possession failed"),
            *GetNameSafe(GeneratedCharacter));
        GeneratedCharacter->Destroy();
        if (IsValid(PreviousPawn))
        {
            PlayerController->Possess(PreviousPawn);
        }
        return;
    }

    // The hidden streaming-focus pawn existed only to run the support/full/LOD
    // passes before the WBP enabled SPAWN. It is no longer needed once the real
    // character is successfully possessed.
    if (IsValid(PreviousPawn) && PreviousPawn->IsA<ACubusSpawnStreamingPawn>())
    {
        PreviousPawn->Destroy();
    }

    SpawnStreamingPawn.Reset();
    bGeneratedSpawnCompleted = true;
    PendingGeneratedPlayer.Reset();

    UE_LOG(LogTemp, Display,
        TEXT("Cubus SPAWN confirmed: created and possessed %s at %s after all preload passes were already complete"),
        *GetNameSafe(GeneratedCharacter),
        *FinalSpawnLocation.ToCompactString());
}
