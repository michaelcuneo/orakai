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

        // Streamed procedural chunk geometry is rebuilt and destroyed often.
        // Excluding it from ray tracing avoids the renderer attempting to use
        // an evicted dynamic ray-tracing geometry instance.
        ProceduralMesh->SetVisibleInRayTracing(false);
        ProceduralMesh->bUseAsyncCooking = true;
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
