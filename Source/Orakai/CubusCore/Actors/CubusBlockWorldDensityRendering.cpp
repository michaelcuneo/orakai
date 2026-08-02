#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Rendering/CubusDensityMeshComponent.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace CubusBlockWorldDensityRendering
{
    FTSTicker::FDelegateHandle SynchronizationTickerHandle;
    TSet<TWeakObjectPtr<ACubusBlockWorldActor>> SynchronizedWorldActors;

    bool IsSupportedWorld(const UWorld* World)
    {
        if (!IsValid(World) || World->bIsTearingDown)
        {
            return false;
        }

        return
            World->WorldType == EWorldType::Editor ||
            World->IsGameWorld();
    }

    bool TickSynchronization(float DeltaSeconds)
    {
        if (GEngine == nullptr)
        {
            return true;
        }

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();

            if (!IsSupportedWorld(World))
            {
                continue;
            }

            for (
                TActorIterator<ACubusBlockWorldActor> Iterator(World);
                Iterator;
                ++Iterator
            )
            {
                ACubusBlockWorldActor* BlockWorld = *Iterator;

                if (
                    !IsValid(BlockWorld) ||
                    BlockWorld->HasAnyFlags(
                        RF_ClassDefaultObject |
                        RF_ArchetypeObject
                    )
                )
                {
                    continue;
                }

                const TWeakObjectPtr<ACubusBlockWorldActor> Key(BlockWorld);

                if (SynchronizedWorldActors.Contains(Key))
                {
                    continue;
                }

                SynchronizedWorldActors.Add(Key);

                // A saved level can already contain generated chunks before
                // any editor property changes occur. Reapplying the current
                // mode once after the world is initialized guarantees those
                // chunks receive the current terrain, material, seed and
                // density configuration instead of stale constructor values.
                BlockWorld->SetVoxelRenderMode(
                    BlockWorld->GetVoxelRenderMode(),
                    true
                );
            }
        }

        for (auto Iterator = SynchronizedWorldActors.CreateIterator(); Iterator; ++Iterator)
        {
            if (!Iterator->IsValid())
            {
                Iterator.RemoveCurrent();
            }
        }

        return true;
    }

    struct FRegistration
    {
        FRegistration()
        {
            SynchronizationTickerHandle =
                FTSTicker::GetCoreTicker().AddTicker(
                    FTickerDelegate::CreateStatic(
                        &TickSynchronization
                    ),
                    0.1f
                );
        }

        ~FRegistration()
        {
            if (SynchronizationTickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(
                    SynchronizationTickerHandle
                );
            }

            SynchronizedWorldActors.Reset();
        }
    };

    FRegistration Registration;
}

void ACubusBlockWorldActor::SetVoxelRenderMode(
    const ECubusVoxelRenderMode InRenderMode,
    const bool bRebuildChunks
)
{
    VoxelRenderMode = InRenderMode;

    if (!bRebuildChunks)
    {
        return;
    }

    RefreshChunkRegistry();

    TArray<TObjectPtr<ACubusVoxelVolumeActor>> ChunksToRebuild;
    ChunksToRebuild.Reserve(ChunksByCoordinate.Num());

    for (const auto& Entry : ChunksByCoordinate)
    {
        ACubusVoxelVolumeActor* ChunkActor = Entry.Value.Get();

        if (IsValid(ChunkActor))
        {
            ChunksToRebuild.Add(ChunkActor);
        }
    }

    // GeneratedChunks is transient and can be empty after reloading an editor
    // level even though owned chunk actors still exist. Reconstruct it from the
    // authoritative coordinate registry before any Clear/Generate operation.
    GeneratedChunks.Reset();

    int32 DensityComponentCount = 0;
    int32 MissingDensityComponentCount = 0;

    for (ACubusVoxelVolumeActor* ChunkActor : ChunksToRebuild)
    {
        if (!IsValid(ChunkActor))
        {
            continue;
        }

        GeneratedChunks.AddUnique(ChunkActor);

        ChunkActor->SetOwningBlockWorld(this);
        ChunkActor->ConfigureGenerationSeeds(GetGenerationSeeds());
        ChunkActor->ConfigureRendering(MaterialRegistry);
        ChunkActor->ConfigureGeology(GeologyProfile);
        ChunkActor->ConfigureTerrain(
            bUseHeightTerrain,
            TerrainSurfaceWorldZ,
            TerrainBaseHeight,
            TerrainContinentAmplitude,
            TerrainContinentFrequency,
            TerrainHillAmplitude,
            TerrainHillFrequency,
            TerrainDetailAmplitude,
            TerrainDetailFrequency,
            TerrainRidgeAmplitude,
            TerrainRidgeFrequency,
            TerrainValleyDepth,
            TerrainValleyFrequency,
            TerrainValleyWidth,
            TerrainValleyFalloff,
            TerrainValleyWarpAmplitude,
            TerrainValleyWarpFrequency,
            TerrainRegionFrequency,
            TerrainPlainsThreshold,
            TerrainPlainsBlend,
            TerrainMountainThreshold,
            TerrainMountainBlend,
            TerrainSurfaceMaterialId,
            TerrainSubsurfaceMaterialId,
            TerrainRockMaterialId,
            TerrainSnowMaterialId,
            TerrainRockSlopeThreshold,
            TerrainSnowMinimumHeight,
            bGenerateWater,
            TerrainWaterLevel,
            TerrainWaterMaterialId
        );

        if (IsValid(ChunkActor->GetDensityMeshComponent()))
        {
            ++DensityComponentCount;
        }
        else
        {
            ++MissingDensityComponentCount;
        }

        ChunkActor->RebuildVolume();
    }

    RegisteredChunkCount = ChunksByCoordinate.Num();
    GeneratedChunkCount = GeneratedChunks.Num();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus render mode %d synchronized %d chunks; density components=%d missing=%d."),
        static_cast<int32>(VoxelRenderMode),
        ChunksToRebuild.Num(),
        DensityComponentCount,
        MissingDensityComponentCount
    );
}

#if WITH_EDITOR
void ACubusBlockWorldActor::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent
)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (
        PropertyChangedEvent.GetPropertyName() ==
        GET_MEMBER_NAME_CHECKED(
            ACubusBlockWorldActor,
            VoxelRenderMode
        )
    )
    {
        SetVoxelRenderMode(
            VoxelRenderMode,
            true
        );
    }
}
#endif
