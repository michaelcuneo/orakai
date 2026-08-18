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

    // The 500 km DEM remains globally sampleable, but Cubus itself starts with
    // only the tiny spawn-area window. The ordinary gameplay view distance is
    // restored after the real character is possessed.
    ConfigureGeneratedWorldBootstrap();
}

void AOrakaiGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    UWorld* World = GetWorld();
    if (IsValid(World))
    {
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

    if (!bGeneratedBootstrapConfigured && FCubusGeneratedTerrainRuntime::IsActive())
    {
        ConfigureGeneratedWorldBootstrap();
    }

    TickGeneratedWorldSpawn();
}

void AOrakaiGameMode::ConfigureGeneratedWorldBootstrap()
{
    if (bGeneratedBootstrapConfigured || !FCubusGeneratedTerrainRuntime::IsActive())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    for (TActorIterator<ACubusBlockWorldActor> Iterator(World); Iterator; ++Iterator)
    {
        ACubusBlockWorldActor* BlockWorld = *Iterator;
        if (!IsValid(BlockWorld))
        {
            continue;
        }

        GeneratedGameplayHorizontalViewRadius = BlockWorld->GetClientHorizontalViewDistance();
        GeneratedGameplayVerticalViewRadius = BlockWorld->GetClientVerticalViewDistance();

        // Spawn staging intentionally materialises only a compact local patch.
        // Radius one is enough to give collision and immediate surroundings,
        // while the coarse LOD actor provides the broader visual context.
        BlockWorld->SetClientViewDistance(1, 1, false);
        bGeneratedBootstrapConfigured = true;

        UE_LOG(LogTemp, Display,
            TEXT("Cubus generated-world bootstrap: local radius=1/1 chunks; gameplay radius saved=%d/%d. Global 500 km DEM remains sample-only."),
            GeneratedGameplayHorizontalViewRadius,
            GeneratedGameplayVerticalViewRadius);
        return;
    }
}

