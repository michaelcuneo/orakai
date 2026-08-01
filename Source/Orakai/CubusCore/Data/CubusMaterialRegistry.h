#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CubusCore/Data/CubusMaterialDefinition.h"

#include "CubusMaterialRegistry.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Editor-authored collection of every voxel material available to Cubus.
 */
UCLASS(
    BlueprintType,
    meta = (DisplayName = "Cubus Material Registry")
)
class ORAKAI_API UCubusMaterialRegistry : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Materials"
    )
    TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Materials"
    )
    TArray<FCubusMaterialDefinition> Materials;

    UFUNCTION(
        BlueprintPure,
        Category = "Cubus|Materials"
    )
    const FCubusMaterialDefinition& GetMaterialDefinition(
        int32 MaterialId
    ) const;

    const FCubusMaterialDefinition* FindMaterialDefinition(
        int32 MaterialId
    ) const;

    UMaterialInterface* ResolveMaterial(
        int32 MaterialId
    ) const;

    UMaterialInterface* ResolveRuntimeMaterial(
        int32 MaterialId
    ) const;

    bool IsRenderableSolid(
        int32 MaterialId
    ) const;

    bool OccludesBlockFaces(
        int32 MaterialId
    ) const;

    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Materials"
    )
    void ValidateRegistry();

    virtual void PostLoad() override;

    #if WITH_EDITOR
    virtual void PostEditChangeProperty(
        FPropertyChangedEvent& PropertyChangedEvent
    ) override;
    #endif

private:
    void RebuildLookupCache() const;

    mutable TMap<int32, int32> MaterialIndexById;
    mutable TMap<int32, TWeakObjectPtr<UMaterialInstanceDynamic>>
        RuntimeMaterialById;

    mutable bool bLookupCacheDirty = true;

    static const FCubusMaterialDefinition InvalidDefinition;
};
