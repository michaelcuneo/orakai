#include "CubusCore/Generation/CubusWorldGenerationLoaderActor.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/CoreUObjectDelegates.h"

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

            bTravelRequested = true;
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus generation complete; opening gameplay level '%s' for seed %d"),
                *Loader->GameplayLevelName.ToString(),
                Loader->GetWorldSeed()
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
