#include "CubusCore/Generation/CubusTerrainMorphology.h"

namespace CubusTerrainMorphology
{
    uint32 Hash2D(const int32 X, const int32 Y, const uint32 Seed)
    {
        uint32 H = static_cast<uint32>(X) * 0x9E3779B9u;
        H ^= static_cast<uint32>(Y) * 0x85EBCA6Bu;
        H ^= Seed * 0xC2B2AE35u;
        H ^= H >> 16;
        H *= 0x7FEB352Du;
        H ^= H >> 15;
        H *= 0x846CA68Bu;
        H ^= H >> 16;
        return H;
    }

    float HashSigned(const int32 X, const int32 Y, const uint32 Seed)
    {
        return static_cast<float>(Hash2D(X, Y, Seed) & 0x00ffffffu) /
            static_cast<float>(0x00ffffffu) * 2.0f - 1.0f;
    }

    float Smooth01(const float T)
    {
        const float X = FMath::Clamp(T, 0.0f, 1.0f);
        return X * X * (3.0f - 2.0f * X);
    }

    float ValueNoise(const double X, const double Y, const uint32 Seed)
    {
        const int32 X0 = FMath::FloorToInt(X);
        const int32 Y0 = FMath::FloorToInt(Y);
        const int32 X1 = X0 + 1;
        const int32 Y1 = Y0 + 1;
        const float Tx = Smooth01(static_cast<float>(X - static_cast<double>(X0)));
        const float Ty = Smooth01(static_cast<float>(Y - static_cast<double>(Y0)));
        const float A = FMath::Lerp(HashSigned(X0, Y0, Seed), HashSigned(X1, Y0, Seed), Tx);
        const float B = FMath::Lerp(HashSigned(X0, Y1, Seed), HashSigned(X1, Y1, Seed), Tx);
        return FMath::Lerp(A, B, Ty);
    }

    float Fbm(double X, double Y, const uint32 Seed, const int32 Octaves)
    {
        float Sum = 0.0f;
        float Weight = 0.0f;
        float Amplitude = 1.0f;
        for (int32 Octave = 0; Octave < Octaves; ++Octave)
        {
            Sum += ValueNoise(X, Y, Seed + static_cast<uint32>(Octave) * 1013u) * Amplitude;
            Weight += Amplitude;
            X = X * 2.031 + 13.7;
            Y = Y * 2.017 - 19.2;
            Amplitude *= 0.5f;
        }
        return Weight > 0.0f ? Sum / Weight : 0.0f;
    }

    float Ridged(double X, double Y, const uint32 Seed, const int32 Octaves)
    {
        float Sum = 0.0f;
        float Weight = 0.0f;
        float Amplitude = 1.0f;
        float Previous = 1.0f;
        for (int32 Octave = 0; Octave < Octaves; ++Octave)
        {
            float Ridge = 1.0f - FMath::Abs(ValueNoise(X, Y, Seed + static_cast<uint32>(Octave) * 1619u));
            Ridge *= Ridge;
            Ridge *= FMath::Lerp(0.45f, 1.0f, Previous);
            Sum += Ridge * Amplitude;
            Weight += Amplitude;
            Previous = Ridge;
            X = X * 2.07 + 7.4;
            Y = Y * 2.03 - 5.8;
            Amplitude *= 0.52f;
        }
        return Weight > 0.0f ? Sum / Weight : 0.0f;
    }
}

