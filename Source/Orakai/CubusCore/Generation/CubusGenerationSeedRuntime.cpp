#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "Engine/World.h"

namespace CubusGenerationSeedRuntime
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

        const ACubusBlockWorldActor* BlockWorld =
            Cast<ACubusBlockWorldActor>(ChunkActor->GetOwner());

        if (!IsValid(BlockWorld))
        {
            return;
        }

        ChunkActor->ConfigureGenerationSeeds(
            BlockWorld->GetGenerationSeeds()
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
