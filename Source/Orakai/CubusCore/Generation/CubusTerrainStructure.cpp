#include "CubusCore/Generation/CubusTerrainStructure.h"

namespace CubusTerrainStructure
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

    float Hash01(const int32 X, const int32 Y, const uint32 Seed)
    {
        return static_cast<float>(Hash2D(X, Y, Seed) & 0x00ffffffu) /
            static_cast<float>(0x00ffffffu);
    }

    float Smooth01(const float T)
    {
        const float X = FMath::Clamp(T, 0.0f, 1.0f);
        return X * X * (3.0f - 2.0f * X);
    }

    float SmoothPulse(const float Distance, const float Inner, const float Outer)
    {
        if (Distance <= Inner)
        {
            return 1.0f;
        }
        if (Distance >= Outer)
        {
            return 0.0f;
        }
        return 1.0f - Smooth01((Distance - Inner) / FMath::Max(KINDA_SMALL_NUMBER, Outer - Inner));
    }

    FVector2D NormalizeSafe(const FVector2D& V)
    {
        const double Length = V.Size();
        return Length > UE_SMALL_NUMBER ? V / Length : FVector2D(1.0, 0.0);
    }

    float DistanceToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B, float& OutAlpha)
    {
        const FVector2D AB = B - A;
        const double Denominator = AB.SquaredLength();
        if (Denominator <= UE_SMALL_NUMBER)
        {
            OutAlpha = 0.0f;
            return static_cast<float>(FVector2D::Distance(P, A));
        }

        const double Alpha = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Denominator, 0.0, 1.0);
        OutAlpha = static_cast<float>(Alpha);
        return static_cast<float>(FVector2D::Distance(P, A + AB * Alpha));
    }

    float ValueNoise(const double X, const double Y, const uint32 Seed)
    {
        const int32 X0 = FMath::FloorToInt(X);
        const int32 Y0 = FMath::FloorToInt(Y);
        const int32 X1 = X0 + 1;
        const int32 Y1 = Y0 + 1;
        const float Tx = Smooth01(static_cast<float>(X - static_cast<double>(X0)));
        const float Ty = Smooth01(static_cast<float>(Y - static_cast<double>(Y0)));
        const float A = FMath::Lerp(Hash01(X0, Y0, Seed), Hash01(X1, Y0, Seed), Tx);
        const float B = FMath::Lerp(Hash01(X0, Y1, Seed), Hash01(X1, Y1, Seed), Tx);
        return FMath::Lerp(A, B, Ty) * 2.0f - 1.0f;
    }

    float Fbm(double X, double Y, const uint32 Seed)
    {
        float Sum = 0.0f;
        float Weight = 0.0f;
        float Amplitude = 1.0f;
        for (int32 Octave = 0; Octave < 4; ++Octave)
        {
            Sum += ValueNoise(X, Y, Seed + static_cast<uint32>(Octave) * 1013u) * Amplitude;
            Weight += Amplitude;
            X = X * 2.03 + 17.13;
            Y = Y * 2.03 - 11.71;
            Amplitude *= 0.5f;
        }
        return Weight > 0.0f ? Sum / Weight : 0.0f;
    }

    struct FRangeSystem
    {
        int32 CellX = 0;
        int32 CellY = 0;
        FVector2D Centre = FVector2D::ZeroVector;
        FVector2D Direction = FVector2D(1.0, 0.0);
        FVector2D Normal = FVector2D(0.0, 1.0);
        float ReliefScale = 1.0f;
        float WidthScale = 1.0f;
    };

    FRangeSystem MakeSystem(const int32 CellX, const int32 CellY, const FCubusTerrainStructureSettings& Settings)
    {
        const double Spacing = FMath::Max(4000.0, static_cast<double>(Settings.MountainSystemSpacingKm) * 1000.0);
        const uint32 Seed = static_cast<uint32>(Settings.Seed);
        const float JitterX = (Hash01(CellX, CellY, Seed ^ 0xA341316Cu) * 2.0f - 1.0f) * 0.28f;
        const float JitterY = (Hash01(CellX, CellY, Seed ^ 0xC8013EA4u) * 2.0f - 1.0f) * 0.28f;
        const double Angle = static_cast<double>(Hash01(CellX, CellY, Seed ^ 0xAD90777Du)) * PI;

        FRangeSystem System;
        System.CellX = CellX;
        System.CellY = CellY;
        System.Centre = FVector2D(
            (static_cast<double>(CellX) + 0.5 + JitterX) * Spacing,
            (static_cast<double>(CellY) + 0.5 + JitterY) * Spacing
        );
        System.Direction = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
        System.Normal = FVector2D(-System.Direction.Y, System.Direction.X);
        System.ReliefScale = FMath::Lerp(0.72f, 1.18f, Hash01(CellX, CellY, Seed ^ 0xB5297A4Du));
        System.WidthScale = FMath::Lerp(0.80f, 1.24f, Hash01(CellX, CellY, Seed ^ 0x68E31DA4u));
        return System;
    }

    FVector2D MakeNode(const FRangeSystem& System, const int32 NodeIndex, const FCubusTerrainStructureSettings& Settings)
    {
        const double NodeSpacing = FMath::Max(2500.0, static_cast<double>(Settings.RangeNodeSpacingKm) * 1000.0);
        const uint32 Seed = static_cast<uint32>(Settings.Seed) ^ 0x1B56C4E9u;
        const float Along = (Hash01(System.CellX * 97 + NodeIndex, System.CellY * 53 - NodeIndex, Seed) * 2.0f - 1.0f) * Settings.AlongSpineJitter;
        const float Across = (Hash01(System.CellX * 131 - NodeIndex, System.CellY * 71 + NodeIndex, Seed ^ 0x9E3779B9u) * 2.0f - 1.0f) * Settings.AcrossSpineJitter;
        const double Longitudinal = (static_cast<double>(NodeIndex) + static_cast<double>(Along)) * NodeSpacing;
        return System.Centre +
            System.Direction * Longitudinal +
            System.Normal * (static_cast<double>(Across) * NodeSpacing);
    }

    void AccumulateRange(
        const FVector2D& P,
        const FRangeSystem& System,
        const FCubusTerrainStructureSettings& Settings,
        FCubusTerrainStructureSample& Result,
        float& OutRangeHeight
    )
    {
        const double NodeSpacing = FMath::Max(2500.0, static_cast<double>(Settings.RangeNodeSpacingKm) * 1000.0);
        const FVector2D Relative = P - System.Centre;
        const double Along = FVector2D::DotProduct(Relative, System.Direction) / NodeSpacing;
        const int32 CentreNode = FMath::FloorToInt(Along);

        const float CoreWidth = Settings.RangeCoreHalfWidthKm * 1000.0f * System.WidthScale;
        const float ShoulderWidth = Settings.RangeShoulderHalfWidthKm * 1000.0f * System.WidthScale;

        for (int32 Node = CentreNode - 2; Node <= CentreNode + 2; ++Node)
        {
            const FVector2D A = MakeNode(System, Node, Settings);
            const FVector2D B = MakeNode(System, Node + 1, Settings);
            float Alpha = 0.0f;
            const float Distance = DistanceToSegment(P, A, B, Alpha);
            Result.DistanceToRangeMeters = FMath::Min(Result.DistanceToRangeMeters, Distance);

            const float Shoulder = SmoothPulse(Distance, CoreWidth * 0.65f, ShoulderWidth);
            const float Core = SmoothPulse(Distance, CoreWidth * 0.20f, CoreWidth);
            const float SegmentRhythm = 0.82f + 0.18f * FMath::Sin((static_cast<float>(Node) + Alpha) * 1.73f + static_cast<float>(System.CellX * 3 + System.CellY));
            const float RangeWeight = Shoulder * SegmentRhythm;
            const float Relief = Settings.RangeReliefMeters * System.ReliefScale;
            const float RangeHeight = Relief * (0.34f * Shoulder + 0.66f * Core) * SegmentRhythm;

            Result.RangeWeight = FMath::Max(Result.RangeWeight, RangeWeight);
            Result.RangeCoreWeight = FMath::Max(Result.RangeCoreWeight, Core);
            OutRangeHeight = FMath::Max(OutRangeHeight, RangeHeight);

            const FVector2D NodePosition = Alpha < 0.5f ? A : B;
            const int32 MassifNode = Alpha < 0.5f ? Node : Node + 1;
            const uint32 Seed = static_cast<uint32>(Settings.Seed) ^ 0xD1B54A35u;
            const float MassifScale = FMath::Lerp(0.72f, 1.24f, Hash01(System.CellX * 193 + MassifNode, System.CellY * 151 - MassifNode, Seed));
            const float MassifRadius = Settings.MassifRadiusKm * 1000.0f * MassifScale;
            const float MassifDistance = static_cast<float>(FVector2D::Distance(P, NodePosition));
            const float MassifWeight = SmoothPulse(MassifDistance, MassifRadius * 0.18f, MassifRadius);
            Result.MassifWeight = FMath::Max(Result.MassifWeight, MassifWeight * Core);
            OutRangeHeight = FMath::Max(
                OutRangeHeight,
                RangeHeight + Settings.MassifReliefMeters * MassifWeight * Core * System.ReliefScale
            );

            if (Hash01(System.CellX * 211 + MassifNode, System.CellY * 157 - MassifNode, Seed ^ 0x94D049BBu) < Settings.PassChance)
            {
                const float PassWidth = FMath::Max(250.0f, Settings.PassHalfWidthKm * 1000.0f);
                const float PassWeight = SmoothPulse(MassifDistance, PassWidth * 0.15f, PassWidth);
                Result.PassWeight = FMath::Max(Result.PassWeight, PassWeight * Core);
                OutRangeHeight -= Settings.PassDepthMeters * PassWeight * Core;
            }
        }
    }

    void AccumulateBranches(
        const FVector2D& P,
        const FRangeSystem& A,
        const FCubusTerrainStructureSettings& Settings,
        FCubusTerrainStructureSample& Result,
        float& OutRangeHeight
    )
    {
        const uint32 Seed = static_cast<uint32>(Settings.Seed) ^ 0xED5AD4BBu;
        const int32 NeighbourOffsets[2][2] = { {1, 0}, {0, 1} };
        for (const auto& Offset : NeighbourOffsets)
        {
            if (Hash01(A.CellX * 17 + Offset[0], A.CellY * 19 + Offset[1], Seed) >= Settings.BranchChance)
            {
                continue;
            }

            const FRangeSystem B = MakeSystem(A.CellX + Offset[0], A.CellY + Offset[1], Settings);
            const FVector2D Start = MakeNode(A, 0, Settings);
            const FVector2D End = MakeNode(B, 0, Settings);
            float Alpha = 0.0f;
            const float Distance = DistanceToSegment(P, Start, End, Alpha);
            const float Width = Settings.RangeCoreHalfWidthKm * 1000.0f * Settings.BranchWidthScale;
            const float Shoulder = SmoothPulse(Distance, Width * 0.35f, Width * 2.6f);
            const float Core = SmoothPulse(Distance, Width * 0.10f, Width);
            Result.RangeWeight = FMath::Max(Result.RangeWeight, Shoulder * 0.65f);
            Result.RangeCoreWeight = FMath::Max(Result.RangeCoreWeight, Core * 0.55f);
            Result.DistanceToRangeMeters = FMath::Min(Result.DistanceToRangeMeters, Distance);
            const float Taper = FMath::Sin(FMath::Clamp(Alpha, 0.0f, 1.0f) * PI);
            OutRangeHeight = FMath::Max(
                OutRangeHeight,
                Settings.RangeReliefMeters * Settings.BranchReliefScale * (0.35f * Shoulder + 0.65f * Core) * Taper
            );
        }
    }

    void AccumulateBasins(
        const FVector2D& P,
        const FCubusTerrainStructureSettings& Settings,
        FCubusTerrainStructureSample& Result,
        float& OutBasinHeight
    )
    {
        const double CellSize = FMath::Max(6000.0, static_cast<double>(Settings.BasinCellSizeKm) * 1000.0);
        const int32 CellX = FMath::FloorToInt(P.X / CellSize);
        const int32 CellY = FMath::FloorToInt(P.Y / CellSize);
        const uint32 Seed = static_cast<uint32>(Settings.Seed) ^ 0xDB4F0B91u;

        for (int32 Y = CellY - 1; Y <= CellY + 1; ++Y)
        {
            for (int32 X = CellX - 1; X <= CellX + 1; ++X)
            {
                const float JitterX = (Hash01(X, Y, Seed) * 2.0f - 1.0f) * 0.28f;
                const float JitterY = (Hash01(X, Y, Seed ^ 0xBB67AE85u) * 2.0f - 1.0f) * 0.28f;
                const FVector2D Centre(
                    (static_cast<double>(X) + 0.5 + JitterX) * CellSize,
                    (static_cast<double>(Y) + 0.5 + JitterY) * CellSize
                );
                const float Radius = Settings.BasinRadiusKm * 1000.0f * FMath::Lerp(0.78f, 1.20f, Hash01(X, Y, Seed ^ 0x3C6EF372u));
                const float Distance = static_cast<float>(FVector2D::Distance(P, Centre));
                const float Weight = SmoothPulse(Distance, Radius * 0.18f, Radius);
                Result.BasinWeight = FMath::Max(Result.BasinWeight, Weight);
                OutBasinHeight = FMath::Min(OutBasinHeight, -Settings.BasinDepthMeters * Weight);
            }
        }
    }
}

