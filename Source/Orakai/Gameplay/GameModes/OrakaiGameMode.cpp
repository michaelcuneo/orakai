// Copyright Epic Games, Inc. All Rights Reserved.

#include "OrakaiGameMode.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusTerrainLodWorldActor.h"
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
                }
            }
        }
    }

    Super::StartPlay();
}

void AOrakaiGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    UWorld* World = GetWorld();
    if (IsValid(World))
    {
        // Never create the real character underneath the generation WBP.
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

    // After SPAWN opens Lvl_ThirdPerson, keep the real character withheld while
    // that gameplay map builds its own density chunks and LODs at the selected XY.
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

    // While still in Lvl_Generator, the WBP owns the flow. Confirm Generated
    // Spawn immediately opens Lvl_ThirdPerson, so never create a character here.
    for (TActorIterator<ACubusWorldGenerationLoaderActor> Iterator(World); Iterator; ++Iterator)
    {
        if (IsValid(*Iterator))
        {
            return;
        }
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
            TEXT("Cubus Lvl_ThirdPerson preload focus at generated spawn %.1fm, %.1fm"),
            SelectedWorldMeters.X,
            SelectedWorldMeters.Y);
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

    // This is the real second-layer loading pass in Lvl_ThirdPerson. The
    // generator map's temporary preload cannot be reused after OpenLevel.
    if (!IsValid(BlockWorld) ||
        !BlockWorld->IsWorldLoadingComplete() ||
        !BlockWorld->IsDensityStreamingCoverageReadyAtWorldLocation(StreamingFocusLocation))
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

    FActorSpawnParameters CharacterSpawnParameters;
    CharacterSpawnParameters.Owner = PlayerController;
    CharacterSpawnParameters.Instigator = StreamingPawn;
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
            TEXT("Cubus failed to create OrakaiCharacter at generated spawn %s; retaining preload pawn"),
            *FinalSpawnLocation.ToCompactString());
        return;
    }

    PlayerController->UnPossess();
    PlayerController->Possess(GeneratedCharacter);

    if (PlayerController->GetPawn() != GeneratedCharacter)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Cubus created generated character %s but possession failed; retaining preload pawn"),
            *GetNameSafe(GeneratedCharacter));
        GeneratedCharacter->Destroy();
        PlayerController->Possess(StreamingPawn);
        return;
    }

    StreamingPawn->Destroy();
    SpawnStreamingPawn.Reset();

    FInputModeGameOnly GameplayInputMode;
    PlayerController->SetInputMode(GameplayInputMode);
    PlayerController->SetShowMouseCursor(false);
    PlayerController->bEnableClickEvents = false;
    PlayerController->bEnableMouseOverEvents = false;
    PlayerController->ResetIgnoreMoveInput();
    PlayerController->ResetIgnoreLookInput();

    bGeneratedSpawnCompleted = true;
    PendingGeneratedPlayer.Reset();

    UE_LOG(LogTemp, Display,
        TEXT("Cubus Lvl_ThirdPerson generated character %s created after full selected-area load at %s"),
        *GetNameSafe(GeneratedCharacter),
        *FinalSpawnLocation.ToCompactString());
}
