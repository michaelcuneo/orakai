#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"

namespace CubusVoxelChunkMobility
{
    FDelegateHandle WorldInitializationHandle;

    void HandleActorSpawned(AActor* SpawnedActor)
    {
        ACubusVoxelVolumeActor* ChunkActor =
            Cast<ACubusVoxelVolumeActor>(SpawnedActor);

        if (!IsValid(ChunkActor))
        {
            return;
        }

        ChunkActor->SetGenerateCollision(true);

        USceneComponent* RootComponent =
            ChunkActor->GetRootComponent();

        if (!IsValid(RootComponent))
        {
            return;
        }

        RootComponent->SetMobility(EComponentMobility::Movable);

        UProceduralMeshComponent* ProceduralMesh =
            Cast<UProceduralMeshComponent>(RootComponent);

        if (!IsValid(ProceduralMesh))
        {
            return;
        }

        // Initial terrain spawning must not race asynchronous Chaos cooking.
        // Re-enable async cooking later once the streamer tracks collision readiness.
        ProceduralMesh->bUseAsyncCooking = false;

        // The current pawn-release logic traces on ECC_Visibility. Explicitly
        // block that channel so generated terrain can be found reliably.
        ProceduralMesh->SetCollisionProfileName(
            UCollisionProfile::BlockAllDynamic_ProfileName
        );
        ProceduralMesh->SetCollisionResponseToChannel(
            ECC_Visibility,
            ECR_Block
        );
    }

    void HandleWorldInitialization(
        UWorld* World,
        const UWorld::InitializationValues InitializationValues
    )
    {
        if (!IsValid(World))
        {
            return;
        }

        World->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateStatic(
                &HandleActorSpawned
            )
        );
    }

    struct FRegistration
    {
        FRegistration()
        {
            WorldInitializationHandle =
                FWorldDelegates::OnPostWorldInitialization.AddStatic(
                    &HandleWorldInitialization
                );
        }

        ~FRegistration()
        {
            if (WorldInitializationHandle.IsValid())
            {
                FWorldDelegates::OnPostWorldInitialization.Remove(
                    WorldInitializationHandle
                );
            }
        }
    };

    FRegistration Registration;
}
