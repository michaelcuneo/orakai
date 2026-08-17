#include "CubusCore/Generation/CubusWorldGenerationLoaderActor.h"
#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

namespace CubusWorldGenerationTravelRuntime
{
    FDelegateHandle WorldPostActorTickHandle;
    FDelegateHandle PostLoadMapHandle;
    bool bTravelRequested = false;

    void HandlePostLoadMap(UWorld* LoadedWorld)
    {
        (void)LoadedWorld;
        bTravelRequested = false;
    }

    void HandleWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
    {
        (void)TickType;
        (void)DeltaSeconds;

        if (bTravelRequested || !IsValid(World) || !World->IsGameWorld())
        {
            return;
        }

        for (TActorIterator<ACubusWorldGenerationLoaderActor> Iterator(World); Iterator; ++Iterator)
        {
            ACubusWorldGenerationLoaderActor* Loader = *Iterator;
            if (!IsValid(Loader) ||
                Loader->GetGenerationStage() != ECubusGenerationLoaderStage::Complete ||
                !Loader->bTravelToGameplayWhenComplete ||
                Loader->GameplayLevelName.IsNone())
            {
                continue;
            }

            // Persist the exact final DEM preview before OpenLevel. Gameplay
            // enters with no pawn, builds DEM-derived voxel/LOD coverage, and
            // uses this retained preview for the explicit spawn selection.
            Loader->PublishPreviewSnapshotToRuntime();

            bTravelRequested = true;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus generation complete; opening generated gameplay level '%s' for seed %d before player spawn"),
                *Loader->GameplayLevelName.ToString(),
                FCubusGeneratedTerrainRuntime::GetWorldSeed()
            );
            UGameplayStatics::OpenLevel(World, Loader->GameplayLevelName);
            return;
        }
    }

    struct FRegistration
    {
        FRegistration()
        {
            WorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddStatic(
                &HandleWorldPostActorTick
            );
            PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(
                &HandlePostLoadMap
            );
        }

        ~FRegistration()
        {
            if (WorldPostActorTickHandle.IsValid())
            {
                FWorldDelegates::OnWorldPostActorTick.Remove(WorldPostActorTickHandle);
            }
            if (PostLoadMapHandle.IsValid())
            {
                FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
            }
        }
    };

    FRegistration Registration;
}
