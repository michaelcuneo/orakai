#include "CubusCore/Generation/CubusTerrainCarving.h"

namespace CubusTerrainCarving
{
    struct FSegmentIndex
    {
        float BucketSizeMeters = 96.0f;
        TMap<FIntPoint, TArray<int32>> Buckets;
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

    float MatureStrength(
        const FCubusTerrainDrainageSegment& Segment,
        const FCubusTerrainCarvingSettings& Settings
    )
    {
        const float Strength = AreaStrength(Segment.ContributingAreaSquareKm, Settings);
        const float OrderBoost = FMath::Clamp(
            (static_cast<float>(Segment.StrahlerOrder) - 1.0f) * 0.08f,
            0.0f,
            0.28f
        );
        return FMath::Clamp(Strength + OrderBoost, 0.0f, 1.0f);
    }

    float ValleyWidthForSegment(
        const FCubusTerrainDrainageSegment& Segment,
        const FCubusTerrainCarvingSettings& Settings
    )
    {
        return FMath::Lerp(
            Settings.HeadwaterValleyHalfWidthMeters,
            Settings.MajorValleyHalfWidthMeters,
            MatureStrength(Segment, Settings)
        );
    }

    FIntPoint BucketCoordinate(const FVector2D& Point, const float BucketSizeMeters)
    {
        const double SafeBucket = FMath::Max(16.0, static_cast<double>(BucketSizeMeters));
        return FIntPoint(
            FMath::FloorToInt(Point.X / SafeBucket),
            FMath::FloorToInt(Point.Y / SafeBucket)
        );
    }

    FSegmentIndex BuildSegmentIndex(
        const TArray<FCubusTerrainDrainageSegment>& Segments,
        const FCubusTerrainCarvingSettings& Settings
    )
    {
        FSegmentIndex Index;
        Index.BucketSizeMeters = FMath::Max(32.0f, Settings.SegmentBucketSizeMeters);

        for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
        {
            const FCubusTerrainDrainageSegment& Segment = Segments[SegmentIndex];

            // Priority-filled depressions are lake/basin candidates. The routed
            // receiver still crosses them so catchment topology remains intact,
            // but carving that synthetic route would cut a trench through the
            // lake floor and destroy the basin we deliberately preserved.
            if (Segment.bLakeTraversal)
            {
                continue;
            }

            const float Radius = ValleyWidthForSegment(Segment, Settings);
            const FVector2D Minimum(
                FMath::Min(Segment.StartMeters.X, Segment.EndMeters.X) - Radius,
                FMath::Min(Segment.StartMeters.Y, Segment.EndMeters.Y) - Radius
            );
            const FVector2D Maximum(
                FMath::Max(Segment.StartMeters.X, Segment.EndMeters.X) + Radius,
                FMath::Max(Segment.StartMeters.Y, Segment.EndMeters.Y) + Radius
            );

            const FIntPoint MinBucket = BucketCoordinate(Minimum, Index.BucketSizeMeters);
            const FIntPoint MaxBucket = BucketCoordinate(Maximum, Index.BucketSizeMeters);
            for (int32 Y = MinBucket.Y; Y <= MaxBucket.Y; ++Y)
            {
                for (int32 X = MinBucket.X; X <= MaxBucket.X; ++X)
                {
                    Index.Buckets.FindOrAdd(FIntPoint(X, Y)).Add(SegmentIndex);
                }
            }
        }

        return Index;
    }

