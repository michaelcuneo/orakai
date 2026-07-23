#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"

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

        USceneComponent* RootComponent =
            ChunkActor->GetRootComponent();

        if (IsValid(RootComponent))
        {
            RootComponent->SetMobility(EComponentMobility::Movable);
        }
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
