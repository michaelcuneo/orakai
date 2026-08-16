#include "CubusCore/Generation/CubusTerrainCarving.h"

namespace CubusTerrainCarving
{
    struct FSegmentInfluence
    {
        float DistanceMeters = MAX_flt;
        float AreaSquareKm = 0.0f;
        float ProfileHeightMeters = 0.0f;
        int32 StreamOrder = 0;
    };

    float Smooth01(const float Value)
    {
        const float T = FMath::Clamp(Value, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    float Falloff(const float DistanceMeters, const float HalfWidthMeters)
    {
        if (HalfWidthMeters <= UE_SMALL_NUMBER || DistanceMeters >= HalfWidthMeters)
        {
            return 0.0f;
        }
        return 1.0f - Smooth01(DistanceMeters / HalfWidthMeters);
    }

    float DistanceToSegment(
        const FVector2D& Point,
        const FVector2D& A,
        const FVector2D& B,
        float& OutAlpha
    )
    {
        const FVector2D AB = B - A;
        const double LengthSquared = AB.SquaredLength();
        if (LengthSquared <= UE_SMALL_NUMBER)
        {
            OutAlpha = 0.0f;
            return static_cast<float>(FVector2D::Distance(Point, A));
        }

        const double Alpha = FMath::Clamp(
            FVector2D::DotProduct(Point - A, AB) / LengthSquared,
            0.0,
            1.0
        );
        OutAlpha = static_cast<float>(Alpha);
        return static_cast<float>(FVector2D::Distance(Point, A + AB * Alpha));
    }

    float AreaStrength(const float AreaKm2, const FCubusTerrainCarvingSettings& Settings)
    {
        const float SourceArea = FMath::Max(0.0001f, Settings.Drainage.StreamSourceAreaSquareKm);
        const float MajorArea = FMath::Max(SourceArea + 0.0001f, Settings.Drainage.MajorRiverAreaSquareKm);
        const float Normalized = FMath::Clamp(
            (AreaKm2 - SourceArea) / (MajorArea - SourceArea),
            0.0f,
            1.0f
        );
        return FMath::Pow(Normalized, FMath::Max(0.05f, Settings.AreaExponent));
    }

    FSegmentInfluence FindBestInfluence(
        const FVector2D& Point,
        const TArray<FCubusTerrainDrainageSegment>& Segments,
        const FCubusTerrainCarvingSettings& Settings
    )
    {
        FSegmentInfluence Best;
        float BestNormalizedDistance = MAX_flt;
        int32 Considered = 0;

        for (const FCubusTerrainDrainageSegment& Segment : Segments)
        {
            const float Strength = AreaStrength(Segment.ContributingAreaSquareKm, Settings);
            const float ValleyWidth = FMath::Lerp(
                Settings.HeadwaterValleyHalfWidthMeters,
                Settings.MajorValleyHalfWidthMeters,
                Strength
            );

            float Alpha = 0.0f;
            const float Distance = DistanceToSegment(Point, Segment.StartMeters, Segment.EndMeters, Alpha);
            if (Distance > ValleyWidth)
            {
                continue;
            }

            const float NormalizedDistance = Distance / FMath::Max(1.0f, ValleyWidth);
            if (NormalizedDistance < BestNormalizedDistance)
            {
                BestNormalizedDistance = NormalizedDistance;
                Best.DistanceMeters = Distance;
                Best.AreaSquareKm = Segment.ContributingAreaSquareKm;
                Best.ProfileHeightMeters = FMath::Lerp(
                    Segment.StartElevationMeters,
                    Segment.EndElevationMeters,
                    Alpha
                );
                Best.StreamOrder = Segment.StrahlerOrder;
            }

            ++Considered;
            if (Considered >= FMath::Max(1, Settings.MaxNearbySegments))
            {
                break;
            }
        }

        return Best;
    }

    FCubusTerrainCarvingSample Evaluate(
        const FVector2D& Point,
        const float OriginalHeightMeters,
        const TArray<FCubusTerrainDrainageSegment>& Segments,
        const FCubusTerrainCarvingSettings& Settings
    )
    {
        FCubusTerrainCarvingSample Result;
        Result.OriginalHeightMeters = OriginalHeightMeters;
        Result.CarvedHeightMeters = OriginalHeightMeters;

        const FSegmentInfluence Influence = FindBestInfluence(Point, Segments, Settings);
        if (Influence.DistanceMeters == MAX_flt)
        {
            return Result;
        }

        const float Strength = AreaStrength(Influence.AreaSquareKm, Settings);
        const float OrderBoost = FMath::Clamp((static_cast<float>(Influence.StreamOrder) - 1.0f) * 0.08f, 0.0f, 0.28f);
        const float MatureStrength = FMath::Clamp(Strength + OrderBoost, 0.0f, 1.0f);

        const float ValleyWidth = FMath::Lerp(Settings.HeadwaterValleyHalfWidthMeters, Settings.MajorValleyHalfWidthMeters, MatureStrength);
        const float FloodplainWidth = FMath::Min(
            ValleyWidth * 0.82f,
            FMath::Lerp(Settings.HeadwaterFloodplainHalfWidthMeters, Settings.MajorFloodplainHalfWidthMeters, MatureStrength)
        );
        const float ChannelWidth = FMath::Min(
            FloodplainWidth * 0.55f,
            FMath::Lerp(Settings.HeadwaterChannelHalfWidthMeters, Settings.MajorChannelHalfWidthMeters, MatureStrength)
        );
        const float BankWidth = FMath::Min(FloodplainWidth, ChannelWidth * FMath::Max(1.1f, Settings.BankWidthScale));

        Result.ValleyWeight = Falloff(Influence.DistanceMeters, ValleyWidth);
        Result.FloodplainWeight = Falloff(Influence.DistanceMeters, FloodplainWidth);
        Result.BankWeight = Falloff(Influence.DistanceMeters, BankWidth);
        Result.ChannelWeight = Falloff(Influence.DistanceMeters, ChannelWidth);
        Result.ContributingAreaSquareKm = Influence.AreaSquareKm;
        Result.StrahlerOrder = Influence.StreamOrder;

        const float ValleyDepth = FMath::Lerp(Settings.HeadwaterValleyDepthMeters, Settings.MajorValleyDepthMeters, MatureStrength);
        const float FloodplainDepth = FMath::Lerp(Settings.HeadwaterFloodplainDepthMeters, Settings.MajorFloodplainDepthMeters, MatureStrength);
        const float ChannelDepth = FMath::Lerp(Settings.HeadwaterChannelDepthMeters, Settings.MajorChannelDepthMeters, MatureStrength);

        // The broad valley is a smooth incision into the existing landscape.
        // Floodplain and channel then converge on the monotonically routed
        // hydraulic profile, preventing disconnected line/lump/line cuts.
        float Carved = OriginalHeightMeters - ValleyDepth * Result.ValleyWeight;

        if (Result.FloodplainWeight > 0.0f)
        {
            const float FloodplainTarget = Influence.ProfileHeightMeters - FloodplainDepth;
            Carved = FMath::Lerp(Carved, FMath::Min(Carved, FloodplainTarget), Result.FloodplainWeight);
        }

        if (Result.BankWeight > 0.0f)
        {
            const float BankT = Result.BankWeight * Result.BankWeight;
            const float BankTarget = Influence.ProfileHeightMeters - FloodplainDepth - ChannelDepth * 0.35f;
            Carved = FMath::Lerp(Carved, FMath::Min(Carved, BankTarget), BankT);
        }

        if (Result.ChannelWeight > 0.0f)
        {
            const float ChannelTarget = Influence.ProfileHeightMeters - FloodplainDepth - ChannelDepth;
            Carved = FMath::Lerp(Carved, FMath::Min(Carved, ChannelTarget), Result.ChannelWeight);
        }

        Result.CarvedHeightMeters = FMath::Min(OriginalHeightMeters, Carved);
        Result.TotalIncisionMeters = OriginalHeightMeters - Result.CarvedHeightMeters;
        return Result;
    }

    float MaximumValleyWidth(const FCubusTerrainCarvingSettings& Settings)
    {
        return FMath::Max(Settings.HeadwaterValleyHalfWidthMeters, Settings.MajorValleyHalfWidthMeters);
    }
}

FCubusTerrainRasterTile FCubusTerrainCarving::CarveTile(
    const FCubusTerrainRasterTile& StructuralTile,
    const FCubusTerrainCarvingSettings& Settings
)
{
    using namespace CubusTerrainCarving;

    if (!StructuralTile.IsValid())
    {
        return StructuralTile;
    }

    FCubusTerrainRasterTile Result = StructuralTile;
    const float Expansion = MaximumValleyWidth(Settings) + Settings.Drainage.AnalysisCellSizeMeters * 2.0f;
    const FVector2D Minimum = StructuralTile.GetWorldMinimumMeters();
    const FVector2D Maximum = StructuralTile.GetWorldMaximumMeters();

    TArray<FCubusTerrainDrainageSegment> Segments;
    FCubusTerrainDrainage::CollectSegments(
        FBox2D(Minimum - FVector2D(Expansion, Expansion), Maximum + FVector2D(Expansion, Expansion)),
        Settings.Drainage,
        Segments
    );

    for (int32 StorageY = 0; StorageY < Result.StorageSampleCount; ++StorageY)
    {
        const int32 GridY = StorageY - Result.HaloSamples;
        const double WorldY = Minimum.Y + static_cast<double>(GridY) * Result.SampleSpacingMeters;

        for (int32 StorageX = 0; StorageX < Result.StorageSampleCount; ++StorageX)
        {
            const int32 GridX = StorageX - Result.HaloSamples;
            const double WorldX = Minimum.X + static_cast<double>(GridX) * Result.SampleSpacingMeters;
            const int32 Index = StorageY * Result.StorageSampleCount + StorageX;
            const float Original = StructuralTile.HeightMeters[Index];
            Result.HeightMeters[Index] = Evaluate(FVector2D(WorldX, WorldY), Original, Segments, Settings).CarvedHeightMeters;
        }
    }

    return Result;
}

FCubusTerrainCarvingSample FCubusTerrainCarving::Sample(
    const double WorldXmeters,
    const double WorldYmeters,
    const FCubusTerrainCarvingSettings& Settings
)
{
    using namespace CubusTerrainCarving;

    const float Original = FCubusTerrainRasterBuilder::SampleStructuralHeightMeters(
        WorldXmeters,
        WorldYmeters,
        Settings.Drainage.Raster
    );

    const float Expansion = MaximumValleyWidth(Settings) + Settings.Drainage.AnalysisCellSizeMeters * 2.0f;
    TArray<FCubusTerrainDrainageSegment> Segments;
    FCubusTerrainDrainage::CollectSegments(
        FBox2D(
            FVector2D(WorldXmeters - Expansion, WorldYmeters - Expansion),
            FVector2D(WorldXmeters + Expansion, WorldYmeters + Expansion)
        ),
        Settings.Drainage,
        Segments
    );

    return Evaluate(FVector2D(WorldXmeters, WorldYmeters), Original, Segments, Settings);
}
