#include "CubusCore/Data/CubusTerrainSurfaceLayers.h"

namespace CubusTerrainSurfaceLayers
{
    float Hash01(const FIntVector& Cell, const int32 Salt)
    {
        uint32 Hash = GetTypeHash(Cell);
        Hash = HashCombineFast(Hash, GetTypeHash(Salt));
        Hash ^= Hash >> 16;
        Hash *= 0x7feb352dU;
        Hash ^= Hash >> 15;
        Hash *= 0x846ca68bU;
        Hash ^= Hash >> 16;
        return static_cast<float>(Hash & 0x00ffffffU) /
            static_cast<float>(0x01000000U);
    }

    float DensityMask(
        const FVector& WorldPosition,
        const float MinimumSpacing,
        const float Density,
        const int32 Salt
    )
    {
        if (Density <= 0.0f)
        {
            return 0.0f;
        }

        const float Spacing = FMath::Max(1.0f, MinimumSpacing);
        const FIntVector Cell(
            FMath::FloorToInt(WorldPosition.X / Spacing),
            FMath::FloorToInt(WorldPosition.Y / Spacing),
            FMath::FloorToInt(WorldPosition.Z / Spacing)
        );

        const float Roll = Hash01(Cell, Salt);
        return Roll <= FMath::Clamp(Density, 0.0f, 1.0f) ? 1.0f : 0.0f;
    }
}

FCubusTerrainSurfaceLayerMasks EvaluateCubusTerrainSurfaceLayers(
    const FCubusTerrainSurfaceLayerSettings& Settings,
    const FVector& WorldPosition,
    const FVector& WorldNormal,
    const int32 WorldSeed
)
{
    FCubusTerrainSurfaceLayerMasks Result;

    const float NormalZ = FMath::Clamp(
        static_cast<float>(WorldNormal.GetSafeNormal().Z),
        -1.0f,
        1.0f
    );
    const float WorldZ = static_cast<float>(WorldPosition.Z);
    const float SlopeWidth = FMath::Max(0.001f, Settings.SlopeBlendWidth);

    Result.Flat = FMath::SmoothStep<float>(
        Settings.GrassMinimumNormalZ - SlopeWidth,
        Settings.GrassMinimumNormalZ + SlopeWidth,
        NormalZ
    );

    Result.Steep = 1.0f - FMath::SmoothStep<float>(
        Settings.RockMaximumNormalZ - SlopeWidth,
        Settings.RockMaximumNormalZ + SlopeWidth,
        NormalZ
    );

    const float HeightWidth = FMath::Max(1.0f, Settings.HeightBlendWidth);

    Result.Sand = 1.0f - FMath::SmoothStep<float>(
        Settings.SandMaximumWorldHeight - HeightWidth,
        Settings.SandMaximumWorldHeight + HeightWidth,
        WorldZ
    );

    Result.Snow = FMath::SmoothStep<float>(
        Settings.SnowMinimumWorldHeight - HeightWidth,
        Settings.SnowMinimumWorldHeight + HeightWidth,
        WorldZ
    );

    const bool bClutterSlopeAllowed =
        NormalZ >= Settings.ClutterMaximumSlopeNormalZ;

    if (bClutterSlopeAllowed)
    {
        Result.GrassClutter = Result.Flat *
            CubusTerrainSurfaceLayers::DensityMask(
                WorldPosition,
                Settings.ClutterMinimumSpacing,
                Settings.GrassClutterDensity,
                WorldSeed ^ 0x1241
            );

        Result.OrganicClutter = Result.Flat *
            CubusTerrainSurfaceLayers::DensityMask(
                WorldPosition,
                Settings.ClutterMinimumSpacing,
                Settings.OrganicClutterDensity,
                WorldSeed ^ 0x8d31
            );
    }

    Result.StoneClutter = FMath::Max(Result.Flat * 0.35f, Result.Steep) *
        CubusTerrainSurfaceLayers::DensityMask(
            WorldPosition,
            Settings.ClutterMinimumSpacing,
            Settings.StoneClutterDensity,
            WorldSeed ^ 0x32b7
        );

    Result.Boulder = FMath::Max(Result.Steep, 0.25f) *
        CubusTerrainSurfaceLayers::DensityMask(
            WorldPosition,
            Settings.FeatureMinimumSpacing,
            Settings.BoulderDensity,
            WorldSeed ^ 0x673d
        );

    Result.Outcrop = FMath::Clamp(
        (Settings.CliffMaximumNormalZ - NormalZ) /
            FMath::Max(Settings.CliffMaximumNormalZ, 0.001f),
        0.0f,
        1.0f
    ) * CubusTerrainSurfaceLayers::DensityMask(
        WorldPosition,
        Settings.FeatureMinimumSpacing,
        Settings.OutcropDensity,
        WorldSeed ^ 0x9bc5
    );

    Result.FallenLog = Result.Flat *
        CubusTerrainSurfaceLayers::DensityMask(
            WorldPosition,
            Settings.FeatureMinimumSpacing,
            Settings.FallenLogDensity,
            WorldSeed ^ 0xf119
        );

    return Result;
}
