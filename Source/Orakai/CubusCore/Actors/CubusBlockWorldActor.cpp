#include "Components/SceneComponent.h"

#include "CubusCore/Actors/CubusBlockWorldActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "Materials/MaterialInterface.h"

#include "EngineUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#endif

namespace CubusBlockWorldActor
{
    const FIntVector NeighbourOffsets[] =
    {
        FIntVector(1, 0, 0),
        FIntVector(-1, 0, 0),
        FIntVector(0, 1, 0),
        FIntVector(0, -1, 0),
        FIntVector(0, 0, 1),
        FIntVector(0, 0, -1)
    };
}

ACubusBlockWorldActor::ACubusBlockWorldActor()
{
    PrimaryActorTick.bCanEverTick = false;

    WorldRoot =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("WorldRoot")
        );

    SetRootComponent(WorldRoot);

    /*
     * Chunk procedural meshes are Static, so their attachment
     * parent must also be Static.
     */
    WorldRoot->SetMobility(
        EComponentMobility::Static
    );
}

void ACubusBlockWorldActor::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    GridDimensions.X =
        FMath::Max(1, GridDimensions.X);

    GridDimensions.Y =
        FMath::Max(1, GridDimensions.Y);

    GridDimensions.Z =
        FMath::Max(1, GridDimensions.Z);

    GeneratedVoxelSize =
        FMath::Max(
            1.0f,
            GeneratedVoxelSize
        );

    TerrainContinentAmplitude =
        FMath::Max(
            0.0f,
            TerrainContinentAmplitude
        );

    TerrainContinentFrequency =
        FMath::Max(
            0.000001f,
            TerrainContinentFrequency
        );

    TerrainHillAmplitude =
        FMath::Max(
            0.0f,
            TerrainHillAmplitude
        );

    TerrainHillFrequency =
        FMath::Max(
            0.000001f,
            TerrainHillFrequency
        );

    TerrainDetailAmplitude =
        FMath::Max(
            0.0f,
            TerrainDetailAmplitude
        );

    TerrainDetailFrequency =
        FMath::Max(
            0.000001f,
            TerrainDetailFrequency
        );

    TerrainRidgeAmplitude =
        FMath::Max(
            0.0f,
            TerrainRidgeAmplitude
        );

    TerrainRidgeFrequency =
        FMath::Max(
            0.000001f,
            TerrainRidgeFrequency
        );

    TerrainValleyDepth =
        FMath::Max(
            0.0f,
            TerrainValleyDepth
        );

    TerrainValleyFrequency =
        FMath::Max(
            0.000001f,
            TerrainValleyFrequency
        );

    TerrainValleyWidth =
        FMath::Clamp(
            TerrainValleyWidth,
            0.0f,
            1.0f
        );

    TerrainValleyFalloff =
        FMath::Clamp(
            TerrainValleyFalloff,
            0.001f,
            1.0f
        );

    TerrainValleyWarpAmplitude =
        FMath::Max(
            0.0f,
            TerrainValleyWarpAmplitude
        );

    TerrainValleyWarpFrequency =
        FMath::Max(
            0.000001f,
            TerrainValleyWarpFrequency
        );

    TerrainRegionFrequency =
        FMath::Max(
            0.000001f,
            TerrainRegionFrequency
        );

    TerrainPlainsThreshold =
        FMath::Clamp(
            TerrainPlainsThreshold,
            -1.0f,
            1.0f
        );

    TerrainPlainsBlend =
        FMath::Clamp(
            TerrainPlainsBlend,
            0.001f,
            1.0f
        );

    TerrainMountainThreshold =
        FMath::Clamp(
            TerrainMountainThreshold,
            TerrainPlainsThreshold,
            1.0f
        );

    TerrainMountainBlend =
        FMath::Clamp(
            TerrainMountainBlend,
            0.001f,
            1.0f
        );

    TerrainRockMaterialId =
        FMath::Max(
            1,
            TerrainRockMaterialId
        );

    TerrainSnowMaterialId =
        FMath::Max(
            1,
            TerrainSnowMaterialId
        );

    TerrainRockSlopeThreshold =
        FMath::Max(
            0.0f,
            TerrainRockSlopeThreshold
        );

    TerrainSurfaceMaterialId =
        FMath::Max(
            1,
            TerrainSurfaceMaterialId
        );

    TerrainSubsurfaceMaterialId =
        FMath::Max(
            1,
            TerrainSubsurfaceMaterialId
        );

    TerrainWaterMaterialId =
    FMath::Max(
        1,
        TerrainWaterMaterialId
    );

    RefreshChunkRegistry();
}

