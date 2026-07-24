#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

void ACubusBlockWorldActor::ReleaseHeldPawnAtLocation(
    APawn* PlayerPawn,
    const FVector& ReleaseLocation
)
{
    if (!IsValid(PlayerPawn))
    {
        return;
    }

    TrackedPawn = PlayerPawn;
    HeldPawnLocation = ReleaseLocation;
    bPawnHeldForStreaming = false;
    bInitialSpawnAreaReady = true;

    PlayerPawn->SetActorLocation(
        ReleaseLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    PlayerPawn->SetActorEnableCollision(true);
    PlayerPawn->SetActorTickEnabled(true);

    if (ACharacter* Character = Cast<ACharacter>(PlayerPawn))
    {
        if (UCharacterMovementComponent* Movement =
            Character->GetCharacterMovement())
        {
            Movement->SetComponentTickEnabled(true);
            Movement->Activate(true);
            Movement->SetMovementMode(MOVE_Falling);
        }
    }

    // The streamed terrain mesh is centred on its chunk actor. Releasing the
    // pawn onto a high surface can move the calculated tracking coordinate into
    // the neighbouring Z layer. Keep one layer above and below loaded so the
    // surface chunk and its vegetation are not immediately queued for removal.
    VerticalViewRadius = FMath::Max(1, VerticalViewRadius);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus block world released player at Z=%.2f"),
        ReleaseLocation.Z
    );

    UpdateRuntimeStreaming(true);
}