FCubusTerrainMorphologySample FCubusTerrainMorphology::Sample(
    const double WorldXmeters,
    const double WorldYmeters,
    const FCubusTerrainStructureSample& Structure,
    const FCubusTerrainStructureSettings& InSettings
)
{
    using namespace CubusTerrainMorphology;

    FCubusTerrainStructureSettings Settings = InSettings;
    Settings.HillScaleKm = FMath::Max(0.6f, Settings.HillScaleKm);
    Settings.SpurScaleKm = FMath::Max(0.20f, Settings.SpurScaleKm);
    Settings.SwaleScaleMeters = FMath::Max(80.0f, Settings.SwaleScaleMeters);
    Settings.FineScaleMeters = FMath::Max(24.0f, Settings.FineScaleMeters);

    const uint32 Seed = static_cast<uint32>(Settings.Seed);

    // Warp the local coordinates with a long wavelength field. This prevents the
    // subordinate ridge families from becoming visibly parallel/repetitive.
    const double WarpScale = FMath::Max(1600.0, static_cast<double>(Settings.HillScaleKm) * 1800.0);
    const float WarpX = Fbm(WorldXmeters / WarpScale, WorldYmeters / WarpScale, Seed ^ 0xA24BAED5u, 3);
    const float WarpY = Fbm(WorldXmeters / WarpScale, WorldYmeters / WarpScale, Seed ^ 0x9FB21C65u, 3);
    const double X = WorldXmeters + static_cast<double>(WarpX) * Settings.MorphologyWarpMeters;
    const double Y = WorldYmeters + static_cast<double>(WarpY) * Settings.MorphologyWarpMeters;

    FCubusTerrainMorphologySample Result;

    // Local terrain strength follows the macro skeleton. Mountain shoulders get
    // stronger ridge relief; lowlands retain rolling relief but remain gentler.
    const float MountainStrength = FMath::Clamp(
        0.20f + Structure.RangeWeight * 0.65f + Structure.MassifWeight * 0.35f,
        0.18f,
        1.0f
    );
    const float LowlandStrength = FMath::Clamp(1.0f - Structure.BasinWeight * 0.55f, 0.35f, 1.0f);

    const double HillScale = static_cast<double>(Settings.HillScaleKm) * 1000.0;
    const float HillBase = Fbm(X / HillScale, Y / HillScale, Seed ^ 0xC13FA9A9u, 5);
    const float HillRidges = Ridged(X / (HillScale * 0.72), Y / (HillScale * 0.72), Seed ^ 0x91E10DA5u, 4);
    Result.HillMeters = (
        HillBase * 0.58f +
        (HillRidges - 0.47f) * 1.05f
    ) * Settings.HillReliefMeters * FMath::Lerp(0.55f, 1.0f, MountainStrength) * LowlandStrength;

    // Two warped ridge families create a branching spur network rather than one
    // isotropic bumpy surface. Their product suppresses broad blobs and favours
    // elongated crests with subordinate branching.
    const double SpurScale = static_cast<double>(Settings.SpurScaleKm) * 1000.0;
    const double SpurAngle = static_cast<double>(ValueNoise(X / 9000.0, Y / 9000.0, Seed ^ 0xD1B54A35u)) * PI;
    const double C = FMath::Cos(SpurAngle);
    const double S = FMath::Sin(SpurAngle);
    const double RX = X * C - Y * S;
    const double RY = X * S + Y * C;
    const float RidgeA = Ridged(RX / SpurScale, RY / (SpurScale * 0.54), Seed ^ 0xABC98388u, 5);
    const float RidgeB = Ridged((RX + RY * 0.37) / (SpurScale * 0.72), (RY - RX * 0.16) / SpurScale, Seed ^ 0x8CB92BA7u, 4);
    const float SpurShape = FMath::Clamp((RidgeA * 0.72f + RidgeB * 0.48f) - 0.47f, -0.35f, 0.75f);
    Result.SpurMeters = SpurShape * Settings.SpurReliefMeters * MountainStrength;

    // Shallow swales are negative ridged fields. They are intentionally not the
    // final river channels; later drainage captures and deepens the connected
    // portions, yielding dendritic valleys instead of stamped straight lines.
    const double SwaleScale = static_cast<double>(Settings.SwaleScaleMeters);
    const float SwaleA = Ridged(
        (X + WarpY * 180.0) / SwaleScale,
        (Y - WarpX * 180.0) / (SwaleScale * 1.35),
        Seed ^ 0xDB4F0B91u,
        4
    );
    const float SwaleThreshold = FMath::Clamp((SwaleA - 0.54f) / 0.46f, 0.0f, 1.0f);
    Result.SwaleMeters = -FMath::Square(SwaleThreshold) * Settings.SwaleDepthMeters *
        FMath::Lerp(0.55f, 1.0f, LowlandStrength);

    // Tens-of-metres breakup. This is intentionally low amplitude: it gives the
    // 1 m DEM something to resolve without turning the landscape into noise.
    const double FineScale = static_cast<double>(Settings.FineScaleMeters);
    const float Fine = Fbm(X / FineScale, Y / FineScale, Seed ^ 0x3C6EF372u, 4);
    const float FineRidge = Ridged(X / (FineScale * 1.8), Y / (FineScale * 1.8), Seed ^ 0xBB67AE85u, 3);
    Result.FineMeters = (Fine * 0.72f + (FineRidge - 0.52f) * 0.45f) *
        Settings.FineReliefMeters * FMath::Lerp(0.45f, 1.0f, MountainStrength);

    Result.HeightOffsetMeters = Result.HillMeters + Result.SpurMeters + Result.SwaleMeters + Result.FineMeters;
    return Result;
}