FCubusTerrainStructureSample FCubusTerrainStructure::Sample(
    const double WorldXmeters,
    const double WorldYmeters,
    const FCubusTerrainStructureSettings& InSettings
)
{
    using namespace CubusTerrainStructure;

    FCubusTerrainStructureSettings Settings = InSettings;
    Settings.RegionalScaleKm = FMath::Max(8.0f, Settings.RegionalScaleKm);
    Settings.MountainSystemSpacingKm = FMath::Max(8.0f, Settings.MountainSystemSpacingKm);
    Settings.RangeNodeSpacingKm = FMath::Max(3.0f, Settings.RangeNodeSpacingKm);
    Settings.RangeCoreHalfWidthKm = FMath::Max(0.4f, Settings.RangeCoreHalfWidthKm);
    Settings.RangeShoulderHalfWidthKm = FMath::Max(Settings.RangeCoreHalfWidthKm + 0.5f, Settings.RangeShoulderHalfWidthKm);
    Settings.BasinCellSizeKm = FMath::Max(8.0f, Settings.BasinCellSizeKm);
    Settings.BasinRadiusKm = FMath::Max(2.0f, Settings.BasinRadiusKm);

    FCubusTerrainStructureSample Result;
    const FVector2D P(WorldXmeters, WorldYmeters);

    const double RegionalScale = static_cast<double>(Settings.RegionalScaleKm) * 1000.0;
    const float RegionalNoise = Fbm(WorldXmeters / RegionalScale, WorldYmeters / RegionalScale, static_cast<uint32>(Settings.Seed) ^ 0x243F6A88u);
    Result.RegionalHeightMeters = Settings.BaseElevationMeters + RegionalNoise * Settings.RegionalReliefMeters;

    float RangeHeight = 0.0f;
    const double SystemSpacing = static_cast<double>(Settings.MountainSystemSpacingKm) * 1000.0;
    const int32 SystemX = FMath::FloorToInt(WorldXmeters / SystemSpacing);
    const int32 SystemY = FMath::FloorToInt(WorldYmeters / SystemSpacing);

    for (int32 Y = SystemY - 1; Y <= SystemY + 1; ++Y)
    {
        for (int32 X = SystemX - 1; X <= SystemX + 1; ++X)
        {
            const FRangeSystem System = MakeSystem(X, Y, Settings);
            AccumulateRange(P, System, Settings, Result, RangeHeight);
            AccumulateBranches(P, System, Settings, Result, RangeHeight);
        }
    }

    float BasinHeight = 0.0f;
    AccumulateBasins(P, Settings, Result, BasinHeight);

    // Basins are suppressed under mountain shoulders. This creates broad
    // intermontane/lowland bowls without drilling circular craters through a range.
    const float BasinSuppression = 1.0f - FMath::Clamp(Result.RangeWeight * 0.92f, 0.0f, 0.95f);
    BasinHeight *= BasinSuppression;

    Result.HeightMeters = Result.RegionalHeightMeters + RangeHeight + BasinHeight;
    return Result;
}