void ACubusBlockWorldActor::RegisterChunk(
    ACubusVoxelVolumeActor* ChunkActor
)
{
    if (!IsValid(ChunkActor))
    {
        return;
    }

    RemoveInvalidChunks();

    for (
        auto Iterator =
            ChunksByCoordinate.CreateIterator();
        Iterator;
        ++Iterator
    )
    {
        if (Iterator.Value().Get() == ChunkActor)
        {
            Iterator.RemoveCurrent();
        }
    }

    const FIntVector Coordinate =
        ChunkActor->GetChunkCoordinate();

    if (
        const TWeakObjectPtr<ACubusVoxelVolumeActor>*
            ExistingChunk =
                ChunksByCoordinate.Find(
                    Coordinate
                )
    )
    {
        ACubusVoxelVolumeActor* ExistingActor =
            ExistingChunk->Get();

        if (
            IsValid(ExistingActor) &&
            ExistingActor != ChunkActor
        )
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT(
                    "Cubus chunk coordinate collision at "
                    "(%d, %d, %d): %s and %s"
                ),
                Coordinate.X,
                Coordinate.Y,
                Coordinate.Z,
                *ExistingActor->GetName(),
                *ChunkActor->GetName()
            );
        }
    }

    ChunksByCoordinate.Add(
        Coordinate,
        ChunkActor
    );

    RegisteredChunkCount =
        ChunksByCoordinate.Num();
}

void ACubusBlockWorldActor::UnregisterChunk(
    ACubusVoxelVolumeActor* ChunkActor
)
{
    if (ChunkActor == nullptr)
    {
        return;
    }

    for (
        auto Iterator =
            ChunksByCoordinate.CreateIterator();
        Iterator;
        ++Iterator
    )
    {
        if (Iterator.Value().Get() == ChunkActor)
        {
            Iterator.RemoveCurrent();
        }
    }

    RegisteredChunkCount =
        ChunksByCoordinate.Num();
}

ACubusVoxelVolumeActor*
ACubusBlockWorldActor::FindChunk(
    const FIntVector& ChunkCoordinate
) const
{
    const TWeakObjectPtr<ACubusVoxelVolumeActor>*
        FoundChunk =
            ChunksByCoordinate.Find(
                ChunkCoordinate
            );

    if (FoundChunk == nullptr)
    {
        return nullptr;
    }

    ACubusVoxelVolumeActor* ChunkActor =
        FoundChunk->Get();

    return IsValid(ChunkActor)
        ? ChunkActor
        : nullptr;
}

void ACubusBlockWorldActor::RebuildChunkAtCoordinate(
    const FIntVector& ChunkCoordinate
)
{
    ACubusVoxelVolumeActor* ChunkActor =
        FindChunk(ChunkCoordinate);

    if (!IsValid(ChunkActor))
    {
        return;
    }

    ChunkActor->RebuildVolume();
}

void ACubusBlockWorldActor::RebuildChunkAndNeighbours(
    const FIntVector& ChunkCoordinate
)
{
    RebuildChunkAtCoordinate(
        ChunkCoordinate
    );

    for (
        const FIntVector& Offset :
        CubusBlockWorldActor::NeighbourOffsets
    )
    {
        RebuildChunkAtCoordinate(
            ChunkCoordinate +
            Offset
        );
    }
}

