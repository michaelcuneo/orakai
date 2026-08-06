#pragma once

#include "CoreMinimal.h"

class AActor;
class UClass;
class UObject;
class UWorld;
class UStaticMesh;
class USkeletalMesh;
class USceneComponent;
class UMaterialInterface;
class USkeletalMeshComponent;
class UInstancedSkinnedMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class USkinnedMeshComponent;
class UInstancedStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

struct FCubusVegetationSpeciesCatalogEntry;

struct FCubusHeroVegetationRenderSettings
{
    bool bEnabled = false;
    bool bUsePveActors = false;
    bool bUseInstancedSkeletalFallback = false;
    bool bForceFoliageMaterialOverride = false;
    bool bCastShadow = true;
    bool bAppendOnly = false;
};

struct FCubusHeroVegetationRenderResult
{
    int32 ActiveHeroComponentCount = 0;
    int32 ActiveHeroPveActorCount = 0;
    int32 InstancedFallbackCount = 0;
    int32 SkeletalInstanceCount = 0;
};

class ORAKAI_API FCubusVegetationRenderer
{
public:
    static int64 MakePrimaryBatchKey(
        int32 SpeciesIndex,
        int32 GrowthStageIndex
    );

    static int64 MakeStaticFallbackBatchKey(
        int32 SpeciesIndex,
        int32 GrowthStageIndex
    );

    void EnsureBatches(
        AActor* Owner,
        USceneComponent* Root,
        const TArray<FCubusVegetationSpeciesCatalogEntry>& SpeciesCatalog,
        bool bCastShadow,
        int32 StartCullDistance,
        int32 EndCullDistance,
        TMap<int64, TObjectPtr<UInstancedStaticMeshComponent>>& GrassBatchComponents,
        TMap<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& StaticBatchComponents,
        TMap<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& SkeletalBatchComponents
    ) const;

    void ApplyShadowSettings(
        bool bCastShadow,
        const TMap<int64, TObjectPtr<UInstancedStaticMeshComponent>>& GrassBatchComponents,
        const TMap<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& StaticBatchComponents,
        const TMap<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& SkeletalBatchComponents,
        const TArray<TObjectPtr<USkeletalMeshComponent>>& HeroComponents
    ) const;

    void ApplyFoliageMaterialOverride(
        USkinnedMeshComponent* Component,
        UMaterialInterface* OverrideMaterial
    ) const;

    void ClearBatches(
        const TMap<int64, TObjectPtr<UInstancedStaticMeshComponent>>& GrassBatchComponents,
        const TMap<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& StaticBatchComponents,
        const TMap<int64, TObjectPtr<UInstancedSkinnedMeshComponent>>& SkeletalBatchComponents
    ) const;

    UInstancedStaticMeshComponent* CreateGrassBatch(
        AActor* Owner,
        USceneComponent* Root,
        FName ComponentName,
        bool bCastShadow,
        int32 StartCullDistance,
        int32 EndCullDistance
    ) const;

    UHierarchicalInstancedStaticMeshComponent* CreateStaticBatch(
        AActor* Owner,
        USceneComponent* Root,
        FName ComponentName,
        bool bCastShadow,
        int32 StartCullDistance,
        int32 EndCullDistance
    ) const;

    UInstancedSkinnedMeshComponent* CreateSkeletalBatch(
        AActor* Owner,
        USceneComponent* Root,
        FName ComponentName,
        bool bCastShadow,
        int32 StartCullDistance,
        int32 EndCullDistance
    ) const;

    USkeletalMeshComponent* CreateHeroSkeletalComponent(
        AActor* Owner,
        USceneComponent* Root,
        FName ComponentName,
        bool bCastShadow
    ) const;

    FCubusHeroVegetationRenderResult RenderSkeletalBatch(
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
    ) const;
};
