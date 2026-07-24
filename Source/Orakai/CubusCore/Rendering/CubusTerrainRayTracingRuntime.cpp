#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"
#include "CubusCore/Rendering/CubusTerrainRayTracingProxyActor.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
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
    constexpr float ProxyRetireDelaySeconds = 0.75f;

    TAutoConsoleVariable<int32> CVarEnabled(
        TEXT("Cubus.RayTracing.Enabled"),
        1,
        TEXT("Enable stable near-field ray-tracing proxy meshes for Cubus terrain."),
        ECVF_Scalability | ECVF_RenderThreadSafe
    );

    TAutoConsoleVariable<int32> CVarHorizontalRadius(
        TEXT("Cubus.RayTracing.Radius"),
        1,
        TEXT("Horizontal chunk radius covered by stable Cubus ray-tracing proxies."),
        ECVF_Scalability | ECVF_RenderThreadSafe
    );

    TAutoConsoleVariable<int32> CVarVerticalRadius(
        TEXT("Cubus.RayTracing.VerticalRadius"),
        0,
        TEXT("Vertical chunk radius covered by stable Cubus ray-tracing proxies."),
        ECVF_Scalability | ECVF_RenderThreadSafe
    );

    TAutoConsoleVariable<int32> CVarStableChecks(
        TEXT("Cubus.RayTracing.StableChecks"),
        2,
        TEXT("Number of unchanged manager checks required before a proxy is built."),
        ECVF_Scalability
    );

    TAutoConsoleVariable<int32> CVarMaxBuildsPerUpdate(
        TEXT("Cubus.RayTracing.MaxProxyBuildsPerUpdate"),
        1,
        TEXT("Maximum number of new terrain ray-tracing proxies built per manager update."),
        ECVF_Scalability
    );

    struct FChunkProxyState
    {
        int32 ObservedRevision = INDEX_NONE;
        int32 StableChecks = 0;
        TWeakObjectPtr<ACubusTerrainRayTracingProxyActor> ActiveProxy;
    };

    struct FWorldProxyState
    {
        TMap<TWeakObjectPtr<ACubusPCGVoxelVolumeActor>, FChunkProxyState> Chunks;
    };

    FTSTicker::FDelegateHandle TickerHandle;
    TMap<TWeakObjectPtr<UWorld>, FWorldProxyState> WorldStates;

    int32 CalculateMeshRevision(UProceduralMeshComponent& Mesh)
    {
        uint32 Hash = GetTypeHash(Mesh.GetNumSections());

        for (int32 SectionIndex = 0; SectionIndex < Mesh.GetNumSections(); ++SectionIndex)
        {
            const FProcMeshSection* Section = Mesh.GetProcMeshSection(SectionIndex);

            if (Section == nullptr)
            {
                Hash = HashCombineFast(Hash, 0u);
                continue;
            }

            Hash = HashCombineFast(Hash, GetTypeHash(Section->ProcVertexBuffer.Num()));
            Hash = HashCombineFast(Hash, GetTypeHash(Section->ProcIndexBuffer.Num()));
            Hash = HashCombineFast(Hash, GetTypeHash(Section->SectionLocalBox.Min));
            Hash = HashCombineFast(Hash, GetTypeHash(Section->SectionLocalBox.Max));
        }

        return static_cast<int32>(Hash);
    }

    FIntVector ResolvePlayerChunk(const APawn& Pawn, const float VoxelSize)
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

    void RetireProxy(FChunkProxyState& State)
    {
        ACubusTerrainRayTracingProxyActor* Proxy = State.ActiveProxy.Get();

        if (IsValid(Proxy))
        {
            Proxy->BeginRetire(ProxyRetireDelaySeconds);
        }

        State.ActiveProxy.Reset();
    }

    void RetireWorld(FWorldProxyState& State)
    {
        for (TPair<TWeakObjectPtr<ACubusPCGVoxelVolumeActor>, FChunkProxyState>& Pair : State.Chunks)
        {
            RetireProxy(Pair.Value);
        }

        State.Chunks.Reset();
    }

    bool BuildProxy(
        UWorld& World,
        ACubusPCGVoxelVolumeActor& SourceChunk,
        FChunkProxyState& State
    )
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Owner = &SourceChunk;
        SpawnParameters.ObjectFlags |= RF_Transient;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ACubusTerrainRayTracingProxyActor* Proxy =
            World.SpawnActor<ACubusTerrainRayTracingProxyActor>(
                ACubusTerrainRayTracingProxyActor::StaticClass(),
                SourceChunk.GetActorTransform(),
                SpawnParameters
            );

        if (!IsValid(Proxy))
        {
            return false;
        }

        if (!Proxy->BuildFromSource(&SourceChunk, State.ObservedRevision))
        {
            Proxy->Destroy();
            return false;
        }

        State.ActiveProxy = Proxy;
        return true;
    }

    void UpdateWorld(UWorld* World)
    {
        if (!IsValid(World) || !World->IsGameWorld())
        {
            return;
        }

        const TWeakObjectPtr<UWorld> WorldKey(World);
        FWorldProxyState& WorldState = WorldStates.FindOrAdd(WorldKey);

        APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
        const bool bEnabled =
            CVarEnabled.GetValueOnGameThread() != 0 &&
            IsValid(PlayerPawn);

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

        const int32 RequiredStableChecks = FMath::Clamp(
            CVarStableChecks.GetValueOnGameThread(),
            1,
            20
        );

        const int32 MaxBuilds = FMath::Clamp(
            CVarMaxBuildsPerUpdate.GetValueOnGameThread(),
            0,
            8
        );

        int32 BuildsThisUpdate = 0;
        TSet<TWeakObjectPtr<ACubusPCGVoxelVolumeActor>> DesiredChunks;

        for (TActorIterator<ACubusPCGVoxelVolumeActor> Iterator(World); Iterator; ++Iterator)
        {
            ACubusPCGVoxelVolumeActor* Chunk = *Iterator;

            if (!IsValid(Chunk))
            {
                continue;
            }

            Chunk->SetTerrainRayTracingEnabled(false);

            if (!bEnabled)
            {
                continue;
            }

            UProceduralMeshComponent* SourceMesh =
                Cast<UProceduralMeshComponent>(Chunk->GetRootComponent());

            const FCubusBlockChunkData* ChunkData = Chunk->GetChunkData();

            if (
                !IsValid(SourceMesh) ||
                SourceMesh->GetNumSections() <= 0 ||
                ChunkData == nullptr ||
                !ChunkData->HasAnyOccupiedVoxel()
            )
            {
                continue;
            }

            const FIntVector PlayerChunk = ResolvePlayerChunk(
                *PlayerPawn,
                Chunk->GetVoxelSize()
            );

            if (!IsInsideRadius(
                Chunk->GetChunkCoordinate(),
                PlayerChunk,
                HorizontalRadius,
                VerticalRadius
            ))
            {
                continue;
            }

            const TWeakObjectPtr<ACubusPCGVoxelVolumeActor> ChunkKey(Chunk);
            DesiredChunks.Add(ChunkKey);

            FChunkProxyState& State = WorldState.Chunks.FindOrAdd(ChunkKey);
            const int32 CurrentRevision = CalculateMeshRevision(*SourceMesh);

            if (State.ObservedRevision != CurrentRevision)
            {
                State.ObservedRevision = CurrentRevision;
                State.StableChecks = 0;
                RetireProxy(State);
                continue;
            }

            State.StableChecks = FMath::Min(
                State.StableChecks + 1,
                RequiredStableChecks
            );

            ACubusTerrainRayTracingProxyActor* ActiveProxy =
                State.ActiveProxy.Get();

            if (
                IsValid(ActiveProxy) &&
                !ActiveProxy->IsRetiring() &&
                ActiveProxy->GetSourceRevision() == CurrentRevision
            )
            {
                continue;
            }

            State.ActiveProxy.Reset();

            if (
                State.StableChecks >= RequiredStableChecks &&
                BuildsThisUpdate < MaxBuilds &&
                BuildProxy(*World, *Chunk, State)
            )
            {
                ++BuildsThisUpdate;
            }
        }

        for (auto Iterator = WorldState.Chunks.CreateIterator(); Iterator; ++Iterator)
        {
            ACubusPCGVoxelVolumeActor* SourceChunk = Iterator.Key().Get();

            if (
                !IsValid(SourceChunk) ||
                !DesiredChunks.Contains(Iterator.Key())
            )
            {
                RetireProxy(Iterator.Value());
                Iterator.RemoveCurrent();
            }
        }
    }

    bool Tick(const float DeltaTime)
    {
        if (GEngine == nullptr)
        {
            return true;
        }

        TSet<TWeakObjectPtr<UWorld>> ActiveWorlds;

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();

            if (IsValid(World) && World->IsGameWorld())
            {
                const TWeakObjectPtr<UWorld> WorldKey(World);
                ActiveWorlds.Add(WorldKey);
                UpdateWorld(World);
            }
        }

        for (auto Iterator = WorldStates.CreateIterator(); Iterator; ++Iterator)
        {
            if (
                !Iterator.Key().IsValid() ||
                !ActiveWorlds.Contains(Iterator.Key())
            )
            {
                RetireWorld(Iterator.Value());
                Iterator.RemoveCurrent();
            }
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

            for (TPair<TWeakObjectPtr<UWorld>, FWorldProxyState>& Pair : WorldStates)
            {
                RetireWorld(Pair.Value);
            }

            WorldStates.Reset();
        }
    };

    FRegistration Registration;
}

void ACubusPCGVoxelVolumeActor::SetTerrainRayTracingEnabled(const bool bEnabled)
{
    bTerrainRayTracingRequested = false;

    UProceduralMeshComponent* Mesh = Cast<UProceduralMeshComponent>(
        GetRootComponent()
    );

    if (!IsValid(Mesh))
    {
        return;
    }

    if (Mesh->bVisibleInRayTracing)
    {
        Mesh->SetVisibleInRayTracing(false);
        Mesh->MarkRenderStateDirty();
    }
}
