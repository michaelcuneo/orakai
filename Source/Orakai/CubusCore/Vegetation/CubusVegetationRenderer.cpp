#include "CubusCore/Vegetation/CubusVegetationRenderer.h"
#include "CubusCore/Vegetation/CubusVegetationTypes.h"
#include "CubusCore/Vegetation/CubusVegetationWindUtilities.h"

#include "Components/ActorComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "DynamicWindSkeletalData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPtr.h"
#include "Animation/TransformProviderData.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DynamicWindData.h"
#include "GameFramework/Actor.h"

int64 FCubusVegetationRenderer::MakePrimaryBatchKey(
    const int32 SpeciesIndex,
    const int32 GrowthStageIndex
)
{
    const uint64 SpeciesPart =
        static_cast<uint64>(
            static_cast<uint32>(SpeciesIndex)
        ) << 32;

    const uint64 StagePart =
        static_cast<uint32>(GrowthStageIndex);

    return static_cast<int64>(
        SpeciesPart | StagePart
    );
}

int64 FCubusVegetationRenderer::MakeStaticFallbackBatchKey(
    const int32 SpeciesIndex,
    const int32 GrowthStageIndex
)
{
    constexpr uint64 StaticFallbackFlag =
        1ULL << 63;

    const uint64 SpeciesPart =
        static_cast<uint64>(
            static_cast<uint32>(SpeciesIndex)
        ) << 32;

    const uint64 StagePart =
        static_cast<uint32>(GrowthStageIndex);

    return static_cast<int64>(
        StaticFallbackFlag |
        SpeciesPart |
        StagePart
    );
}

UHierarchicalInstancedStaticMeshComponent*
FCubusVegetationRenderer::CreateStaticBatch(
    AActor* Owner,
    USceneComponent* Root,
    const FName ComponentName,
    const bool bCastShadow,
    const int32 StartCullDistance,
    const int32 EndCullDistance
) const
{
    if (!IsValid(Owner) || !IsValid(Root))
    {
        return nullptr;
    }

    UHierarchicalInstancedStaticMeshComponent* Component =
        NewObject<UHierarchicalInstancedStaticMeshComponent>(
            Owner,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(bCastShadow);
    Component->SetMobility(EComponentMobility::Movable);

    Component->SetCullDistances(
        FMath::Max(0, StartCullDistance),
        FMath::Max(StartCullDistance, EndCullDistance)
    );

    Component->RegisterComponent();
    Owner->AddInstanceComponent(Component);

    return Component;
}

UInstancedSkinnedMeshComponent*
FCubusVegetationRenderer::CreateSkeletalBatch(
    AActor* Owner,
    USceneComponent* Root,
    const FName ComponentName,
    const bool bCastShadow,
    const int32 StartCullDistance,
    const int32 EndCullDistance
) const
{
    if (!IsValid(Owner) || !IsValid(Root))
    {
        return nullptr;
    }

    UInstancedSkinnedMeshComponent* Component =
        NewObject<UInstancedSkinnedMeshComponent>(
            Owner,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(bCastShadow);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetVisibility(true, true);
    Component->SetHiddenInGame(false, true);
    Component->SetComponentTickEnabled(true);

    Component->VisibilityBasedAnimTickOption =
        EVisibilityBasedAnimTickOption::
            AlwaysTickPoseAndRefreshBones;

    Component->SetAnimationMinScreenSize(-1.0f);

    UDynamicWindData* DynamicWindProvider =
        NewObject<UDynamicWindData>(
            Component,
            TEXT("DynamicWindProvider"),
            RF_Transient
        );

    Component->SetTransformProvider(
        DynamicWindProvider
    );

    Component->SetCullDistances(
        FMath::Max(0, StartCullDistance),
        FMath::Max(StartCullDistance, EndCullDistance)
    );

    Component->RegisterComponent();
    Owner->AddInstanceComponent(Component);

    return Component;
}

USkeletalMeshComponent*
FCubusVegetationRenderer::CreateHeroSkeletalComponent(
    AActor* Owner,
    USceneComponent* Root,
    const FName ComponentName,
    const bool bCastShadow
) const
{
    if (!IsValid(Owner) || !IsValid(Root))
    {
        return nullptr;
    }

    USkeletalMeshComponent* Component =
        NewObject<USkeletalMeshComponent>(
            Owner,
            ComponentName,
            RF_Transient
        );

    if (!IsValid(Component))
    {
        return nullptr;
    }

    Component->SetupAttachment(Root);
    Component->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(bCastShadow);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetVisibility(false, true);
    Component->SetHiddenInGame(true, true);

    Component->RegisterComponent();
    Owner->AddInstanceComponent(Component);

    return Component;
}

void FCubusVegetationRenderer::ApplyShadowSettings(
    const bool bCastShadow,
    const TMap<
        int64,
        TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
    >& StaticBatchComponents,
    const TMap<
        int64,
        TObjectPtr<UInstancedSkinnedMeshComponent>
    >& SkeletalBatchComponents,
    const TArray<TObjectPtr<USkeletalMeshComponent>>& HeroComponents
) const
{
    for (
        const TPair<
            int64,
            TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
        >& Pair : StaticBatchComponents
    )
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->SetCastShadow(bCastShadow);
        }
    }

    for (
        const TPair<
            int64,
            TObjectPtr<UInstancedSkinnedMeshComponent>
        >& Pair : SkeletalBatchComponents
    )
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->SetCastShadow(bCastShadow);
        }
    }

    for (USkeletalMeshComponent* HeroComponent : HeroComponents)
    {
        if (IsValid(HeroComponent))
        {
            HeroComponent->SetCastShadow(bCastShadow);
        }
    }
}

