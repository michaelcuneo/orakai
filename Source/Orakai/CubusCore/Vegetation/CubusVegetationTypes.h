#pragma once

#include "CoreMinimal.h"
#include "CubusVegetationTypes.generated.h"

UENUM(BlueprintType, meta = (Bitflags))
enum class ECubusVegetationBiome : uint8
{
    None = 0 UMETA(Hidden),
    Plains = 1 << 0,
    Forest = 1 << 1,
    Rocky = 1 << 2,
    Wetland = 1 << 3
};

USTRUCT(BlueprintType)
struct FCubusVegetationSpeciesCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    FName SpeciesId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    int32 TypeId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (ClampMin = "0.001"))
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (Bitmask, BitmaskEnum = "/Script/Orakai.ECubusVegetationBiome"))
    int32 BiomeMask = static_cast<int32>(ECubusVegetationBiome::Forest);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
    TArray<TSoftObjectPtr<UObject>> GrowthStageMeshes;

    /**
     * Static representations used when a skeletal tree is outside the
     * interactive hero radius.
     *
     * Entries correspond to GrowthStageMeshes by growth-stage index.
     * A missing entry means no static fallback exists for that stage.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Catalog"
    )
    TArray<TSoftObjectPtr<UStaticMesh>> StaticGrowthStageMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (AllowedClasses = "/Script/Engine.Actor"))
    TSoftClassPtr<AActor> HeroPveActorClassOverride;

    // Accepts data assets/blueprints that indirectly reference the runtime actor class.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    TSoftObjectPtr<UObject> HeroPveActorAssetOverride;
};
