#include "CubusCore/Generation/CubusTerrainDeposition.h"

namespace CubusTerrainDeposition
{
    struct FFan
    {
        FVector2D ApexMeters = FVector2D::ZeroVector;
        FVector2D Direction = FVector2D::ZeroVector;
        float ApexElevationMeters = 0.0f;
        float RadiusMeters = 0.0f;
        float ThicknessMeters = 0.0f;
        float SurfaceGrade = 0.0f;
    };

    float Smooth01(const float Value)
    {
        const float T = FMath::Clamp(Value, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    float AreaStrength(
        const FCubusTerrainDrainageSegment& Segment,
        const FCubusTerrainCarvingSettings& CarvingSettings
    )
    {
        const float SourceArea = FMath::Max(0.0001f, CarvingSettings.Drainage.StreamSourceAreaSquareKm);
        const float MajorArea = FMath::Max(SourceArea + 0.0001f, CarvingSettings.Drainage.MajorRiverAreaSquareKm);
        const float Normalized = FMath::Clamp(
            (Segment.ContributingAreaSquareKm - SourceArea) / (MajorArea - SourceArea),
            0.0f,
            1.0f
        );
        const float Area = FMath::Pow(Normalized, FMath::Max(0.05f, CarvingSettings.AreaExponent));
        const float OrderBoost = FMath::Clamp(
            (static_cast<float>(Segment.StrahlerOrder) - 1.0f) * 0.08f,
            0.0f,
            0.28f
        );
        return FMath::Clamp(Area + OrderBoost, 0.0f, 1.0f);
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

    float SegmentSlope(const FCubusTerrainDrainageSegment& Segment)
    {
        const double Length = FVector2D::Distance(Segment.StartMeters, Segment.EndMeters);
        if (Length <= UE_SMALL_NUMBER)
        {
            return 0.0f;
        }
        return FMath::Max(
            0.0f,
            (Segment.StartElevationMeters - Segment.EndElevationMeters) / static_cast<float>(Length)
        );
    }

    bool SegmentTouchesBounds(
        const FCubusTerrainDrainageSegment& Segment,
        const FBox2D& Bounds,
        const float Expansion
    )
    {
        const FBox2D Expanded(
            Bounds.Min - FVector2D(Expansion, Expansion),
            Bounds.Max + FVector2D(Expansion, Expansion)
        );
        const FVector2D Minimum(
            FMath::Min(Segment.StartMeters.X, Segment.EndMeters.X),
            FMath::Min(Segment.StartMeters.Y, Segment.EndMeters.Y)
        );
        const FVector2D Maximum(
            FMath::Max(Segment.StartMeters.X, Segment.EndMeters.X),
            FMath::Max(Segment.StartMeters.Y, Segment.EndMeters.Y)
        );
        return FBox2D(Minimum, Maximum).Intersect(Expanded);
    }

    const FCubusTerrainDrainageSegment* FindDownstreamSegment(
        const FCubusTerrainDrainageSegment& Incoming,
        const TArray<FCubusTerrainDrainageSegment>& Segments,
        const float MatchToleranceMeters
    )
    {
        const FCubusTerrainDrainageSegment* Best = nullptr;
        float BestDistance = MatchToleranceMeters;
        for (const FCubusTerrainDrainageSegment& Candidate : Segments)
        {
            if (&Candidate == &Incoming || Candidate.bLakeTraversal)
            {
                continue;
            }

            const float Distance = static_cast<float>(FVector2D::Distance(Incoming.EndMeters, Candidate.StartMeters));
            if (Distance > BestDistance)
            {
                continue;
            }

            if (Candidate.ContributingAreaSquareKm + KINDA_SMALL_NUMBER < Incoming.ContributingAreaSquareKm)
            {
                continue;
            }

            BestDistance = Distance;
            Best = &Candidate;
        }
        return Best;
    }

    TArray<FFan> BuildFans(
        const FBox2D& TileBounds,
        const FCubusTerrainDepositionSettings& Settings,
        const FCubusTerrainCarvingSettings& CarvingSettings,
        const TArray<FCubusTerrainDrainageSegment>& Segments
    )
    {
        TArray<FFan> Fans;
        const float MatchTolerance = FMath::Max(0.5f, CarvingSettings.Drainage.AnalysisCellSizeMeters * 0.35f);
        const float MaximumRadius = FMath::Max(Settings.MinimumFanRadiusMeters, Settings.MaximumFanRadiusMeters);

        for (const FCubusTerrainDrainageSegment& Incoming : Segments)
        {
            if (Incoming.bLakeTraversal || !SegmentTouchesBounds(Incoming, TileBounds, MaximumRadius))
            {
                continue;
            }

            const float IncomingSlope = SegmentSlope(Incoming);
            if (IncomingSlope < Settings.FanMinimumIncomingSlope)
            {
                continue;
            }

            const FCubusTerrainDrainageSegment* Downstream = FindDownstreamSegment(
                Incoming,
                Segments,
                MatchTolerance
            );
            if (Downstream == nullptr)
            {
                continue;
            }

            const float DownstreamSlope = SegmentSlope(*Downstream);
            const float SlopeDrop = IncomingSlope - DownstreamSlope;
            if (DownstreamSlope > Settings.FanMaximumDownstreamSlope ||
                SlopeDrop < Settings.FanMinimumSlopeDrop)
            {
                continue;
            }

            const FVector2D DownstreamVector = Downstream->EndMeters - Downstream->StartMeters;
            const double DownstreamLength = FMath::Sqrt(DownstreamVector.SquaredLength());
            if (DownstreamLength <= UE_SMALL_NUMBER)
            {
                continue;
            }

            const float Strength = AreaStrength(Incoming, CarvingSettings);
            const float BreakStrength = FMath::Clamp(
                SlopeDrop / FMath::Max(Settings.FanMinimumSlopeDrop * 3.0f, 0.001f),
                0.0f,
                1.0f
            );
            const float FanStrength = FMath::Clamp(0.35f + Strength * 0.45f + BreakStrength * 0.20f, 0.0f, 1.0f);

            FFan Fan;
            Fan.ApexMeters = Incoming.EndMeters;
            Fan.Direction = DownstreamVector / DownstreamLength;
            Fan.ApexElevationMeters = Incoming.EndElevationMeters;
            Fan.RadiusMeters = FMath::Lerp(
                FMath::Max(8.0f, Settings.MinimumFanRadiusMeters),
                MaximumRadius,
                FanStrength
            );
            Fan.ThicknessMeters = FMath::Max(0.0f, Settings.MaximumFanThicknessMeters) * FanStrength;
            Fan.SurfaceGrade = FMath::Clamp(DownstreamSlope * 0.75f, 0.004f, 0.035f);
            Fans.Add(Fan);
        }

        return Fans;
    }

    void ApplyHydrologicDeposition(
        FCubusTerrainRasterTile& Result,
        const FCubusTerrainDepositionSettings& Settings,
        const FCubusTerrainCarvingSettings& CarvingSettings,
        const TArray<FCubusTerrainDrainageSegment>& Segments
    )
    {
        const FVector2D TileMinimum = Result.GetWorldMinimumMeters();
        const FVector2D TileMaximum = Result.GetWorldMaximumMeters();
        const FBox2D TileBounds(TileMinimum, TileMaximum);
        const float MaximumFloodplainWidth = FMath::Max(
            CarvingSettings.HeadwaterFloodplainHalfWidthMeters,
            CarvingSettings.MajorFloodplainHalfWidthMeters
        );
        const float Expansion = FMath::Max(
            MaximumFloodplainWidth,
            FMath::Max(Settings.MinimumFanRadiusMeters, Settings.MaximumFanRadiusMeters)
        ) + CarvingSettings.Drainage.AnalysisCellSizeMeters * 2.0f;

        TArray<const FCubusTerrainDrainageSegment*> NearbySegments;
        for (const FCubusTerrainDrainageSegment& Segment : Segments)
        {
            if (!Segment.bLakeTraversal && SegmentTouchesBounds(Segment, TileBounds, Expansion))
            {
                NearbySegments.Add(&Segment);
            }
        }

        const TArray<FFan> Fans = BuildFans(TileBounds, Settings, CarvingSettings, Segments);
        const float FanCosine = FMath::Cos(FMath::DegreesToRadians(
            FMath::Clamp(Settings.FanHalfAngleDegrees, 10.0f, 85.0f)
        ));

        for (int32 StorageY = 0; StorageY < Result.StorageSampleCount; ++StorageY)
        {
            const int32 GridY = StorageY - Result.HaloSamples;
            const double WorldY = TileMinimum.Y + static_cast<double>(GridY) * Result.SampleSpacingMeters;

            for (int32 StorageX = 0; StorageX < Result.StorageSampleCount; ++StorageX)
            {
                const int32 GridX = StorageX - Result.HaloSamples;
                const double WorldX = TileMinimum.X + static_cast<double>(GridX) * Result.SampleSpacingMeters;
                const FVector2D Point(WorldX, WorldY);
                const int32 Index = StorageY * Result.StorageSampleCount + StorageX;
                const float ExistingHeight = Result.HeightMeters[Index];
                float DepositedHeight = ExistingHeight;

                for (const FCubusTerrainDrainageSegment* Segment : NearbySegments)
                {
                    const float Strength = AreaStrength(*Segment, CarvingSettings);
                    const float FloodplainWidth = FMath::Lerp(
                        CarvingSettings.HeadwaterFloodplainHalfWidthMeters,
                        CarvingSettings.MajorFloodplainHalfWidthMeters,
                        Strength
                    );
                    const float ChannelWidth = FMath::Min(
                        FloodplainWidth * 0.55f,
                        FMath::Lerp(
                            CarvingSettings.HeadwaterChannelHalfWidthMeters,
                            CarvingSettings.MajorChannelHalfWidthMeters,
                            Strength
                        )
                    );

                    float Alpha = 0.0f;
                    const float Distance = DistanceToSegment(Point, Segment->StartMeters, Segment->EndMeters, Alpha);
                    if (Distance >= FloodplainWidth)
                    {
                        continue;
                    }

                    const float ChannelExclusion = ChannelWidth * FMath::Max(1.0f, Settings.FloodplainChannelExclusionScale);
                    if (Distance <= ChannelExclusion)
                    {
                        continue;
                    }

                    const float ProfileHeight = FMath::Lerp(
                        Segment->StartElevationMeters,
                        Segment->EndElevationMeters,
                        Alpha
                    );
                    const float FloodplainDepth = FMath::Lerp(
                        CarvingSettings.HeadwaterFloodplainDepthMeters,
                        CarvingSettings.MajorFloodplainDepthMeters,
                        Strength
                    );
                    const float Inner = FMath::Clamp(
                        (Distance - ChannelExclusion) / FMath::Max(1.0f, FloodplainWidth - ChannelExclusion),
                        0.0f,
                        1.0f
                    );
                    const float CrossSectionWeight = 1.0f - Smooth01(Inner);
                    const float Aggradation = FMath::Max(0.0f, Settings.MaximumFloodplainAggradationMeters) *
                        (0.22f + Strength * 0.78f);
                    const float Target = ProfileHeight - FloodplainDepth + Aggradation;
                    const float Raise = FMath::Max(0.0f, Target - DepositedHeight) * CrossSectionWeight;
                    DepositedHeight += Raise;
                }

                for (const FFan& Fan : Fans)
                {
                    const FVector2D Offset = Point - Fan.ApexMeters;
                    const double DistanceSquared = Offset.SquaredLength();
                    if (DistanceSquared <= UE_SMALL_NUMBER || DistanceSquared >= Fan.RadiusMeters * Fan.RadiusMeters)
                    {
                        continue;
                    }

                    const float Distance = static_cast<float>(FMath::Sqrt(DistanceSquared));
                    const FVector2D DirectionToPoint = Offset / static_cast<double>(Distance);
                    const float Forward = static_cast<float>(FVector2D::DotProduct(DirectionToPoint, Fan.Direction));
                    if (Forward < FanCosine)
                    {
                        continue;
                    }

                    const float Radial = 1.0f - Smooth01(Distance / Fan.RadiusMeters);
                    const float Angular = Smooth01(
                        FMath::Clamp((Forward - FanCosine) / FMath::Max(0.001f, 1.0f - FanCosine), 0.0f, 1.0f)
                    );
                    const float Weight = Radial * Angular;
                    const float AlongDistance = FMath::Max(
                        0.0f,
                        static_cast<float>(FVector2D::DotProduct(Offset, Fan.Direction))
                    );
                    const float FanSurface = Fan.ApexElevationMeters - Fan.SurfaceGrade * AlongDistance +
                        Fan.ThicknessMeters * Radial;
                    const float Raise = FMath::Max(0.0f, FanSurface - DepositedHeight) * Weight;
                    DepositedHeight += Raise;
                }

                Result.HeightMeters[Index] = DepositedHeight;
            }
        }
    }

    void ApplyLocalSettling(
        FCubusTerrainRasterTile& Result,
        const FCubusTerrainDepositionSettings& Settings
    )
    {
        const int32 Size = Result.StorageSampleCount;
        const float CellSize = FMath::Max(0.25f, Result.SampleSpacingMeters);
        const float MaxRiseAtSlope = FMath::Tan(FMath::DegreesToRadians(Settings.MaximumDepositionSlopeDegrees)) * CellSize;

        TArray<float> Current = Result.HeightMeters;
        TArray<float> Next = Current;

        for (int32 Iteration = 0; Iteration < Settings.Iterations; ++Iteration)
        {
            const int32 Margin = Settings.Iterations - Iteration;
            const float* CurrentData = Current.GetData();
            float* NextData = Next.GetData();

            for (int32 Y = Margin; Y < Size - Margin; ++Y)
            {
                const int32 Row = Y * Size;
                const int32 PreviousRow = Row - Size;
                const int32 FollowingRow = Row + Size;

                for (int32 X = Margin; X < Size - Margin; ++X)
                {
                    const int32 Index = Row + X;
                    const float Centre = CurrentData[Index];
                    const float Left = CurrentData[Index - 1];
                    const float Right = CurrentData[Index + 1];
                    const float Up = CurrentData[PreviousRow + X];
                    const float Down = CurrentData[FollowingRow + X];
                    const float UL = CurrentData[PreviousRow + X - 1];
                    const float UR = CurrentData[PreviousRow + X + 1];
                    const float DL = CurrentData[FollowingRow + X - 1];
                    const float DR = CurrentData[FollowingRow + X + 1];

                    const float Mean4 = (Left + Right + Up + Down) * 0.25f;
                    const float Mean8 = (Left + Right + Up + Down + UL + UR + DL + DR) * 0.125f;
                    const float Dx = (Right - Left) * 0.5f;
                    const float Dy = (Down - Up) * 0.5f;
                    const float LocalRise = FMath::Sqrt(Dx * Dx + Dy * Dy);
                    const float SlopeAcceptance = 1.0f - FMath::Clamp(
                        LocalRise / FMath::Max(0.001f, MaxRiseAtSlope),
                        0.0f,
                        1.0f
                    );

                    const float Concavity = FMath::Max(0.0f, Mean8 - Centre);
                    const float NeighbourRelief = FMath::Max(
                        FMath::Max(FMath::Abs(Centre - Left), FMath::Abs(Centre - Right)),
                        FMath::Max(FMath::Abs(Centre - Up), FMath::Abs(Centre - Down))
                    );
                    const float SlopeBreak = FMath::Max(0.0f, NeighbourRelief - LocalRise);

                    float Deposit = Concavity * Settings.ConcavityFillStrength * SlopeAcceptance;
                    Deposit += SlopeBreak * Settings.SlopeBreakStrength * SlopeAcceptance;
                    Deposit = FMath::Min(Settings.MaxDepositPerIterationMeters, Deposit);

                    float NewHeight = Centre + Deposit;
                    if (SlopeAcceptance > 0.2f && Mean4 > NewHeight)
                    {
                        const float Diffusion = Settings.FloodplainDiffusion * SlopeAcceptance;
                        NewHeight += (Mean4 - NewHeight) * Diffusion;
                    }

                    const float LocalCeiling = FMath::Max(FMath::Max(Left, Right), FMath::Max(Up, Down));
                    NextData[Index] = FMath::Max(Centre, FMath::Min(NewHeight, LocalCeiling));
                }
            }

            Swap(Current, Next);
        }

        Result.HeightMeters = MoveTemp(Current);
    }
}

FCubusTerrainRasterTile FCubusTerrainDeposition::DepositTile(
    const FCubusTerrainRasterTile& ErodedTile,
    const FCubusTerrainDepositionSettings& InSettings,
    const FCubusTerrainCarvingSettings& CarvingSettings,
    const TArray<FCubusTerrainDrainageSegment>& DrainageSegments
)
{
    using namespace CubusTerrainDeposition;

    if (!ErodedTile.IsValid())
    {
        return ErodedTile;
    }

    FCubusTerrainDepositionSettings Settings = InSettings;
    Settings.Iterations = FMath::Clamp(Settings.Iterations, 1, FMath::Max(1, ErodedTile.HaloSamples - 1));
    Settings.MaximumDepositionSlopeDegrees = FMath::Clamp(Settings.MaximumDepositionSlopeDegrees, 1.0f, 35.0f);
    Settings.ConcavityFillStrength = FMath::Clamp(Settings.ConcavityFillStrength, 0.0f, 0.8f);
    Settings.SlopeBreakStrength = FMath::Clamp(Settings.SlopeBreakStrength, 0.0f, 0.6f);
    Settings.MaxDepositPerIterationMeters = FMath::Max(0.0f, Settings.MaxDepositPerIterationMeters);
    Settings.FloodplainDiffusion = FMath::Clamp(Settings.FloodplainDiffusion, 0.0f, 0.2f);
    Settings.MaximumFloodplainAggradationMeters = FMath::Max(0.0f, Settings.MaximumFloodplainAggradationMeters);
    Settings.FloodplainChannelExclusionScale = FMath::Max(1.0f, Settings.FloodplainChannelExclusionScale);
    Settings.FanMinimumIncomingSlope = FMath::Max(0.0f, Settings.FanMinimumIncomingSlope);
    Settings.FanMaximumDownstreamSlope = FMath::Max(0.0f, Settings.FanMaximumDownstreamSlope);
    Settings.FanMinimumSlopeDrop = FMath::Max(0.0f, Settings.FanMinimumSlopeDrop);
    Settings.MinimumFanRadiusMeters = FMath::Max(8.0f, Settings.MinimumFanRadiusMeters);
    Settings.MaximumFanRadiusMeters = FMath::Max(Settings.MinimumFanRadiusMeters, Settings.MaximumFanRadiusMeters);
    Settings.MaximumFanThicknessMeters = FMath::Max(0.0f, Settings.MaximumFanThicknessMeters);

    FCubusTerrainRasterTile Result = ErodedTile;
    ApplyHydrologicDeposition(Result, Settings, CarvingSettings, DrainageSegments);
    ApplyLocalSettling(Result, Settings);
    return Result;
}