void FCubusVegetationRenderer::ClearBatches(
    const TMap<
        int64,
        TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
    >& StaticBatchComponents,
    const TMap<
        int64,
        TObjectPtr<UInstancedSkinnedMeshComponent>
    >& SkeletalBatchComponents
) const
{
    for (
        const TPair<
            int64,
            TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
        >& Pair : StaticBatchComponents
    )
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->ClearInstances();
        }
    }

    for (
        const TPair<
            int64,
            TObjectPtr<UInstancedSkinnedMeshComponent>
        >& Pair : SkeletalBatchComponents
    )
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->ClearInstances();
        }
    }
}

void FCubusVegetationRenderer::EnsureBatches(
    AActor* Owner,
    USceneComponent* Root,
    const TArray<
        FCubusVegetationSpeciesCatalogEntry
    >& SpeciesCatalog,
    const bool bCastShadow,
    const int32 StartCullDistance,
    const int32 EndCullDistance,
    TMap<
        int64,
        TObjectPtr<
            UHierarchicalInstancedStaticMeshComponent
        >
    >& StaticBatchComponents,
    TMap<
        int64,
        TObjectPtr<UInstancedSkinnedMeshComponent>
    >& SkeletalBatchComponents
) const
{
    TMap<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> NewStaticBatches;
    TMap<int64, TObjectPtr<UInstancedSkinnedMeshComponent>> NewSkeletalBatches;

    for (int32 SpeciesIndex = 0; SpeciesIndex < SpeciesCatalog.Num(); ++SpeciesIndex)
    {
        const FCubusVegetationSpeciesCatalogEntry& Entry = SpeciesCatalog[SpeciesIndex];

        if (Entry.TypeId <= 0)
        {
            continue;
        }

        for (int32 StageIndex = 0; StageIndex < Entry.GrowthStageMeshes.Num(); ++StageIndex)
        {
            const TSoftObjectPtr<UObject>& MeshReference = Entry.GrowthStageMeshes[StageIndex];

            if (MeshReference.IsNull())
            {
                continue;
            }

            UObject* MeshAsset = MeshReference.LoadSynchronous();

            const int64 BatchKey =
                MakePrimaryBatchKey(
                    SpeciesIndex,
                    StageIndex
                );

            if (!IsValid(MeshAsset))
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("Cubus vegetation could not load mesh asset %s"),
                    *MeshReference.ToSoftObjectPath().ToString()
                );
                continue;
            }

            const FString SpeciesToken = Entry.SpeciesId.IsNone()
                ? FString::Printf(TEXT("Species%d"), SpeciesIndex)
                : Entry.SpeciesId.ToString();

            if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(MeshAsset))
            {
                if (UInstancedSkinnedMeshComponent* OldSkeletal =
                        SkeletalBatchComponents.FindRef(BatchKey))
                {
                    OldSkeletal->ClearInstances();
                    OldSkeletal->DestroyComponent();
                    SkeletalBatchComponents.Remove(BatchKey);
                }

                UHierarchicalInstancedStaticMeshComponent* Component =
                    StaticBatchComponents.FindRef(BatchKey);

                if (!IsValid(Component))
                {
                    const FName ComponentName(
                        *FString::Printf(
                            TEXT("CubusWorldCatalogStatic_%s_%d"),
                            *SpeciesToken,
                            StageIndex
                        )
                    );

                    Component = CreateStaticBatch(Owner, Root, ComponentName, bCastShadow, StartCullDistance, EndCullDistance);
                }

                if (!IsValid(Component))
                {
                    continue;
                }

                Component->SetStaticMesh(StaticMesh);
                Component->SetCastShadow(bCastShadow);
                Component->SetCullDistances(
                    FMath::Max(0, StartCullDistance),
                    FMath::Max(StartCullDistance, EndCullDistance)
                );

                NewStaticBatches.Add(BatchKey, Component);
                continue;
            }

            if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(MeshAsset))
            {
                if (UHierarchicalInstancedStaticMeshComponent* OldStatic =
                        StaticBatchComponents.FindRef(BatchKey))
                {
                    OldStatic->ClearInstances();
                    OldStatic->DestroyComponent();
                    StaticBatchComponents.Remove(BatchKey);
                }

                UInstancedSkinnedMeshComponent* Component =
                    SkeletalBatchComponents.FindRef(BatchKey);

                if (!IsValid(Component))
                {
                    const FName ComponentName(
                        *FString::Printf(
                            TEXT("CubusWorldCatalogSkeletal_%s_%d"),
                            *SpeciesToken,
                            StageIndex
                        )
                    );

                    Component = CreateSkeletalBatch(
                        Owner,
                        Root,
                        ComponentName,
                        bCastShadow,
                        StartCullDistance,
                        EndCullDistance
                    );
                }

                if (!IsValid(Component))
                {
                    continue;
                }

                Component->SetSkinnedAssetAndUpdate(SkeletalMesh);
                Component->VisibilityBasedAnimTickOption =
                    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
                Component->SetAnimationMinScreenSize(-1.0f);

                static TSet<FName> LoggedDynamicWindMeshes;
                if (!LoggedDynamicWindMeshes.Contains(SkeletalMesh->GetFName()))
                {
                    const UDynamicWindSkeletalData* DynamicWindSkeletalData =
                        SkeletalMesh->GetAssetUserData<UDynamicWindSkeletalData>();

                    const bool bDynamicWindReady =
                        IsValid(DynamicWindSkeletalData) &&
                        DynamicWindSkeletalData->bIsEnabled;

                    UE_LOG(
                        LogTemp,
                        Display,
                        TEXT("Cubus DynamicWind mesh: mesh=%s data=%s enabled=%s groups=%d provider=%s"),
                        *SkeletalMesh->GetName(),
                        IsValid(DynamicWindSkeletalData) ? TEXT("present") : TEXT("missing"),
                        bDynamicWindReady ? TEXT("yes") : TEXT("no"),
                        IsValid(DynamicWindSkeletalData)
                            ? DynamicWindSkeletalData->SimulationGroups.Num()
                            : 0,
                        IsValid(Component->GetTransformProvider())
                            ? *Component->GetTransformProvider()->GetClass()->GetName()
                            : TEXT("None")
                    );

                    LoggedDynamicWindMeshes.Add(SkeletalMesh->GetFName());
                }

                Component->SetCastShadow(bCastShadow);
                Component->SetCullDistances(
                    FMath::Max(0, StartCullDistance),
                    FMath::Max(StartCullDistance, EndCullDistance)
                );

                NewSkeletalBatches.Add(BatchKey, Component);
                continue;
            }

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Cubus vegetation stage asset is unsupported class %s for %s"),
                *MeshAsset->GetClass()->GetName(),
                *MeshReference.ToSoftObjectPath().ToString()
            );
        }
    }

    /*
    * Build authored static fallback batches into the new batch map so they
    * participate in the same reuse and cleanup lifecycle as primary batches.
    */
    for (
        int32 SpeciesIndex = 0;
        SpeciesIndex < SpeciesCatalog.Num();
        ++SpeciesIndex
    )
    {
        const FCubusVegetationSpeciesCatalogEntry& Entry =
            SpeciesCatalog[SpeciesIndex];

        const FString SpeciesToken = Entry.SpeciesId.IsNone()
            ? FString::Printf(
                TEXT("Species%d"),
                SpeciesIndex
            )
            : Entry.SpeciesId.ToString();

        for (
            int32 GrowthStageIndex = 0;
            GrowthStageIndex < Entry.StaticGrowthStageMeshes.Num();
            ++GrowthStageIndex
        )
        {
            const TSoftObjectPtr<UStaticMesh>& StaticMeshReference =
                Entry.StaticGrowthStageMeshes[GrowthStageIndex];

            if (StaticMeshReference.IsNull())
            {
                continue;
            }

            UStaticMesh* StaticFallbackMesh =
                StaticMeshReference.LoadSynchronous();

            if (!IsValid(StaticFallbackMesh))
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT(
                        "Cubus vegetation failed to load static fallback "
                        "for species '%s', growth stage %d."
                    ),
                    *Entry.SpeciesId.ToString(),
                    GrowthStageIndex
                );

                continue;
            }

            const int64 StaticFallbackBatchKey =
                MakeStaticFallbackBatchKey(
                    SpeciesIndex,
                    GrowthStageIndex
                );

            UHierarchicalInstancedStaticMeshComponent* Component =
                StaticBatchComponents.FindRef(
                    StaticFallbackBatchKey
                );

            if (!IsValid(Component))
            {
                const FName ComponentName(
                    *FString::Printf(
                        TEXT("CubusWorldCatalogFallback_%s_%d"),
                        *SpeciesToken,
                        GrowthStageIndex
                    )
                );

                Component = CreateStaticBatch(
                    Owner,
                    Root,
                    ComponentName,
                    bCastShadow,
                    StartCullDistance,
                    EndCullDistance
                );
            }

            if (!IsValid(Component))
            {
                continue;
            }

            Component->SetStaticMesh(
                StaticFallbackMesh
            );

            Component->SetCastShadow(
                bCastShadow
            );

            Component->SetCullDistances(
                FMath::Max(0, StartCullDistance),
                FMath::Max(
                    StartCullDistance,
                    EndCullDistance
                )
            );

            NewStaticBatches.Add(
                StaticFallbackBatchKey,
                Component
            );
        }
    }

    for (const TPair<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair
         : StaticBatchComponents)
    {
        if (NewStaticBatches.Contains(Pair.Key) || !IsValid(Pair.Value))
        {
            continue;
        }

        Pair.Value->ClearInstances();
        Pair.Value->DestroyComponent();
    }

    for (const TPair<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& Pair
         : SkeletalBatchComponents)
    {
        if (NewSkeletalBatches.Contains(Pair.Key) || !IsValid(Pair.Value))
        {
            continue;
        }

        Pair.Value->ClearInstances();
        Pair.Value->DestroyComponent();
    }

    StaticBatchComponents = MoveTemp(NewStaticBatches);
    SkeletalBatchComponents = MoveTemp(NewSkeletalBatches);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus vegetation catalog batches: static=%d skeletal=%d"
        ),
        StaticBatchComponents.Num(),
        SkeletalBatchComponents.Num()
    );
}