void AOrakaiGameMode::PromoteGeneratedWorldStreaming()
{
    if (!bGeneratedBootstrapConfigured)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    for (TActorIterator<ACubusBlockWorldActor> Iterator(World); Iterator; ++Iterator)
    {
        ACubusBlockWorldActor* BlockWorld = *Iterator;
        if (!IsValid(BlockWorld))
        {
            continue;
        }

        BlockWorld->SetClientViewDistance(
            GeneratedGameplayHorizontalViewRadius,
            GeneratedGameplayVerticalViewRadius,
            false);

        UE_LOG(LogTemp, Display,
            TEXT("Cubus generated-world streaming promoted after spawn: gameplay radius=%d/%d chunks; coarse LOD continues outward asynchronously."),
            GeneratedGameplayHorizontalViewRadius,
            GeneratedGameplayVerticalViewRadius);
        return;
    }
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

    for (TActorIterator<ACubusWorldGenerationLoaderActor> Iterator(World); Iterator; ++Iterator)
    {
        if (IsValid(*Iterator))
        {
            return;
        }
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

    FVector2D ProposedWorldMeters;
    float ProposedHeightMeters = 0.0f;
    if (!FCubusGeneratedTerrainRuntime::GetProposedSpawnWorldMeters(ProposedWorldMeters) ||
        !FCubusGeneratedTerrainRuntime::TrySampleHeightMeters(ProposedWorldMeters, ProposedHeightMeters))
    {
        return;
    }

    const FVector ProposedFocusLocation(
        ProposedWorldMeters.X * 100.0,
        ProposedWorldMeters.Y * 100.0,
        static_cast<double>(ProposedHeightMeters) * 100.0 + 500.0);

    ACubusSpawnStreamingPawn* StreamingPawn = SpawnStreamingPawn.Get();
    if (!IsValid(StreamingPawn))
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        SpawnParameters.ObjectFlags |= RF_Transient;

        StreamingPawn = World->SpawnActor<ACubusSpawnStreamingPawn>(
            ACubusSpawnStreamingPawn::StaticClass(),
            ProposedFocusLocation,
            FRotator::ZeroRotator,
            SpawnParameters);

        if (!IsValid(StreamingPawn))
        {
            return;
        }

        SpawnStreamingPawn = StreamingPawn;
        PlayerController->Possess(StreamingPawn);

        UE_LOG(LogTemp, Display,
            TEXT("Cubus gameplay preload focus created at proposed spawn %.1fm, %.1fm"),
            ProposedWorldMeters.X,
            ProposedWorldMeters.Y);
        return;
    }

    if (!StreamingPawn->GetActorLocation().Equals(ProposedFocusLocation, 1.0))
    {
        if (IsValid(BlockWorld))
        {
            BlockWorld->ReleaseHeldPawnAtLocation(StreamingPawn, ProposedFocusLocation);
        }
        else
        {
            StreamingPawn->SetActorLocation(ProposedFocusLocation, false, nullptr, ETeleportType::TeleportPhysics);
        }
    }

    if (!FCubusGeneratedTerrainRuntime::HasConfirmedSpawn())
    {
        return;
    }

    FVector2D ConfirmedWorldMeters;
    float ConfirmedHeightMeters = 0.0f;
    if (!FCubusGeneratedTerrainRuntime::GetConfirmedSpawnWorldMeters(ConfirmedWorldMeters) ||
        !FCubusGeneratedTerrainRuntime::TryGetConfirmedSpawnSurfaceHeightMeters(ConfirmedHeightMeters))
    {
        return;
    }

    const FVector ConfirmedFocusLocation(
        ConfirmedWorldMeters.X * 100.0,
        ConfirmedWorldMeters.Y * 100.0,
        static_cast<double>(ConfirmedHeightMeters) * 100.0 + 500.0);

    if (!IsValid(BlockWorld) ||
        !BlockWorld->IsWorldLoadingComplete() ||
        !BlockWorld->IsDensityStreamingCoverageReadyAtWorldLocation(ConfirmedFocusLocation))
    {
        return;
    }

    ACubusTerrainLodWorldActor* LodWorld = nullptr;
    for (TActorIterator<ACubusTerrainLodWorldActor> Iterator(World); Iterator; ++Iterator)
    {
        if (IsValid(*Iterator))
        {
            LodWorld = *Iterator;
            break;
        }
    }

    // Only the useful near coarse context is required to enter the world.
    // LOD3-LOD6 continue building after possession; they no longer block spawn.
    if (!IsValid(LodWorld) || !LodWorld->IsPreSpawnVisualCoverageReady())
    {
        return;
    }

    FVector FinalSpawnLocation = ConfirmedFocusLocation;
    FHitResult SurfaceHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CubusGeneratedSpawnSurface), true, StreamingPawn);
    const FVector TraceStart = ConfirmedFocusLocation + FVector(0.0, 0.0, 50000.0);
    const FVector TraceEnd = ConfirmedFocusLocation - FVector(0.0, 0.0, 50000.0);
    if (World->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        FinalSpawnLocation = SurfaceHit.ImpactPoint + FVector(0.0, 0.0, 200.0);
    }
    else
    {
        FinalSpawnLocation.Z = static_cast<double>(ConfirmedHeightMeters) * 100.0 + 200.0;
    }

    FActorSpawnParameters CharacterSpawnParameters;
    CharacterSpawnParameters.Owner = PlayerController;
    CharacterSpawnParameters.Instigator = StreamingPawn;
    CharacterSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    APawn* GeneratedCharacter = World->SpawnActor<APawn>(
        DefaultPawnClass,
        FinalSpawnLocation,
        FRotator::ZeroRotator,
        CharacterSpawnParameters);

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

    // We are safely standing on the compact local terrain now. Expand the
    // gameplay window around the real player; far terrain is still streamed,
    // never instantiated across the full 500 km domain.
    PromoteGeneratedWorldStreaming();

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
        TEXT("Cubus generated character %s created in Lvl_ThirdPerson after compact selected gameplay area was ready at %s"),
        *GetNameSafe(GeneratedCharacter),
        *FinalSpawnLocation.ToCompactString());
}
