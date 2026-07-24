#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "Containers/Ticker.h"
#include "Engine/World.h"

namespace CubusGenerationSeedRuntime
{
    constexpr float CacheTickIntervalSeconds = 0.1f;
    constexpr float AutosaveIntervalSeconds = 5.0f;

    FDelegateHandle WorldInitializationHandle;
    FTSTicker::FDelegateHandle ChunkStoreTickerHandle;

    TArray<TWeakObjectPtr<ACubusVoxelVolumeActor>> PendingChunks;
    TArray<TWeakObjectPtr<ACubusVoxelVolumeActor>> ManagedChunks;
    float TimeUntilAutosave = AutosaveIntervalSeconds;

    void AddUniqueChunk(
        TArray<TWeakObjectPtr<ACubusVoxelVolumeActor>>& Chunks,
        ACubusVoxelVolumeActor* ChunkActor
    )
    {
        if (!IsValid(ChunkActor))
        {
            return;
        }

        for (const TWeakObjectPtr<ACubusVoxelVolumeActor>& Existing : Chunks)
        {
            if (Existing.Get() == ChunkActor)
            {
                return;
            }
        }

        Chunks.Add(ChunkActor);
    }

    void RemoveInvalidChunks(
        TArray<TWeakObjectPtr<ACubusVoxelVolumeActor>>& Chunks
    )
    {
        for (int32 Index = Chunks.Num() - 1; Index >= 0; --Index)
        {
            if (!Chunks[Index].IsValid())
            {
                Chunks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
            }
        }
    }

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

        // SpawnChunkAtCoordinate generates synchronously after SpawnActor
        // returns. The ticker therefore performs the cache operation on the
        // next frame, after the chunk contains its complete generated data.
        AddUniqueChunk(PendingChunks, ChunkActor);
    }

    bool TickChunkStore(const float DeltaTime)
    {
        RemoveInvalidChunks(PendingChunks);
        RemoveInvalidChunks(ManagedChunks);

        for (int32 Index = PendingChunks.Num() - 1; Index >= 0; --Index)
        {
            ACubusVoxelVolumeActor* ChunkActor = PendingChunks[Index].Get();

            if (!IsValid(ChunkActor))
            {
                PendingChunks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
                continue;
            }

            const FCubusBlockChunkData* ChunkData = ChunkActor->GetChunkData();

            if (ChunkData == nullptr || ChunkData->GetVoxelCount() <= 0)
            {
                continue;
            }

            const bool bLoadedFromCache =
                ChunkActor->TryLoadCachedChunk();

            if (bLoadedFromCache)
            {
                ChunkActor->RegenerateVegetationData();
                ChunkActor->RebuildVolume();
            }
            else
            {
                ChunkActor->SaveCachedChunk();

                const FIntVector Coordinate =
                    ChunkActor->GetChunkCoordinate();

                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT("Cubus chunk cache miss (%d, %d, %d): generated data saved"),
                    Coordinate.X,
                    Coordinate.Y,
                    Coordinate.Z
                );
            }

            AddUniqueChunk(ManagedChunks, ChunkActor);
            PendingChunks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
        }

        TimeUntilAutosave -= DeltaTime;

        if (TimeUntilAutosave <= 0.0f)
        {
            TimeUntilAutosave = AutosaveIntervalSeconds;

            for (
                const TWeakObjectPtr<ACubusVoxelVolumeActor>& WeakChunk :
                ManagedChunks
            )
            {
                ACubusVoxelVolumeActor* ChunkActor = WeakChunk.Get();

                if (IsValid(ChunkActor))
                {
                    ChunkActor->SaveCachedChunk();
                }
            }
        }

        return true;
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

            ChunkStoreTickerHandle =
                FTSTicker::GetCoreTicker().AddTicker(
                    FTickerDelegate::CreateStatic(&TickChunkStore),
                    CacheTickIntervalSeconds
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

            if (ChunkStoreTickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(
                    ChunkStoreTickerHandle
                );
            }

            PendingChunks.Reset();
            ManagedChunks.Reset();
        }
    };

    FRegistration Registration;
}
