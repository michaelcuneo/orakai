#pragma once

#include "CoreMinimal.h"

#include "CubusBlockVoxel.generated.h"

enum class ECubusBlockVoxelFlags : uint8
{
    None  = 0,
    Water = 1 << 0
};

ENUM_CLASS_FLAGS(ECubusBlockVoxelFlags);

USTRUCT(BlueprintType)
struct ORAKAI_API FCubusBlockVoxel
{
    GENERATED_BODY()

public:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Block Voxel",
        meta = (
            ClampMin = "0",
            ClampMax = "65535"
        )
    )
    int32 MaterialId = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Block Voxel",
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

    FORCEINLINE bool operator==(
        const FCubusBlockVoxel& Other
    ) const
    {
        return
            MaterialId == Other.MaterialId &&
            Flags == Other.Flags;
    }

    FORCEINLINE bool operator!=(
        const FCubusBlockVoxel& Other
    ) const
    {
        return !(*this == Other);
    }

    bool IsWater() const
    {
        return
            !IsEmpty() &&
            (
                Flags &
                static_cast<uint8>(
                    ECubusBlockVoxelFlags::Water
                )
            ) != 0;
    }

    bool IsSolid() const
    {
        return
            !IsEmpty() &&
            !IsWater();
    }

    void SetWater(
        const bool bWater
    )
    {
        const uint8 WaterFlag =
            static_cast<uint8>(
                ECubusBlockVoxelFlags::Water
            );

        if (bWater)
        {
            Flags |= WaterFlag;
        }
        else
        {
            Flags &= ~WaterFlag;
        }
    }
};