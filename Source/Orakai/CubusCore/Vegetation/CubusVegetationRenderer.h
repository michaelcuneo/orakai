#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;
class UStaticMesh;
class USkeletalMesh;
class USceneComponent;
class USkeletalMeshComponent;
class UInstancedSkinnedMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

struct FCubusVegetationSpeciesCatalogEntry;

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
        TMap<
            int64,
            TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
        >& StaticBatchComponents,
        TMap<
            int64,
            TObjectPtr<UInstancedSkinnedMeshComponent>
        >& SkeletalBatchComponents
    ) const;

    void ApplyShadowSettings(
      bool bCastShadow,
      const TMap<
          int64,
          TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
      >& StaticBatchComponents,
      const TMap<
          int64,
          TObjectPtr<UInstancedSkinnedMeshComponent>
      >& SkeletalBatchComponents,
      const TArray<TObjectPtr<USkeletalMeshComponent>>& HeroComponents
  ) const;

  void ClearBatches(
      const TMap<
          int64,
          TObjectPtr<UHierarchicalInstancedStaticMeshComponent>
      >& StaticBatchComponents,
      const TMap<
          int64,
          TObjectPtr<UInstancedSkinnedMeshComponent>
      >& SkeletalBatchComponents
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
};