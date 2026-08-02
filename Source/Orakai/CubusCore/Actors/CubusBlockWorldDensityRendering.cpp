#include "CubusCore/Actors/CubusBlockWorldActor.h"

#include "CubusCore/Actors/CubusVoxelVolumeActor.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"

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

                // Level-authored chunks can exist before the transient runtime
                // arrays are reconstructed. Synchronize them once through the
                // same path used by an explicit render-mode change.
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
        ACubusVoxelVolumeActor* ChunkActor =
            Entry.Value.Get();

        if (IsValid(ChunkActor))
        {
            ChunksToRebuild.Add(ChunkActor);
        }
    }

    // This array is transient, while editor-authored chunk actors are not.
    // Reconstruct it from the coordinate registry so later Clear/Generate
    // operations act on the chunks that are actually in the world.
    GeneratedChunks.Reset();

    int32 RootMeshCount = 0;
    int32 RootSectionCount = 0;

    for (ACubusVoxelVolumeActor* ChunkActor : ChunksToRebuild)
    {
        if (!IsValid(ChunkActor))
        {
            continue;
        }

        GeneratedChunks.AddUnique(ChunkActor);

        ChunkActor->SetOwningBlockWorld(this);
        ChunkActor->ConfigureGenerationSeeds(
            GetGenerationSeeds()
        );
        ChunkActor->ConfigureRendering(
            MaterialRegistry
        );
        ChunkActor->ConfigureGeology(
            GeologyProfile
        );
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

        ChunkActor->RebuildVolume();

        if (UProceduralMeshComponent* TerrainMesh =
                ChunkActor->GetTerrainMeshComponent())
        {
            ++RootMeshCount;
            RootSectionCount +=
                TerrainMesh->GetNumSections();
        }
    }

    RegisteredChunkCount =
        ChunksByCoordinate.Num();
    GeneratedChunkCount =
        GeneratedChunks.Num();

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Cubus render mode %d synchronized %d chunks on %d root meshes with %d total sections."),
        static_cast<int32>(VoxelRenderMode),
        ChunksToRebuild.Num(),
        RootMeshCount,
        RootSectionCount
    );
}

#if WITH_EDITOR
void ACubusBlockWorldActor::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent
)
{
    Super::PostEditChangeProperty(
        PropertyChangedEvent
    );

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
