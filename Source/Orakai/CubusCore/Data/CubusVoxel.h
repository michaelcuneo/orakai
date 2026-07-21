#pragma once
#pragma once

#include "CoreMinimal.h"
#include "CubusVoxel.generated.h"

/**
 * The basic representation used by the block-voxel system.
 *
 * Density voxels will use a separate data structure because they represent
 * scalar samples rather than discrete occupied cubes.
 */
UENUM(BlueprintType)
enum class ECubusVoxelType : uint8
{
    Air UMETA(DisplayName = "Air"),
    Solid UMETA(DisplayName = "Solid")
};

/**
 * Data belonging to one discrete block voxel.
 */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusVoxel
{
    GENERATED_BODY()

public:
    /**
     * Determines whether the voxel occupies space.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Voxel"
    )
    ECubusVoxelType Type = ECubusVoxelType::Solid;

    /**
     * Identifier used later by chunk material palettes.
     *
     * This is deliberately an ID rather than a direct material pointer.
     * Thousands of voxels should not each carry a UObject reference.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Voxel",
        meta = (ClampMin = "0")
    )
    int32 MaterialId = 0;

    /**
     * Optional flags reserved for voxel behaviour.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Voxel",
        AdvancedDisplay
    )
    uint8 Flags = 0;

    FORCEINLINE bool IsSolid() const
    {
        return Type != ECubusVoxelType::Air;
    }

    FORCEINLINE bool IsAir() const
    {
        return Type == ECubusVoxelType::Air;
    }
};