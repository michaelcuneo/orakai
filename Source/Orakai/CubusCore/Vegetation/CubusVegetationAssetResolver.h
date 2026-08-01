#pragma once

#include "CoreMinimal.h"

class UClass;
class UMaterialInterface;
class UTransformProviderData;

struct FCubusVegetationSpeciesCatalogEntry;

class ORAKAI_API FCubusVegetationAssetResolver
{
public:
    static FName NormalizeSpeciesId(
        FName SpeciesId
    );

    static TSoftObjectPtr<UMaterialInterface>
    ResolveFoliageMaterial(
        FName SpeciesId
    );

    static UTransformProviderData*
    ResolveTransformProvider(
        FName SpeciesId
    );

    static UClass* ResolveHeroPveActorClass(
        const FCubusVegetationSpeciesCatalogEntry& SpeciesEntry
    );
};