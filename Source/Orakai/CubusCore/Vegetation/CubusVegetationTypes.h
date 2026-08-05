#pragma once

#include "CoreMinimal.h"
#include "CubusVegetationTypes.generated.h"

/** Stable generated-instance type IDs shared by generation and rendering. */
namespace CubusVegetationType
{
    constexpr int32 Grass = 1;
    constexpr int32 Shrub = 2;
    constexpr int32 BroadleafTree = 3;
    constexpr int32 Reeds = 4;
    constexpr int32 Alpine = 5;
    constexpr int32 ConiferTree = 6;
    constexpr int32 StoneClutter = 7;
    constexpr int32 OrganicClutter = 8;
    constexpr int32 Count = 9;
}

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

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Catalog",
        meta = (ToolTip = "1 Grass, 2 Shrub, 3 Broadleaf, 4 Reeds, 5 Alpine, 6 Conifer, 7 Stone clutter, 8 Organic clutter")
    )
    int32 TypeId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (ClampMin = "0.001"))
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (Bitmask, BitmaskEnum = "/Script/Orakai.ECubusVegetationBiome"))
    int32 BiomeMask = static_cast<int32>(ECubusVegetationBiome::Forest);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
    TArray<TSoftObjectPtr<UObject>> GrowthStageMeshes;

    /** Static representations used outside the interactive hero radius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    TArray<TSoftObjectPtr<UStaticMesh>> StaticGrowthStageMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (AllowedClasses = "/Script/Engine.Actor"))
    TSoftClassPtr<AActor> HeroPveActorClassOverride;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    TSoftObjectPtr<UObject> HeroPveActorAssetOverride;
};
