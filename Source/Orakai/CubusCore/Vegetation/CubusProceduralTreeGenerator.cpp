#include "CubusCore/Vegetation/CubusProceduralTreeGenerator.h"

#include "CubusCore/Data/CubusTreeSpecies.h"

namespace CubusProceduralTree
{
    FVector SafePerpendicular(const FVector& Direction)
    {
        const FVector Reference =
            FMath::Abs(Direction.Z) < 0.9f
                ? FVector::UpVector
                : FVector::ForwardVector;
        return FVector::CrossProduct(Reference, Direction).GetSafeNormal();
    }

    FLinearColor MakeWindColor(
        const float WindStrength,
        const float HeightMask,
        const float Phase,
        const float Stiffness
    )
    {
        return FLinearColor(
            FMath::Clamp(WindStrength, 0.0f, 1.0f),
            FMath::Clamp(HeightMask, 0.0f, 1.0f),
            FMath::Clamp(Phase, 0.0f, 1.0f),
            FMath::Clamp(Stiffness, 0.0f, 1.0f)
        );
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

    void AddFrustum(
        FCubusTreeMeshSection& Section,
        const FVector& Start,
        const FVector& End,
        const float StartRadius,
        const float EndRadius,
        const int32 SideCount,
        const float StartHeightMask,
        const float EndHeightMask,
        const float WindStrength,
        const float Phase,
        const bool bCapStart,
        const bool bCapEnd
    )
    {
        const FVector Axis = (End - Start).GetSafeNormal();
        if (Axis.IsNearlyZero())
        {
            return;
        }

        const FVector BasisX = SafePerpendicular(Axis);
        const FVector BasisY = FVector::CrossProduct(Axis, BasisX).GetSafeNormal();
        const int32 Sides = FMath::Clamp(SideCount, 3, 12);
        const int32 BaseVertex = Section.Vertices.Num();
        const float Length = FVector::Distance(Start, End);

        for (int32 Ring = 0; Ring < 2; ++Ring)
        {
            const FVector Center = Ring == 0 ? Start : End;
            const float Radius = Ring == 0 ? StartRadius : EndRadius;
            const float HeightMask = Ring == 0 ? StartHeightMask : EndHeightMask;

            for (int32 Side = 0; Side < Sides; ++Side)
            {
                const float Alpha = static_cast<float>(Side) / static_cast<float>(Sides);
                const float Angle = Alpha * UE_TWO_PI;
                const FVector Radial =
                    BasisX * FMath::Cos(Angle) + BasisY * FMath::Sin(Angle);

                Section.Vertices.Add(Center + Radial * Radius);
                Section.Normals.Add(Radial);
                Section.UV0.Add(FVector2D(Alpha, Ring == 0 ? 0.0f : Length / 100.0f));
                Section.VertexColors.Add(
                    MakeWindColor(
                        WindStrength,
                        HeightMask,
                        Phase,
                        1.0f - WindStrength
                    )
                );
            }
        }

        for (int32 Side = 0; Side < Sides; ++Side)
        {
            const int32 Next = (Side + 1) % Sides;
            const int32 A = BaseVertex + Side;
            const int32 B = BaseVertex + Next;
            const int32 C = BaseVertex + Sides + Side;
            const int32 D = BaseVertex + Sides + Next;
            AddTriangle(Section, A, D, C);
            AddTriangle(Section, A, B, D);
        }

        if (bCapStart)
        {
            const int32 CenterIndex = Section.Vertices.Add(Start);
            Section.Normals.Add(-Axis);
            Section.UV0.Add(FVector2D(0.5f, 0.5f));
            Section.VertexColors.Add(
                MakeWindColor(WindStrength, StartHeightMask, Phase, 1.0f - WindStrength)
            );

            for (int32 Side = 0; Side < Sides; ++Side)
            {
                const int32 Next = (Side + 1) % Sides;
                AddTriangle(Section, CenterIndex, BaseVertex + Next, BaseVertex + Side);
            }
        }

        if (bCapEnd)
        {
            const int32 CenterIndex = Section.Vertices.Add(End);
            Section.Normals.Add(Axis);
            Section.UV0.Add(FVector2D(0.5f, 0.5f));
            Section.VertexColors.Add(
                MakeWindColor(WindStrength, EndHeightMask, Phase, 1.0f - WindStrength)
            );
            const int32 EndRing = BaseVertex + Sides;

            for (int32 Side = 0; Side < Sides; ++Side)
            {
                const int32 Next = (Side + 1) % Sides;
                AddTriangle(Section, CenterIndex, EndRing + Side, EndRing + Next);
            }
        }
    }

    void AddOctahedronCluster(
        FCubusTreeMeshSection& Section,
        const FVector& Center,
        const FVector& Scale,
        const FLinearColor& Tint,
        const float WindStrength,
        const float HeightMask,
        const float Phase
    )
    {
        const int32 Base = Section.Vertices.Num();
        const FVector LocalVertices[6] =
        {
            FVector( Scale.X, 0.0, 0.0),
            FVector(-Scale.X, 0.0, 0.0),
            FVector(0.0,  Scale.Y, 0.0),
            FVector(0.0, -Scale.Y, 0.0),
            FVector(0.0, 0.0,  Scale.Z),
            FVector(0.0, 0.0, -Scale.Z)
        };

        for (const FVector& Local : LocalVertices)
        {
            Section.Vertices.Add(Center + Local);
            Section.Normals.Add(Local.GetSafeNormal());
            Section.UV0.Add(FVector2D(Local.X >= 0.0 ? 1.0 : 0.0, Local.Z >= 0.0 ? 1.0 : 0.0));
            FLinearColor Wind = MakeWindColor(
                WindStrength,
                HeightMask,
                Phase,
                1.0f - WindStrength
            );
            Wind *= Tint;
            Wind.A = 1.0f - WindStrength;
            Section.VertexColors.Add(Wind);
        }

        const int32 Faces[8][3] =
        {
            {4, 0, 2}, {4, 2, 1}, {4, 1, 3}, {4, 3, 0},
            {5, 2, 0}, {5, 1, 2}, {5, 3, 1}, {5, 0, 3}
        };

        for (const auto& Face : Faces)
        {
            AddTriangle(Section, Base + Face[0], Base + Face[1], Base + Face[2]);
        }
    }

    FVector RandomScale(
        FRandomStream& Random,
        const FVector& Minimum,
        const FVector& Maximum
    )
    {
        return FVector(
            Random.FRandRange(Minimum.X, Maximum.X),
            Random.FRandRange(Minimum.Y, Maximum.Y),
            Random.FRandRange(Minimum.Z, Maximum.Z)
        );
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
    const float MinimumHeight = FMath::Max(100.0f, Species.MinimumHeight);
    const float MaximumHeight = FMath::Max(MinimumHeight, Species.MaximumHeight);
    const float Height = Random.FRandRange(MinimumHeight, MaximumHeight);
    const int32 TrunkSegments = FMath::Clamp(Species.TrunkSegments, 2, 16);
    const int32 TrunkSides = FMath::Clamp(Species.TrunkSides, 3, 12);
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
        const float Alpha =
            static_cast<float>(Segment) / static_cast<float>(TrunkSegments);
        const float Bend = FMath::Square(Alpha) * Species.TrunkBend;
        const float Twist = FMath::Sin(Alpha * UE_PI * 1.5f + Phase * UE_TWO_PI) *
            Species.TrunkBend * 0.2f;
        const FVector Side = FVector(-BendDirection.Y, BendDirection.X, 0.0f);
        TrunkPoints.Add(
            FVector(0.0, 0.0, Height * Alpha) +
            BendDirection * Bend +
            Side * Twist
        );
    }

    for (int32 Segment = 0; Segment < TrunkSegments; ++Segment)
    {
        const float A0 =
            static_cast<float>(Segment) / static_cast<float>(TrunkSegments);
        const float A1 =
            static_cast<float>(Segment + 1) / static_cast<float>(TrunkSegments);
        const float Radius0 = FMath::Lerp(
            Species.BaseRadius,
            Species.BaseRadius * Species.TopRadiusRatio,
            A0
        );
        const float Radius1 = FMath::Lerp(
            Species.BaseRadius,
            Species.BaseRadius * Species.TopRadiusRatio,
            A1
        );

        AddFrustum(
            OutMesh.Bark,
            TrunkPoints[Segment],
            TrunkPoints[Segment + 1],
            Radius0,
            Radius1,
            TrunkSides,
            A0,
            A1,
            Species.TrunkWindStrength * A1,
            Phase,
            Segment == 0,
            Segment == TrunkSegments - 1
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
            FMath::Clamp(Species.BranchStartHeightRatio, 0.0f, 0.95f),
            0.92f
        );
        const int32 SegmentIndex = FMath::Clamp(
            FMath::FloorToInt(BranchAlpha * TrunkSegments),
            0,
            TrunkSegments - 1
        );
        const float LocalAlpha = BranchAlpha * TrunkSegments - SegmentIndex;
        const FVector Start = FMath::Lerp(
            TrunkPoints[SegmentIndex],
            TrunkPoints[SegmentIndex + 1],
            LocalAlpha
        );
        const float Angle = Random.FRandRange(0.0f, UE_TWO_PI);
        const FVector Horizontal(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
        const FVector Direction = (
            Horizontal * (1.0f - Species.BranchUpwardBias) +
            FVector::UpVector * Species.BranchUpwardBias
        ).GetSafeNormal();
        const float Length = Random.FRandRange(
            FMath::Max(20.0f, Species.MinimumBranchLength),
            FMath::Max(Species.MinimumBranchLength, Species.MaximumBranchLength)
        );
        const FVector Tip = Start + Direction * Length;
        const float StartRadius = Species.BaseRadius * Species.BranchRadiusRatio *
            FMath::Lerp(1.0f, Species.TopRadiusRatio, BranchAlpha);

        AddFrustum(
            OutMesh.Bark,
            Start,
            Tip,
            StartRadius,
            FMath::Max(3.0f, StartRadius * 0.25f),
            FMath::Max(3, TrunkSides - 2),
            BranchAlpha,
            1.0f,
            Species.BranchWindStrength,
            FMath::Frac(Phase + BranchIndex * 0.173f),
            false,
            true
        );
        BranchTips.Add(Tip);
    }

    if (Species.CanopyShape != ECubusTreeCanopyShape::Dead)
    {
        const int32 MinimumClusters = FMath::Max(0, Species.MinimumCanopyClusters);
        const int32 MaximumClusters = FMath::Max(MinimumClusters, Species.MaximumCanopyClusters);
        const int32 ClusterCount = Random.RandRange(MinimumClusters, MaximumClusters);
        const FVector CrownCenter = TrunkPoints.Last() - FVector(0.0, 0.0, Height * 0.12f);

        for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
        {
            FVector Center = CrownCenter;
            FVector Scale = RandomScale(
                Random,
                Species.MinimumCanopyScale,
                Species.MaximumCanopyScale
            );

            if (Species.CanopyShape == ECubusTreeCanopyShape::LayeredConifer)
            {
                const float LayerAlpha = ClusterCount > 1
                    ? static_cast<float>(ClusterIndex) / static_cast<float>(ClusterCount - 1)
                    : 0.0f;
                Center.Z = Height * FMath::Lerp(0.42f, 0.95f, LayerAlpha);
                Scale.X *= FMath::Lerp(1.35f, 0.45f, LayerAlpha);
                Scale.Y *= FMath::Lerp(1.35f, 0.45f, LayerAlpha);
                Scale.Z *= 0.55f;
            }
            else
            {
                const FVector Offset(
                    Random.FRandRange(-Scale.X * 0.65f, Scale.X * 0.65f),
                    Random.FRandRange(-Scale.Y * 0.65f, Scale.Y * 0.65f),
                    Random.FRandRange(-Scale.Z * 0.35f, Scale.Z * 0.55f)
                );
                Center += Offset;

                if (BranchTips.Num() > 0 && Random.FRand() < 0.5f)
                {
                    Center = FMath::Lerp(
                        Center,
                        BranchTips[Random.RandRange(0, BranchTips.Num() - 1)],
                        0.45f
                    );
                }
            }

            if (Species.CanopyShape == ECubusTreeCanopyShape::Sparse)
            {
                Scale *= 0.65f;
            }

            AddOctahedronCluster(
                OutMesh.Canopy,
                Center,
                Scale,
                Species.CanopyTint,
                Species.CanopyWindStrength,
                FMath::Clamp(Center.Z / Height, 0.0f, 1.0f),
                FMath::Frac(Phase + ClusterIndex * 0.217f)
            );
        }
    }

    for (const FVector& Vertex : OutMesh.Bark.Vertices)
    {
        OutMesh.Bounds += Vertex;
    }
    for (const FVector& Vertex : OutMesh.Canopy.Vertices)
    {
        OutMesh.Bounds += Vertex;
    }

    OutMesh.GeneratedHeight = Height;
    return OutMesh.IsValid();
}