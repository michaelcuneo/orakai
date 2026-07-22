#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CubusCore/Data/CubusMaterialDefinition.h"

#include "CubusMaterialRegistry.generated.h"

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

    bool IsRenderableSolid(
        int32 MaterialId
    ) const;

    bool OccludesBlockFaces(
        int32 MaterialId
    ) const;

    /**
     * Checks for duplicate IDs and verifies that ID 0 is empty.
     */
    UFUNCTION(
        BlueprintCallable,
        CallInEditor,
        Category = "Cubus|Materials"
    )
    void ValidateRegistry();

private:
    static const FCubusMaterialDefinition InvalidDefinition;
};