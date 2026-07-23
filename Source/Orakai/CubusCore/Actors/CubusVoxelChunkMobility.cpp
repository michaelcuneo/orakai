#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

#include "Components/SceneComponent.h"
#include "Containers/Ticker.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"

namespace CubusVoxelChunkMobility
{
    FDelegateHandle WorldInitializationHandle;
    FTSTicker::FDelegateHandle SpawnReleaseTickerHandle;

    void RestoreRuntimeChunkGeology(UWorld* World)
    {
        if (!IsValid(World) || !World->IsGameWorld())
        {
            return;
        }

        for (
            TActorIterator<ACubusVoxelVolumeActor> Iterator(World);
            Iterator;
            ++Iterator
        )
        {
            ACubusVoxelVolumeActor* ChunkActor = *Iterator;

            if (
                !IsValid(ChunkActor) ||
                !ChunkActor->RestoreClassDefaultGeologyProfile()
            )
            {
                continue;
            }

            ChunkActor->GenerateTestShapeData();
            ChunkActor->RebuildVolume();

            if (
                ACubusPCGVoxelVolumeActor* PCGChunk =
                    Cast<ACubusPCGVoxelVolumeActor>(ChunkActor)
            )
            {
                PCGChunk->RegenerateVegetationPCG();
            }

            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus restored Blueprint geology profile for runtime chunk (%d, %d, %d)"),
                ChunkActor->GetChunkCoordinate().X,
                ChunkActor->GetChunkCoordinate().Y,
                ChunkActor->GetChunkCoordinate().Z
            );
        }
    }

    bool TryReleaseHeldPawn(UWorld* World)
    {
        if (!IsValid(World) || !World->IsGameWorld())
        {
            return false;
        }

        APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);

        if (!IsValid(PlayerPawn))
        {
            return false;
        }

        if (
            PlayerPawn->IsActorTickEnabled() &&
            PlayerPawn->GetActorEnableCollision()
        )
        {
            return false;
        }

        ACubusBlockWorldActor* BlockWorld = nullptr;

        for (
            TActorIterator<ACubusBlockWorldActor> Iterator(World);
            Iterator;
            ++Iterator
        )
        {
            if (IsValid(*Iterator))
            {
                BlockWorld = *Iterator;
                break;
            }
        }

        if (!IsValid(BlockWorld))
        {
            return false;
        }

        const FVector PawnLocation = PlayerPawn->GetActorLocation();
        bool bHasGeneratedChunk = false;
        bool bFoundSurface = false;
        double HighestSurfaceZ = -DBL_MAX;

        for (
            TActorIterator<ACubusVoxelVolumeActor> Iterator(World);
            Iterator;
            ++Iterator
        )
        {
            ACubusVoxelVolumeActor* ChunkActor = *Iterator;

            if (!IsValid(ChunkActor))
            {
                continue;
            }

            bHasGeneratedChunk = true;

            const FCubusBlockChunkData* ChunkData =
                ChunkActor->GetChunkData();

            if (
                ChunkData == nullptr ||
                !ChunkData->HasAnyOccupiedVoxel()
            )
            {
                continue;
            }

            const double VoxelSize =
                static_cast<double>(FMath::Max(1.0f, ChunkActor->GetVoxelSize()));

            const double HalfChunkWorldSize =
                static_cast<double>(Cubus::ChunkSize) * VoxelSize * 0.5;

            const FVector ChunkLocation = ChunkActor->GetActorLocation();

            const int32 LocalX = FMath::FloorToInt(
                (PawnLocation.X - ChunkLocation.X + HalfChunkWorldSize) /
                VoxelSize
            );

            const int32 LocalY = FMath::FloorToInt(
                (PawnLocation.Y - ChunkLocation.Y + HalfChunkWorldSize) /
                VoxelSize
            );

            if (
                LocalX < 0 || LocalX >= Cubus::ChunkSize ||
                LocalY < 0 || LocalY >= Cubus::ChunkSize
            )
            {
                continue;
            }

            for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
            {
                if (ChunkData->IsEmpty(LocalX, LocalY, LocalZ))
                {
                    continue;
                }

                const double SurfaceZ =
                    ChunkLocation.Z +
                    ((static_cast<double>(LocalZ) + 1.0) * VoxelSize) -
                    HalfChunkWorldSize;

                if (!bFoundSurface || SurfaceZ > HighestSurfaceZ)
                {
                    HighestSurfaceZ = SurfaceZ;
                    bFoundSurface = true;
                }

                break;
            }
        }

        if (!bHasGeneratedChunk)
        {
            return false;
        }

        const FVector ReleaseLocation = bFoundSurface
            ? FVector(
                PawnLocation.X,
                PawnLocation.Y,
                HighestSurfaceZ + 150.0
            )
            : FVector(
                PawnLocation.X,
                PawnLocation.Y,
                PawnLocation.Z + 200.0
            );

        BlockWorld->ReleaseHeldPawnAtLocation(
            PlayerPawn,
            ReleaseLocation
        );

        if (!bFoundSurface)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus released player without an exact voxel surface; gravity fallback used at Z=%.2f"),
                ReleaseLocation.Z
            );
        }

        return true;
    }

    bool TickSpawnRelease(float DeltaTime)
    {
        if (GEngine == nullptr)
        {
            return true;
        }

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            RestoreRuntimeChunkGeology(World);
            TryReleaseHeldPawn(World);
        }

        return true;
    }

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

        ProceduralMesh->bUseAsyncCooking = false;
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

            SpawnReleaseTickerHandle =
                FTSTicker::GetCoreTicker().AddTicker(
                    FTickerDelegate::CreateStatic(&TickSpawnRelease),
                    0.1f
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

            if (SpawnReleaseTickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(
                    SpawnReleaseTickerHandle
                );
            }
        }
    };

    FRegistration Registration;
}