    FCubusTerrainCarvingSample Evaluate(
        const FVector2D& Point,
        const float OriginalHeightMeters,
        const TArray<FCubusTerrainDrainageSegment>& Segments,
        const FSegmentIndex& SegmentIndex,
        const FCubusTerrainCarvingSettings& Settings
    )
    {
        FCubusTerrainCarvingSample Result;
        Result.OriginalHeightMeters = OriginalHeightMeters;
        Result.CarvedHeightMeters = OriginalHeightMeters;

        const TArray<int32>* CandidateIndices = SegmentIndex.Buckets.Find(
            BucketCoordinate(Point, SegmentIndex.BucketSizeMeters)
        );
        if (CandidateIndices == nullptr)
        {
            return Result;
        }

        float BestArea = 0.0f;
        int32 BestOrder = 0;

        for (const int32 Index : *CandidateIndices)
        {
            if (!Segments.IsValidIndex(Index))
            {
                continue;
            }

            const FCubusTerrainDrainageSegment& Segment = Segments[Index];
            if (Segment.bLakeTraversal)
            {
                continue;
            }

            const float Strength = MatureStrength(Segment, Settings);
            const float ValleyWidth = FMath::Lerp(
                Settings.HeadwaterValleyHalfWidthMeters,
                Settings.MajorValleyHalfWidthMeters,
                Strength
            );

            float Alpha = 0.0f;
            const float Distance = DistanceToSegment(Point, Segment.StartMeters, Segment.EndMeters, Alpha);
            if (Distance >= ValleyWidth)
            {
                continue;
            }

            const float FloodplainWidth = FMath::Min(
                ValleyWidth * 0.82f,
                FMath::Lerp(
                    Settings.HeadwaterFloodplainHalfWidthMeters,
                    Settings.MajorFloodplainHalfWidthMeters,
                    Strength
                )
            );
            const float ChannelWidth = FMath::Min(
                FloodplainWidth * 0.55f,
                FMath::Lerp(
                    Settings.HeadwaterChannelHalfWidthMeters,
                    Settings.MajorChannelHalfWidthMeters,
                    Strength
                )
            );
            const float BankWidth = FMath::Min(
                FloodplainWidth,
                ChannelWidth * FMath::Max(1.1f, Settings.BankWidthScale)
            );

            const float ValleyWeight = Falloff(Distance, ValleyWidth);
            const float FloodplainWeight = Falloff(Distance, FloodplainWidth);
            const float BankWeight = Falloff(Distance, BankWidth);
            const float ChannelWeight = Falloff(Distance, ChannelWidth);

            const float ValleyDepth = FMath::Lerp(
                Settings.HeadwaterValleyDepthMeters,
                Settings.MajorValleyDepthMeters,
                Strength
            );
            const float FloodplainDepth = FMath::Lerp(
                Settings.HeadwaterFloodplainDepthMeters,
                Settings.MajorFloodplainDepthMeters,
                Strength
            );
            const float ChannelDepth = FMath::Lerp(
                Settings.HeadwaterChannelDepthMeters,
                Settings.MajorChannelDepthMeters,
                Strength
            );
            const float ProfileHeight = FMath::Lerp(
                Segment.StartElevationMeters,
                Segment.EndElevationMeters,
                Alpha
            );

            float CandidateHeight = OriginalHeightMeters - ValleyDepth * ValleyWeight;
            if (FloodplainWeight > 0.0f)
            {
                const float FloodplainTarget = ProfileHeight - FloodplainDepth;
                CandidateHeight = FMath::Lerp(
                    CandidateHeight,
                    FMath::Min(CandidateHeight, FloodplainTarget),
                    FloodplainWeight
                );
            }
            if (BankWeight > 0.0f)
            {
                const float BankTarget = ProfileHeight - FloodplainDepth - ChannelDepth * 0.35f;
                CandidateHeight = FMath::Lerp(
                    CandidateHeight,
                    FMath::Min(CandidateHeight, BankTarget),
                    BankWeight * BankWeight
                );
            }
            if (ChannelWeight > 0.0f)
            {
                const float ChannelTarget = ProfileHeight - FloodplainDepth - ChannelDepth;
                CandidateHeight = FMath::Lerp(
                    CandidateHeight,
                    FMath::Min(CandidateHeight, ChannelTarget),
                    ChannelWeight
                );
            }

            // Union all overlapping valley cross-sections. Taking the minimum is
            // commutative, so results no longer depend on segment collection order.
            Result.CarvedHeightMeters = FMath::Min(Result.CarvedHeightMeters, CandidateHeight);
            Result.ValleyWeight = FMath::Max(Result.ValleyWeight, ValleyWeight);
            Result.FloodplainWeight = FMath::Max(Result.FloodplainWeight, FloodplainWeight);
            Result.BankWeight = FMath::Max(Result.BankWeight, BankWeight);
            Result.ChannelWeight = FMath::Max(Result.ChannelWeight, ChannelWeight);

            if (Segment.ContributingAreaSquareKm > BestArea ||
                (FMath::IsNearlyEqual(Segment.ContributingAreaSquareKm, BestArea) && Segment.StrahlerOrder > BestOrder))
            {
                BestArea = Segment.ContributingAreaSquareKm;
                BestOrder = Segment.StrahlerOrder;
            }
        }

        Result.CarvedHeightMeters = FMath::Min(OriginalHeightMeters, Result.CarvedHeightMeters);
        Result.TotalIncisionMeters = OriginalHeightMeters - Result.CarvedHeightMeters;
        Result.ContributingAreaSquareKm = BestArea;
        Result.StrahlerOrder = BestOrder;
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
    const FSegmentIndex SegmentIndex = BuildSegmentIndex(Segments, Settings);

    for (int32 StorageY = 0; StorageY < Result.StorageSampleCount; ++StorageY)
    {
        const int32 GridY = StorageY - Result.HaloSamples;
        const double WorldY = Minimum.Y + static_cast<double>(GridY) * Result.SampleSpacingMeters;

        for (int32 StorageX = 0; StorageX < Result.StorageSampleCount; ++StorageX)
        {
            const int32 GridX = StorageX - Result.HaloSamples;
            const double WorldX = Minimum.X + static_cast<double>(GridX) * Result.SampleSpacingMeters;
            const int32 StorageIndex = StorageY * Result.StorageSampleCount + StorageX;
            const float Original = StructuralTile.HeightMeters[StorageIndex];
            Result.HeightMeters[StorageIndex] = Evaluate(
                FVector2D(WorldX, WorldY),
                Original,
                Segments,
                SegmentIndex,
                Settings
            ).CarvedHeightMeters;
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
    const FSegmentIndex SegmentIndex = BuildSegmentIndex(Segments, Settings);

    return Evaluate(
        FVector2D(WorldXmeters, WorldYmeters),
        Original,
        Segments,
        SegmentIndex,
        Settings
    );
}
