#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

#include "Components/SceneComponent.h"
#include "Containers/Ticker.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"

namespace CubusVoxelChunkMobility
{
    FDelegateHandle WorldInitializationHandle;
    FDelegateHandle SpawnReleaseTickerHandle;

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

        // The Cubus block world holds the pawn by disabling both tick and
        // collision. Only intervene while the pawn is actually being held.
        if (
            PlayerPawn->IsActorTickEnabled() &&
            PlayerPawn->GetActorEnableCollision()
        )
        {
            return false;
        }

        const FVector PawnLocation = PlayerPawn->GetActorLocation();
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

        if (!bFoundSurface)
        {
            return false;
        }

        constexpr double SpawnClearance = 150.0;

        PlayerPawn->SetActorLocation(
            FVector(
                PawnLocation.X,
                PawnLocation.Y,
                HighestSurfaceZ + SpawnClearance
            ),
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

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus released held player from voxel data at terrain Z=%.2f"),
            HighestSurfaceZ
        );

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
            TryReleaseHeldPawn(Context.World());
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
