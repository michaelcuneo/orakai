#include "CubusCore/Actors/CubusPCGVoxelVolumeActor.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "ProceduralMeshComponent.h"

namespace CubusTerrainRayTracingRuntime
{
    constexpr float UpdateIntervalSeconds = 0.25f;

    TAutoConsoleVariable<int32> CVarEnabled(
        TEXT("Cubus.RayTracing.Enabled"),
        0,
        TEXT("Terrain ray tracing is disabled on live procedural chunks. A stable proxy path is required."),
        ECVF_Scalability | ECVF_RenderThreadSafe
    );

    FTSTicker::FDelegateHandle TickerHandle;
    bool bReportedUnsupportedRequest = false;

    void DisableProceduralChunkRayTracing(UWorld* World)
    {
        if (!IsValid(World) || !World->IsGameWorld())
        {
            return;
        }

        const bool bRequested =
            CVarEnabled.GetValueOnGameThread() != 0;

        if (bRequested && !bReportedUnsupportedRequest)
        {
            bReportedUnsupportedRequest = true;

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus terrain ray tracing request ignored: live UProceduralMeshComponent chunks cannot safely share streamed/rebuilt geometry with the ray-tracing scene. Use the dedicated proxy path instead.")
            );
        }
        else if (!bRequested)
        {
            bReportedUnsupportedRequest = false;
        }

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

            Chunk->SetTerrainRayTracingEnabled(false);

            UProceduralMeshComponent* Mesh =
                Cast<UProceduralMeshComponent>(Chunk->GetRootComponent());

            if (IsValid(Mesh) && Mesh->bVisibleInRayTracing)
            {
                Mesh->SetVisibleInRayTracing(false);
                Mesh->MarkRenderStateDirty();
            }
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
            DisableProceduralChunkRayTracing(Context.World());
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
    // Live Cubus chunks are rebuilt and destroyed as part of normal streaming.
    // They must never enter the dynamic ray-tracing geometry path because the
    // renderer may evict their geometry independently of the actor lifetime.
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
