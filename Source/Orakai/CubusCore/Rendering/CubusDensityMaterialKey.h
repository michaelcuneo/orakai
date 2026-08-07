#pragma once

#include "CoreMinimal.h"

/**
 * Packs the two voxel materials used by a density mesh section into a
 * negative int32 key. Positive values remain ordinary block material IDs.
 *
 * Cubus material IDs are currently small editor-authored values. Density
 * pairing reserves 15 bits for each ID so the sign bit can identify the key.
 */
struct ORAKAI_API FCubusDensityMaterialKey
{
    static constexpr int32 MaximumPackedMaterialId = 0x7fff;

    static int32 Make(const int32 MaterialA, const int32 MaterialB)
    {
        const int32 Primary = FMath::Clamp(
            FMath::Min(MaterialA, MaterialB),
            1,
            MaximumPackedMaterialId
        );

        const int32 Secondary = FMath::Clamp(
            FMath::Max(MaterialA, MaterialB),
            1,
            MaximumPackedMaterialId
        );

        const uint32 Packed =
            0x80000000u |
            (static_cast<uint32>(Primary) << 15u) |
            static_cast<uint32>(Secondary);

        return static_cast<int32>(Packed);
    }

    static bool IsDensityKey(const int32 Key)
    {
        return Key < 0;
    }

    static bool Decode(
        const int32 Key,
        int32& OutPrimaryMaterialId,
        int32& OutSecondaryMaterialId
    )
    {
        if (!IsDensityKey(Key))
        {
            return false;
        }

        const uint32 Packed = static_cast<uint32>(Key);

        OutPrimaryMaterialId =
            static_cast<int32>((Packed >> 15u) & 0x7fffu);

        OutSecondaryMaterialId =
            static_cast<int32>(Packed & 0x7fffu);

        return
            OutPrimaryMaterialId > 0 &&
            OutSecondaryMaterialId > 0;
    }
};
