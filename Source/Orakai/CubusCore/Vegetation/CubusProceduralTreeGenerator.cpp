#include "CubusCore/Vegetation/CubusProceduralTreeGenerator.h"

#include "CubusCore/Data/CubusTreeSpecies.h"

namespace CubusProceduralTree
{
    FLinearColor MakeWindColor(
        const float WindStrength,
        const float HeightMask,
        const float Phase
    )
    {
        return FLinearColor(
            FMath::Clamp(WindStrength, 0.0f, 1.0f),
            FMath::Clamp(HeightMask, 0.0f, 1.0f),
            FMath::Clamp(Phase, 0.0f, 1.0f),
            FMath::Clamp(1.0f - WindStrength, 0.0f, 1.0f)
        );
    }

    FVector SnapVector(const FVector& Value, const float Grid)
    {
        const float SafeGrid = FMath::Max(1.0f, Grid);
        return FVector(
            FMath::GridSnap(Value.X, SafeGrid),
            FMath::GridSnap(Value.Y, SafeGrid),
            FMath::GridSnap(Value.Z, SafeGrid)
        );
    }

    float SnapSize(const float Value, const float Grid)
    {
        return FMath::Max(Grid, FMath::GridSnap(Value, Grid));
    }

    void AddTriangle(
        FCubusTreeMeshSection& Section,
        const int32 A,
        const int32 B,
        const int32 C
    )
    {
        Section.Triangles.Add(A);
        Section.Triangles.Add(B);
        Section.Triangles.Add(C);
    }

    void AddFace(
        FCubusTreeMeshSection& Section,
        const FVector& A,
        const FVector& B,
        const FVector& C,
        const FVector& D,
        const FVector& Normal,
        const FLinearColor& Wind
    )
    {
        const int32 Base = Section.Vertices.Num();
        Section.Vertices.Append({ A, B, C, D });
        Section.Normals.Append({ Normal, Normal, Normal, Normal });
        Section.UV0.Append(
        {
            FVector2D(0.0f, 0.0f),
            FVector2D(1.0f, 0.0f),
            FVector2D(1.0f, 1.0f),
            FVector2D(0.0f, 1.0f)
        });
        Section.VertexColors.Append({ Wind, Wind, Wind, Wind });
        AddTriangle(Section, Base, Base + 1, Base + 2);
        AddTriangle(Section, Base, Base + 2, Base + 3);
    }