void FCubusVegetationRenderer::ApplyFoliageMaterialOverride(
    USkinnedMeshComponent* Component,
    UMaterialInterface* OverrideMaterial
) const
{
    if (
        !IsValid(Component) ||
        !IsValid(OverrideMaterial)
    )
    {
        return;
    }

    const int32 MaterialCount =
        Component->GetNumMaterials();

    if (MaterialCount <= 0)
    {
        return;
    }

    bool bOverrodeAnySlot = false;

    for (
        int32 MaterialIndex = 0;
        MaterialIndex < MaterialCount;
        ++MaterialIndex
    )
    {
        UMaterialInterface* ExistingMaterial =
            Component->GetMaterial(MaterialIndex);

        const FString ExistingName =
            IsValid(ExistingMaterial)
                ? ExistingMaterial->GetName()
                : FString();

        const bool bLooksLikeFoliageSlot =
            ExistingName.IsEmpty() ||
            ExistingName.Contains(
                TEXT("Foliage"),
                ESearchCase::IgnoreCase
            ) ||
            ExistingName.Contains(
                TEXT("Leaf"),
                ESearchCase::IgnoreCase
            ) ||
            ExistingName.Contains(
                TEXT("Needle"),
                ESearchCase::IgnoreCase
            ) ||
            ExistingName.Contains(
                TEXT("Twig"),
                ESearchCase::IgnoreCase
            );

        if (!bLooksLikeFoliageSlot)
        {
            continue;
        }

        Component->SetMaterial(
            MaterialIndex,
            OverrideMaterial
        );

        bOverrodeAnySlot = true;
    }

    if (
        !bOverrodeAnySlot &&
        MaterialCount == 1
    )
    {
        Component->SetMaterial(
            0,
            OverrideMaterial
        );
    }
}

