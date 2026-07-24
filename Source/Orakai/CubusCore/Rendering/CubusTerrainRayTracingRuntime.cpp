#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"
#include "CubusCore/Actors/CubusVoxelVolumeActor.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"

namespace CubusTerrainRayTracingRuntime
{
    constexpr float UpdateIntervalSeconds = 0.25f;

    TAutoConsoleVariable<int32> CVarEnabled(
        TEXT("Cubus.RayTracing.Enabled"),
        1,
        TEXT("Enable near-field hardware ray tracing for completed Cubus terrain chunks."),
        ECVF_Scalability | ECVF_RenderThreadSafe
    );

    TAutoConsoleVariable<int32> CVarHorizontalRadius(
        TEXT("Cubus.RayTracing.Radius"),
        1,
        TEXT("Horizontal chunk radius included in near-field Cubus terrain ray tracing."),
        ECVF_Scalability | ECVF_RenderThreadSafe
    );

    TAutoConsoleVariable<int32> CVarVerticalRadius(
        TEXT("Cubus.RayTracing.VerticalRadius"),
        0,
        TEXT("Vertical chunk radius included in near-field Cubus terrain ray tracing."),
        ECVF_Scalability | ECVF_RenderThreadSafe
    );

    FTSTicker::FDelegateHandle TickerHandle;

    bool IsInsideRadius(
        const FIntVector& Coordinate,
        const FIntVector& Centre,
        const int32 HorizontalRadius,
        const int32 VerticalRadius
    )
    {
        return
            FMath::Abs(Coordinate.X - Centre.X) <= HorizontalRadius &&
            FMath::Abs(Coordinate.Y - Centre.Y) <= HorizontalRadius &&
            FMath::Abs(Coordinate.Z - Centre.Z) <= VerticalRadius;
    }

    FIntVector ResolvePlayerChunk(
        const APawn& Pawn,
        const float VoxelSize
    )
    {
        const double ChunkWorldSize =
            static_cast<double>(Cubus::ChunkSize) *
            static_cast<double>(FMath::Max(1.0f, VoxelSize));

        const FVector Location = Pawn.GetActorLocation();

        return FIntVector(
            FMath::FloorToInt(Location.X / ChunkWorldSize),
            FMath::FloorToInt(Location.Y / ChunkWorldSize),
            FMath::FloorToInt(Location.Z / ChunkWorldSize)
        );
    }

    void UpdateWorld(UWorld* World)
    {
        if (!IsValid(World) || !World->IsGameWorld())
        {
            return;
        }

        APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
        if (!IsValid(PlayerPawn))
        {
            return;
        }

        const bool bEnabled = CVarEnabled.GetValueOnGameThread() != 0;
        const int32 HorizontalRadius = FMath::Clamp(
            CVarHorizontalRadius.GetValueOnGameThread(),
            0,
            4
        );
        const int32 VerticalRadius = FMath::Clamp(
            CVarVerticalRadius.GetValueOnGameThread(),
            0,
            2
        );

        for (
            TActorIterator<ACubusPCGVoxelVolumeActor> Iterator(World);
            Iterator;
            ++Iterator
        )
        {
            ACubusPCGVoxelVolumeActor* Chunk = *Iterator;
            if (!IsValid(Chunk))
            {
                continue;
            }

            const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();
            const bool bHasCompletedGeometry =
                ChunkData != nullptr &&
                ChunkData->GetVoxelCount() > 0 &&
                ChunkData->HasAnyOccupiedVoxel();

            const FIntVector PlayerChunk = ResolvePlayerChunk(
                *PlayerPawn,
                Chunk->GetVoxelSize()
            );

            const bool bShouldTrace =
                bEnabled &&
                bHasCompletedGeometry &&
                IsInsideRadius(
                    Chunk->GetChunkCoordinate(),
                    PlayerChunk,
                    HorizontalRadius,
                    VerticalRadius
                );

            Chunk->SetTerrainRayTracingEnabled(bShouldTrace);
        }
    }

    bool Tick(const float DeltaTime)
    {
        if (GEngine == nullptr)
        {
            return true;
        }

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UpdateWorld(Context.World());
        }

        return true;
    }

    struct FRegistration
    {
        FRegistration()
        {
            TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateStatic(&Tick),
                UpdateIntervalSeconds
            );
        }

        ~FRegistration()
        {
            if (TickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
            }
        }
    };

    FRegistration Registration;
}

void ACubusPCGVoxelVolumeActor::SetTerrainRayTracingEnabled(
    const bool bEnabled
)
{
    if (bTerrainRayTracingRequested == bEnabled)
    {
        return;
    }

    bTerrainRayTracingRequested = bEnabled;

    UProceduralMeshComponent* Mesh = Cast<UProceduralMeshComponent>(
        GetRootComponent()
    );

    if (!IsValid(Mesh))
    {
        return;
    }

    Mesh->SetVisibleInRayTracing(bEnabled);
    Mesh->MarkRenderStateDirty();
}
