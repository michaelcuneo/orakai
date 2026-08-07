#include "CubusCore/Actors/CubusWorldVegetationActor.h"

#include "CubusCore/Vegetation/CubusVegetationWindUtilities.h"

#include "Components/ActorComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/UObjectIterator.h"

namespace CubusWorldVegetationWindLifecycle
{
    FTSTicker::FDelegateHandle LifecycleTickerHandle;

    TMap<
        TWeakObjectPtr<ACubusWorldVegetationActor>,
        uint32
    > LastTargetSignatures;

    uint32 AddObjectToSignature(
        uint32 Signature,
        const UObject* Object
    )
    {
        return HashCombineFast(
            Signature,
            IsValid(Object)
                ? GetTypeHash(Object->GetUniqueID())
                : 0u
        );
    }

    uint32 CalculateTargetSignature(
        ACubusWorldVegetationActor* VegetationActor
    )
    {
        if (!IsValid(VegetationActor))
        {
            return 0;
        }

        uint32 Signature =
            GetTypeHash(VegetationActor->GetUniqueID());

        TInlineComponentArray<UActorComponent*> Components(
            VegetationActor
        );

        Signature = HashCombineFast(
            Signature,
            GetTypeHash(Components.Num())
        );

        for (UActorComponent* Component : Components)
        {
            if (!IsValid(Component))
            {
                continue;
            }

            Signature = AddObjectToSignature(
                Signature,
                Component
            );

            Signature = HashCombineFast(
                Signature,
                GetTypeHash(Component->GetClass()->GetFName())
            );

            if (const UMeshComponent* MeshComponent =
                    Cast<UMeshComponent>(Component))
            {
                const int32 MaterialCount =
                    MeshComponent->GetNumMaterials();

                Signature = HashCombineFast(
                    Signature,
                    GetTypeHash(MaterialCount)
                );

                for (
                    int32 MaterialIndex = 0;
                    MaterialIndex < MaterialCount;
                    ++MaterialIndex
                )
                {
                    Signature = AddObjectToSignature(
                        Signature,
                        MeshComponent->GetMaterial(
                            MaterialIndex
                        )
                    );
                }
            }

            if (const UInstancedSkinnedMeshComponent* SkinnedBatch =
                    Cast<UInstancedSkinnedMeshComponent>(Component))
            {
                Signature = AddObjectToSignature(
                    Signature,
                    SkinnedBatch->GetTransformProvider()
                );

                Signature = AddObjectToSignature(
                    Signature,
                    SkinnedBatch->GetSkinnedAsset()
                );
            }
        }

        UWorld* World = VegetationActor->GetWorld();

        if (IsValid(World))
        {
            Signature = AddObjectToSignature(
                Signature,
                FCubusVegetationWindUtilities::
                    ResolveUltraDynamicWeatherActor(World)
            );

            Signature = AddObjectToSignature(
                Signature,
                FCubusVegetationWindUtilities::
                    ResolveGlobalFoliageActor(World)
            );
        }

        int32 CollectionCount = 0;

        for (
            TObjectIterator<UMaterialParameterCollection> Iterator;
            Iterator;
            ++Iterator
        )
        {
            UMaterialParameterCollection* Collection =
                *Iterator;

            if (!IsValid(Collection))
            {
                continue;
            }

            ++CollectionCount;

            Signature = AddObjectToSignature(
                Signature,
                Collection
            );
        }

        Signature = HashCombineFast(
            Signature,
            GetTypeHash(CollectionCount)
        );

        return Signature;
    }

    bool TickLifecycle(float DeltaSeconds)
    {
        (void)DeltaSeconds;

        if (GEngine == nullptr)
        {
            return true;
        }

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();

            if (
                !IsValid(World) ||
                !World->IsGameWorld() ||
                World->bIsTearingDown
            )
            {
                continue;
            }

            for (
                TActorIterator<ACubusWorldVegetationActor> Iterator(World);
                Iterator;
                ++Iterator
            )
            {
                ACubusWorldVegetationActor* VegetationActor =
                    *Iterator;

                if (!IsValid(VegetationActor))
                {
                    continue;
                }

                const uint32 CurrentSignature =
                    CalculateTargetSignature(
                        VegetationActor
                    );

                const TWeakObjectPtr<ACubusWorldVegetationActor> Key(
                    VegetationActor
                );

                const uint32* PreviousSignature =
                    LastTargetSignatures.Find(Key);

                if (
                    PreviousSignature != nullptr &&
                    *PreviousSignature == CurrentSignature
                )
                {
                    continue;
                }

                LastTargetSignatures.Add(
                    Key,
                    CurrentSignature
                );

                VegetationActor
                    ->InvalidateDynamicWindBridgeTargets();

                UE_LOG(
                    LogTemp,
                    Verbose,
                    TEXT("Cubus foliage target topology changed; wind/weather bridge will republish on the next vegetation tick.")
                );
            }
        }

        for (
            auto Iterator = LastTargetSignatures.CreateIterator();
            Iterator;
            ++Iterator
        )
        {
            if (!Iterator->Key.IsValid())
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
            LifecycleTickerHandle =
                FTSTicker::GetCoreTicker().AddTicker(
                    FTickerDelegate::CreateStatic(
                        &TickLifecycle
                    ),
                    0.25f
                );
        }

        ~FRegistration()
        {
            if (LifecycleTickerHandle.IsValid())
            {
                FTSTicker::GetCoreTicker().RemoveTicker(
                    LifecycleTickerHandle
                );
            }

            LastTargetSignatures.Reset();
        }
    };

    FRegistration Registration;
}

void ACubusWorldVegetationActor::
InvalidateDynamicWindBridgeTargets()
{
    CachedUltraDynamicWeatherActor = nullptr;
    CachedGlobalFoliageActor = nullptr;
    CachedDynamicWindCollection = nullptr;

    LastBridgedWindDirection = FVector(
        MAX_flt,
        MAX_flt,
        MAX_flt
    );

    LastBridgedWindIntensity = -MAX_flt;
}
