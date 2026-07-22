#pragma once

#include "CoreMinimal.h"
#include "CubusVoxel.generated.h"

/**
 * Compact data stored for every voxel.
 *
 * MaterialId 0 is always reserved for empty air.
 * All detailed properties come from UCubusMaterialRegistry.
 */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusVoxel
{
    GENERATED_BODY()

public:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Voxel",
        meta = (
            ClampMin = "0",
            ClampMax = "65535"
        )
    )
    int32 MaterialId = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Voxel",
        AdvancedDisplay
    )
    uint8 Flags = 0;

    FORCEINLINE bool IsEmpty() const
    {
        return MaterialId == 0;
    }

    FORCEINLINE bool HasMaterial() const
    {
        return MaterialId != 0;
    }
};