void ACubusBlockWorldActor::GenerateChunkGrid()
{
    ClearGeneratedChunks();

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    GridDimensions.X =
        FMath::Max(
            1,
            GridDimensions.X
        );

    GridDimensions.Y =
        FMath::Max(
            1,
            GridDimensions.Y
        );

    GridDimensions.Z =
        FMath::Max(
            1,
            GridDimensions.Z
        );

    GeneratedVoxelSize =
        FMath::Max(
            1.0f,
            GeneratedVoxelSize
        );

    TSubclassOf<ACubusVoxelVolumeActor>
        ResolvedChunkClass =
            ChunkActorClass;

    if (!ResolvedChunkClass)
    {
        ResolvedChunkClass =
            ACubusVoxelVolumeActor::StaticClass();
    }

    for (
        int32 Z = 0;
        Z < GridDimensions.Z;
        ++Z
    )
    {
        for (
            int32 Y = 0;
            Y < GridDimensions.Y;
            ++Y
        )
        {
            for (
                int32 X = 0;
                X < GridDimensions.X;
                ++X
            )
            {
                const FIntVector Coordinate =
                    GridOrigin +
                    FIntVector(
                        X,
                        Y,
                        Z
                    );

                    FActorSpawnParameters SpawnParameters;

                    SpawnParameters.Owner = this;

                    /*
                    * Generated chunks must live in the same level as the world actor.
                    * Actor attachment cannot persist correctly across different levels.
                    */
                    SpawnParameters.OverrideLevel =
                        GetLevel();

                    SpawnParameters.SpawnCollisionHandlingOverride =
                        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                    #if WITH_EDITOR
                    SpawnParameters.ObjectFlags |=
                        RF_Transactional;
                    #endif

                ACubusVoxelVolumeActor* ChunkActor =
                    World->SpawnActor<
                        ACubusVoxelVolumeActor
                    >(
                        ResolvedChunkClass,
                        FVector::ZeroVector,
                        FRotator::ZeroRotator,
                        SpawnParameters
                    );

                if (!IsValid(ChunkActor))
                {
                    continue;
                }

                GeneratedChunks.Add(
                    ChunkActor
                );

                ChunkActor->ConfigureGeneratedChunk(
                    Coordinate,
                    GeneratedVoxelSize,
                    this
                );

                ChunkActor->ConfigureRendering(
                    MaterialRegistry,
                    FallbackVoxelMaterial
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

                ChunkActor->SetOwner(this);

                #if WITH_EDITOR

                FText ParentingFailureReason;

                if (
                    GEditor != nullptr &&
                    GEditor->CanParentActors(
                        this,
                        ChunkActor,
                        &ParentingFailureReason
                    )
                )
                {
                    Modify();
                    ChunkActor->Modify();

                    GEditor->ParentActors(
                        this,
                        ChunkActor,
                        NAME_None,
                        WorldRoot
                    );

                    ChunkActor->PostEditMove(true);

                    MarkPackageDirty();
                    ChunkActor->MarkPackageDirty();
                }
                else
                {
                    UE_LOG(
                        LogTemp,
                        Error,
                        TEXT(
                            "Cubus could not parent chunk %s to %s: %s"
                        ),
                        *ChunkActor->GetName(),
                        *GetName(),
                        *ParentingFailureReason.ToString()
                    );
                }

                #else

                ChunkActor->AttachToComponent(
                    WorldRoot,
                    FAttachmentTransformRules::KeepWorldTransform
                );

                #endif
            }
        }
    }

    GeneratedChunkCount =
        GeneratedChunks.Num();

    RefreshChunkRegistry();
    RegenerateTerrain();
}

void ACubusBlockWorldActor::ClearGeneratedChunks()
{
    for (
        ACubusVoxelVolumeActor* ChunkActor :
        GeneratedChunks
    )
    {
        if (!IsValid(ChunkActor))
        {
            continue;
        }

        UnregisterChunk(
            ChunkActor
        );

        ChunkActor->Destroy();
    }

    GeneratedChunks.Reset();

    GeneratedChunkCount = 0;

    RefreshChunkRegistry();
}

void ACubusBlockWorldActor::RefreshChunkRegistry()
{
    ChunksByCoordinate.Reset();

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        RegisteredChunkCount = 0;
        return;
    }

    for (
        TActorIterator<ACubusVoxelVolumeActor>
            Iterator(World);
        Iterator;
        ++Iterator
    )
    {
        ACubusVoxelVolumeActor* ChunkActor =
            *Iterator;

        if (!IsValid(ChunkActor))
        {
            continue;
        }

        const bool bOwnedByThisWorld =
            ChunkActor->GetOwner() == this;

        const bool bAttachedToThisWorld =
            ChunkActor->GetAttachParentActor() == this;

        if (
            !bOwnedByThisWorld &&
            !bAttachedToThisWorld
        )
        {
            continue;
        }

        ChunkActor->SetOwningBlockWorld(
            this
        );

        RegisterChunk(
            ChunkActor
        );
    }

    RemoveInvalidChunks();

    RegisteredChunkCount =
        ChunksByCoordinate.Num();
}

void ACubusBlockWorldActor::RebuildAllChunks()
{
    RefreshChunkRegistry();

    for (
        const TPair<
            FIntVector,
            TWeakObjectPtr<ACubusVoxelVolumeActor>
        >& Entry :
        ChunksByCoordinate
    )
    {
        ACubusVoxelVolumeActor* ChunkActor =
            Entry.Value.Get();

        if (IsValid(ChunkActor))
        {
            ChunkActor->RebuildVolume();
        }
    }
}

void ACubusBlockWorldActor::RegenerateTerrain()
{
    RefreshChunkRegistry();

    for (
        const TPair<
            FIntVector,
            TWeakObjectPtr<ACubusVoxelVolumeActor>
        >& Entry :
        ChunksByCoordinate
    )
    {
        ACubusVoxelVolumeActor* ChunkActor =
            Entry.Value.Get();

        if (!IsValid(ChunkActor))
        {
            continue;
        }

        ChunkActor->ConfigureRendering(
            MaterialRegistry,
            FallbackVoxelMaterial
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
    }

    for (
        const TPair<
            FIntVector,
            TWeakObjectPtr<ACubusVoxelVolumeActor>
        >& Entry :
        ChunksByCoordinate
    )
    {
        ACubusVoxelVolumeActor* ChunkActor =
            Entry.Value.Get();

        if (IsValid(ChunkActor))
        {
            ChunkActor->GenerateTestShapeData();
        }
    }

    RebuildAllChunks();
}

void ACubusBlockWorldActor::RemoveInvalidChunks()
{
    for (
        auto Iterator =
            ChunksByCoordinate.CreateIterator();
        Iterator;
        ++Iterator
    )
    {
        if (!Iterator.Value().IsValid())
        {
            Iterator.RemoveCurrent();
        }
    }

    RegisteredChunkCount =
        ChunksByCoordinate.Num();
}