    void AddOrientedBox(
        FCubusTreeMeshSection& Section,
        const FVector& Start,
        const FVector& End,
        const float StartHalfWidth,
        const float EndHalfWidth,
        const float StartHeightMask,
        const float EndHeightMask,
        const float WindStrength,
        const float Phase
    )
    {
        const FVector Axis = (End - Start).GetSafeNormal();
        if (Axis.IsNearlyZero())
        {
            return;
        }

        const FVector Reference =
            FMath::Abs(Axis.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
        const FVector Right = FVector::CrossProduct(Reference, Axis).GetSafeNormal();
        const FVector Up = FVector::CrossProduct(Axis, Right).GetSafeNormal();

        const float StartWidth = FMath::Max(2.0f, StartHalfWidth);
        const float EndWidth = FMath::Max(2.0f, EndHalfWidth);

        const FVector S0 = Start - Right * StartWidth - Up * StartWidth;
        const FVector S1 = Start + Right * StartWidth - Up * StartWidth;
        const FVector S2 = Start + Right * StartWidth + Up * StartWidth;
        const FVector S3 = Start - Right * StartWidth + Up * StartWidth;
        const FVector E0 = End - Right * EndWidth - Up * EndWidth;
        const FVector E1 = End + Right * EndWidth - Up * EndWidth;
        const FVector E2 = End + Right * EndWidth + Up * EndWidth;
        const FVector E3 = End - Right * EndWidth + Up * EndWidth;

        const FLinearColor StartWind = MakeWindColor(
            WindStrength * StartHeightMask,
            StartHeightMask,
            Phase
        );
        const FLinearColor EndWind = MakeWindColor(
            WindStrength * EndHeightMask,
            EndHeightMask,
            Phase
        );
        const FLinearColor SideWind = FLinearColor(
            (StartWind.R + EndWind.R) * 0.5f,
            (StartWind.G + EndWind.G) * 0.5f,
            Phase,
            (StartWind.A + EndWind.A) * 0.5f
        );

        AddFace(Section, S0, S1, S2, S3, -Axis, StartWind);
        AddFace(Section, E3, E2, E1, E0, Axis, EndWind);
        AddFace(Section, S1, E1, E2, S2, Right, SideWind);
        AddFace(Section, S3, E3, E0, S0, -Right, SideWind);
        AddFace(Section, S2, E2, E3, S3, Up, SideWind);
        AddFace(Section, S0, E0, E1, S1, -Up, SideWind);
    }

    void AddCuboidCluster(
        FCubusTreeMeshSection& Section,
        const FVector& Center,
        const FVector& HalfExtent,
        const float YawRadians,
        const float WindStrength,
        const float HeightMask,
        const float Phase
    )
    {
        const float C = FMath::Cos(YawRadians);
        const float S = FMath::Sin(YawRadians);
        const FVector Right(C, S, 0.0f);
        const FVector Forward(-S, C, 0.0f);
        const FVector Up = FVector::UpVector;

        const FVector X = Right * HalfExtent.X;
        const FVector Y = Forward * HalfExtent.Y;
        const FVector Z = Up * HalfExtent.Z;

        const FVector P000 = Center - X - Y - Z;
        const FVector P100 = Center + X - Y - Z;
        const FVector P110 = Center + X + Y - Z;
        const FVector P010 = Center - X + Y - Z;
        const FVector P001 = Center - X - Y + Z;
        const FVector P101 = Center + X - Y + Z;
        const FVector P111 = Center + X + Y + Z;
        const FVector P011 = Center - X + Y + Z;
        const FLinearColor Wind = MakeWindColor(WindStrength, HeightMask, Phase);

        AddFace(Section, P100, P101, P111, P110, Right, Wind);
        AddFace(Section, P010, P011, P001, P000, -Right, Wind);
        AddFace(Section, P110, P111, P011, P010, Forward, Wind);
        AddFace(Section, P000, P001, P101, P100, -Forward, Wind);
        AddFace(Section, P101, P001, P011, P111, Up, Wind);
        AddFace(Section, P000, P100, P110, P010, -Up, Wind);
    }

    FVector RandomScale(
        FRandomStream& Random,
        const FVector& Minimum,
        const FVector& Maximum,
        const float Grid
    )
    {
        return FVector(
            SnapSize(Random.FRandRange(Minimum.X, Maximum.X), Grid),
            SnapSize(Random.FRandRange(Minimum.Y, Maximum.Y), Grid),
            SnapSize(Random.FRandRange(Minimum.Z, Maximum.Z), Grid)
        );
    }

    void AccumulateBounds(
        const FCubusTreeMeshSection& Section,
        FBox& Bounds
    )
    {
        for (const FVector& Vertex : Section.Vertices)
        {
            Bounds += Vertex;
        }
    }
}

void FCubusTreeMeshSection::Reset()
{
    Vertices.Reset();
    Triangles.Reset();
    Normals.Reset();
    UV0.Reset();
    VertexColors.Reset();
}

bool FCubusTreeMeshSection::IsValid() const
{
    return Vertices.Num() >= 3 &&
        Triangles.Num() >= 3 &&
        Triangles.Num() % 3 == 0 &&
        Normals.Num() == Vertices.Num() &&
        UV0.Num() == Vertices.Num() &&
        VertexColors.Num() == Vertices.Num();
}

void FCubusGeneratedTreeMesh::Reset()
{
    Bark.Reset();
    Canopy.Reset();
    Bounds = FBox(EForceInit::ForceInit);
    GeneratedHeight = 0.0f;
    Seed = 0;
}

bool FCubusGeneratedTreeMesh::IsValid() const
{
    return Bark.IsValid() && Bounds.IsValid;
}

bool FCubusProceduralTreeGenerator::BuildTree(
    const UCubusTreeSpecies& Species,
    const int32 Seed,
    FCubusGeneratedTreeMesh& OutMesh
)
{
    using namespace CubusProceduralTree;

    OutMesh.Reset();
    OutMesh.Seed = Seed;

    FRandomStream Random(Seed);
    const float BlockGrid = FMath::Max(10.0f, Species.BaseRadius * 0.5f);
    const float MinimumHeight = FMath::Max(100.0f, Species.MinimumHeight);
    const float MaximumHeight = FMath::Max(MinimumHeight, Species.MaximumHeight);
    const float Height = SnapSize(
        Random.FRandRange(MinimumHeight, MaximumHeight),
        BlockGrid
    );
    const int32 TrunkSegments = FMath::Clamp(Species.TrunkSegments, 3, 12);
    const float Phase = Random.FRand();
    const FVector BendDirection = FVector(
        Random.FRandRange(-1.0f, 1.0f),
        Random.FRandRange(-1.0f, 1.0f),
        0.0f
    ).GetSafeNormal();

    TArray<FVector> TrunkPoints;
    TrunkPoints.Reserve(TrunkSegments + 1);

    for (int32 Segment = 0; Segment <= TrunkSegments; ++Segment)
    {
        const float Alpha = static_cast<float>(Segment) /
            static_cast<float>(TrunkSegments);
        const FVector RawPoint =
            FVector(0.0f, 0.0f, Height * Alpha) +
            BendDirection * FMath::Square(Alpha) * Species.TrunkBend;
        TrunkPoints.Add(SnapVector(RawPoint, BlockGrid));
    }

    for (int32 Segment = 0; Segment < TrunkSegments; ++Segment)
    {
        const float A0 = static_cast<float>(Segment) /
            static_cast<float>(TrunkSegments);
        const float A1 = static_cast<float>(Segment + 1) /
            static_cast<float>(TrunkSegments);

        // Deliberately stepped instead of continuously tapered.
        const int32 TaperStep0 = FMath::FloorToInt(A0 * 4.0f);
        const int32 TaperStep1 = FMath::FloorToInt(A1 * 4.0f);
        const float StepAlpha0 = static_cast<float>(TaperStep0) / 4.0f;
        const float StepAlpha1 = static_cast<float>(TaperStep1) / 4.0f;
        const float Width0 = SnapSize(
            FMath::Lerp(
                Species.BaseRadius,
                Species.BaseRadius * Species.TopRadiusRatio,
                StepAlpha0
            ),
            BlockGrid * 0.5f
        );
        const float Width1 = SnapSize(
            FMath::Lerp(
                Species.BaseRadius,
                Species.BaseRadius * Species.TopRadiusRatio,
                StepAlpha1
            ),
            BlockGrid * 0.5f
        );

        AddOrientedBox(
            OutMesh.Bark,
            TrunkPoints[Segment],
            TrunkPoints[Segment + 1],
            Width0,
            Width1,
            A0,
            A1,
            Species.TrunkWindStrength,
            Phase
        );
    }

    const int32 MinimumBranches = FMath::Max(0, Species.MinimumBranches);
    const int32 MaximumBranches = FMath::Max(MinimumBranches, Species.MaximumBranches);
    const int32 BranchCount = Random.RandRange(MinimumBranches, MaximumBranches);
    TArray<FVector> BranchTips;
    BranchTips.Reserve(BranchCount);

    for (int32 BranchIndex = 0; BranchIndex < BranchCount; ++BranchIndex)
    {
        const float BranchAlpha = Random.FRandRange(
            FMath::Clamp(Species.BranchStartHeightRatio, 0.0f, 0.9f),
            0.88f
        );
        const int32 SegmentIndex = FMath::Clamp(
            FMath::FloorToInt(BranchAlpha * TrunkSegments),
            0,
            TrunkSegments - 1
        );
        const FVector Start = TrunkPoints[SegmentIndex];
        const int32 DirectionIndex = Random.RandRange(0, 7);
        const float Angle = static_cast<float>(DirectionIndex) * UE_PI / 4.0f;
        const FVector Horizontal(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        const FVector Direction = (
            Horizontal * (1.0f - Species.BranchUpwardBias) +
            FVector::UpVector * Species.BranchUpwardBias
        ).GetSafeNormal();
        const float Length = SnapSize(
            Random.FRandRange(
                FMath::Max(20.0f, Species.MinimumBranchLength),
                FMath::Max(Species.MinimumBranchLength, Species.MaximumBranchLength)
            ),
            BlockGrid
        );
        const FVector Tip = SnapVector(Start + Direction * Length, BlockGrid);
        const float Width = SnapSize(
            Species.BaseRadius * Species.BranchRadiusRatio,
            BlockGrid * 0.5f
        );

        AddOrientedBox(
            OutMesh.Bark,
            Start,
            Tip,
            Width,
            FMath::Max(BlockGrid * 0.5f, Width * 0.55f),
            BranchAlpha,
            1.0f,
            Species.BranchWindStrength,
            FMath::Frac(Phase + BranchIndex * 0.173f)
        );
        BranchTips.Add(Tip);
    }

    if (Species.CanopyShape != ECubusTreeCanopyShape::Dead)
    {
        const int32 MinimumClusters = FMath::Max(0, Species.MinimumCanopyClusters);
        const int32 MaximumClusters = FMath::Max(MinimumClusters, Species.MaximumCanopyClusters);
        const int32 ClusterCount = Random.RandRange(MinimumClusters, MaximumClusters);
        const FVector CrownCenter = SnapVector(
            TrunkPoints.Last() - FVector(0.0f, 0.0f, Height * 0.12f),
            BlockGrid
        );

        for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
        {
            FVector Center = CrownCenter;
            FVector Extent = RandomScale(
                Random,
                Species.MinimumCanopyScale * 0.5f,
                Species.MaximumCanopyScale * 0.5f,
                BlockGrid
            );

            if (Species.CanopyShape == ECubusTreeCanopyShape::LayeredConifer)
            {
                const float LayerAlpha = ClusterCount > 1
                    ? static_cast<float>(ClusterIndex) /
                        static_cast<float>(ClusterCount - 1)
                    : 0.0f;
                Center.Z = SnapSize(
                    Height * FMath::Lerp(0.38f, 0.96f, LayerAlpha),
                    BlockGrid
                );
                Center.X += FMath::GridSnap(
                    Random.FRandRange(-BlockGrid, BlockGrid),
                    BlockGrid
                );
                Center.Y += FMath::GridSnap(
                    Random.FRandRange(-BlockGrid, BlockGrid),
                    BlockGrid
                );
                Extent.X *= FMath::Lerp(1.5f, 0.35f, LayerAlpha);
                Extent.Y *= FMath::Lerp(1.5f, 0.35f, LayerAlpha);
                Extent.Z = FMath::Max(BlockGrid, Extent.Z * 0.45f);
            }
            else
            {
                const FVector Offset(
                    Random.FRandRange(-Extent.X * 0.8f, Extent.X * 0.8f),
                    Random.FRandRange(-Extent.Y * 0.8f, Extent.Y * 0.8f),
                    Random.FRandRange(-Extent.Z * 0.35f, Extent.Z * 0.65f)
                );
                Center = SnapVector(Center + Offset, BlockGrid);

                if (BranchTips.Num() > 0 && Random.FRand() < 0.6f)
                {
                    Center = SnapVector(
                        FMath::Lerp(
                            Center,
                            BranchTips[Random.RandRange(0, BranchTips.Num() - 1)],
                            0.55f
                        ),
                        BlockGrid
                    );
                }

                if (Species.CanopyShape == ECubusTreeCanopyShape::Sparse)
                {
                    Extent *= 0.65f;
                }
            }

            const int32 RotationStep = Random.RandRange(0, 3);
            const float Yaw = static_cast<float>(RotationStep) * UE_PI * 0.5f;
            AddCuboidCluster(
                OutMesh.Canopy,
                Center,
                Extent,
                Yaw,
                Species.CanopyWindStrength,
                FMath::Clamp(Center.Z / Height, 0.0f, 1.0f),
                FMath::Frac(Phase + ClusterIndex * 0.117f)
            );
        }
    }

    AccumulateBounds(OutMesh.Bark, OutMesh.Bounds);
    AccumulateBounds(OutMesh.Canopy, OutMesh.Bounds);
    OutMesh.GeneratedHeight = Height;

    return OutMesh.IsValid();
}
