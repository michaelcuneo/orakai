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

        // Generation completion is no longer permission to enter gameplay.
        // The player must explicitly select and confirm a spawn location from
        // the generated terrain preview first.
        if (!FCubusGeneratedTerrainRuntime::HasConfirmedSpawn())
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

            bTravelRequested = true;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus spawn confirmed; opening gameplay level '%s' for generated seed %d"),
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