FCubusHeroVegetationRenderResult
FCubusVegetationRenderer::RenderSkeletalBatch(
    AActor* Owner,
    UWorld* World,
    USceneComponent* Root,
    UInstancedSkinnedMeshComponent* BatchComponent,
    const TArray<FTransform>& Transforms,
    UClass* HeroPveActorClass,
    UMaterialInterface* FoliageOverrideMaterial,
    AActor* WindProviderActor,
    const FCubusHeroVegetationRenderSettings& Settings,
    int32& InOutActiveHeroComponentCount,
    int32& InOutActiveHeroPveActorCount,
    int32& InOutRemainingFallbackBudget,
    TArray<TObjectPtr<USkeletalMeshComponent>>& HeroComponents,
    TArray<TObjectPtr<AActor>>& HeroPveActors
) const
{
    FCubusHeroVegetationRenderResult Result;

    if (
        !IsValid(Owner) ||
        !IsValid(World) ||
        !IsValid(Root) ||
        !IsValid(BatchComponent)
    )
    {
        return Result;
    }

    if (!Settings.bEnabled)
    {
        TArray<int32> AnimationIndices;
        AnimationIndices.Init(0, Transforms.Num());

        BatchComponent->AddInstances(
            Transforms,
            AnimationIndices,
            false,
            false
        );

        if (!Settings.bAppendOnly)
        {
            BatchComponent->OptimizeInstanceData(false);
        }

        Result.SkeletalInstanceCount =
            Transforms.Num();

        return Result;
    }

    const USkeletalMesh* SkeletalMesh =
        Cast<USkeletalMesh>(
            BatchComponent->GetSkinnedAsset()
        );

    if (!IsValid(SkeletalMesh))
    {
        return Result;
    }

    TArray<FTransform> FallbackTransforms;

    for (const FTransform& LocalTransform : Transforms)
    {
        const bool bUseHero = true;

        if (bUseHero)
        {
            const int32 HeroIndex =
                InOutActiveHeroComponentCount;

            if (
                Settings.bUsePveActors &&
                HeroPveActorClass != nullptr
            )
            {
                AActor* HeroActor = nullptr;

                if (HeroPveActors.IsValidIndex(HeroIndex))
                {
                    HeroActor =
                        HeroPveActors[HeroIndex];

                    if (
                        IsValid(HeroActor) &&
                        HeroActor->GetClass() !=
                            HeroPveActorClass
                    )
                    {
                        HeroActor->Destroy();
                        HeroActor = nullptr;
                    }
                }

                if (!IsValid(HeroActor))
                {
                    const FName ActorName(
                        *FString::Printf(
                            TEXT(
                                "CubusWorldHeroPveWind_%d"
                            ),
                            HeroIndex
                        )
                    );

                    FActorSpawnParameters Params;
                    Params.Owner = Owner;
                    Params.Name = ActorName;
                    Params.SpawnCollisionHandlingOverride =
                        ESpawnActorCollisionHandlingMethod::
                            AlwaysSpawn;

                    HeroActor =
                        World->SpawnActor<AActor>(
                            HeroPveActorClass,
                            FTransform::Identity,
                            Params
                        );

                    if (IsValid(HeroActor))
                    {
                        HeroActor->AttachToComponent(
                            Root,
                            FAttachmentTransformRules::
                                KeepRelativeTransform
                        );

                        if (
                            !HeroPveActors.IsValidIndex(
                                HeroIndex
                            )
                        )
                        {
                            HeroPveActors.SetNum(
                                HeroIndex + 1
                            );
                        }

                        HeroPveActors[HeroIndex] =
                            HeroActor;
                    }
                }

                if (IsValid(HeroActor))
                {
                    HeroActor->SetActorRelativeTransform(
                        LocalTransform,
                        false,
                        nullptr,
                        ETeleportType::None
                    );

                    HeroActor->SetActorHiddenInGame(
                        false
                    );

                    FCubusVegetationWindUtilities::
                        AssignLikelyWindProviderActor(
                            HeroActor,
                            WindProviderActor
                        );

                    TInlineComponentArray<
                        UActorComponent*
                    > Components(HeroActor);

                    for (
                        UActorComponent* Component :
                        Components
                    )
                    {
                        FCubusVegetationWindUtilities::
                            AssignLikelyWindProviderActor(
                                Component,
                                WindProviderActor
                            );
                    }

                    ++InOutActiveHeroComponentCount;
                    ++InOutActiveHeroPveActorCount;

                    Result.ActiveHeroComponentCount++;
                    Result.ActiveHeroPveActorCount++;
                    continue;
                }
            }

            USkeletalMeshComponent* HeroComponent =
                nullptr;

            if (HeroComponents.IsValidIndex(HeroIndex))
            {
                HeroComponent =
                    HeroComponents[HeroIndex];
            }

            if (!IsValid(HeroComponent))
            {
                const FName ComponentName(
                    *FString::Printf(
                        TEXT(
                            "CubusWorldHeroSkeletalWind_%d"
                        ),
                        HeroIndex
                    )
                );

                HeroComponent =
                    CreateHeroSkeletalComponent(
                        Owner,
                        Root,
                        ComponentName,
                        Settings.bCastShadow
                    );

                if (
                    !HeroComponents.IsValidIndex(
                        HeroIndex
                    )
                )
                {
                    HeroComponents.SetNum(
                        HeroIndex + 1
                    );
                }

                HeroComponents[HeroIndex] =
                    HeroComponent;
            }

            if (IsValid(HeroComponent))
            {
                if (
                    HeroComponent
                        ->GetSkeletalMeshAsset() !=
                    SkeletalMesh
                )
                {
                    HeroComponent->SetSkeletalMesh(
                        const_cast<USkeletalMesh*>(
                            SkeletalMesh
                        )
                    );
                }

                if (
                    !Settings
                        .bForceFoliageMaterialOverride
                )
                {
                    HeroComponent
                        ->EmptyOverrideMaterials();
                }
                else
                {
                    ApplyFoliageMaterialOverride(
                        HeroComponent,
                        FoliageOverrideMaterial
                    );
                }
                
                HeroComponent->SetRelativeTransform(
                    LocalTransform
                );

                HeroComponent->SetVisibility(
                    true,
                    true
                );

                HeroComponent->SetHiddenInGame(
                    false,
                    true
                );

                ++InOutActiveHeroComponentCount;
                ++Result.ActiveHeroComponentCount;
                continue;
            }
        }

        if (
            Settings.bUseInstancedSkeletalFallback &&
            InOutRemainingFallbackBudget > 0
        )
        {
            FallbackTransforms.Add(
                LocalTransform
            );

            --InOutRemainingFallbackBudget;
        }
    }

    if (!FallbackTransforms.IsEmpty())
    {
        TArray<int32> AnimationIndices;
        AnimationIndices.Init(
            0,
            FallbackTransforms.Num()
        );

        BatchComponent->AddInstances(
            FallbackTransforms,
            AnimationIndices,
            false,
            false
        );

        if (!Settings.bAppendOnly)
        {
            BatchComponent->OptimizeInstanceData(false);
        }

        Result.InstancedFallbackCount =
            FallbackTransforms.Num();

        Result.SkeletalInstanceCount =
            FallbackTransforms.Num();
    }

    return Result;
}