#pragma once

#include "CoreMinimal.h"

enum class ECubusDensityFace : uint8
{
    NegativeX = 0,
    PositiveX,
    NegativeY,
    PositiveY,
    NegativeZ,
    PositiveZ,
    Count
};

/**
 * Snapshot of the target density resolution on all six neighbouring faces.
 * A transition cell is owned by the coarser chunk whenever a face neighbour
 * has exactly twice its subdivision count.
 */
struct ORAKAI_API FCubusDensityTransitionFaces
{
    int32 NeighbourSubdivisions[6] = { 0, 0, 0, 0, 0, 0 };

    int32 Get(const ECubusDensityFace Face) const
    {
        return NeighbourSubdivisions[static_cast<int32>(Face)];
    }

    void Set(const ECubusDensityFace Face, const int32 Subdivisions)
    {
        NeighbourSubdivisions[static_cast<int32>(Face)] = Subdivisions;
    }

    bool HasFinerNeighbour(const ECubusDensityFace Face, const int32 SelfSubdivisions) const
    {
        return Get(Face) == SelfSubdivisions * 2;
    }

    uint32 GetSignature(const int32 SelfSubdivisions) const
    {
        uint32 Hash = 2166136261u;
        auto Mix = [&Hash](const uint32 Value)
        {
            Hash ^= Value;
            Hash *= 16777619u;
        };

        Mix(static_cast<uint32>(SelfSubdivisions));
        for (const int32 Value : NeighbourSubdivisions)
        {
            Mix(static_cast<uint32>(Value));
        }
        return Hash;
    }

    static FIntVector GetOffset(const ECubusDensityFace Face)
    {
        switch (Face)
        {
        case ECubusDensityFace::NegativeX: return FIntVector(-1, 0, 0);
        case ECubusDensityFace::PositiveX: return FIntVector(1, 0, 0);
        case ECubusDensityFace::NegativeY: return FIntVector(0, -1, 0);
        case ECubusDensityFace::PositiveY: return FIntVector(0, 1, 0);
        case ECubusDensityFace::NegativeZ: return FIntVector(0, 0, -1);
        case ECubusDensityFace::PositiveZ: return FIntVector(0, 0, 1);
        default: return FIntVector::ZeroValue;
        }
    }
};

/** Half-open aligned XY bounds used by the density clipmap hierarchy. */
struct ORAKAI_API FCubusDensityTileBounds2D
{
    FIntPoint Min = FIntPoint::ZeroValue;
    FIntPoint MaxExclusive = FIntPoint::ZeroValue;

    bool IsValid() const
    {
        return MaxExclusive.X > Min.X && MaxExclusive.Y > Min.Y;
    }

    int32 WidthX() const { return MaxExclusive.X - Min.X; }
    int32 WidthY() const { return MaxExclusive.Y - Min.Y; }

    bool Contains(const int32 X, const int32 Y) const
    {
        return X >= Min.X && X < MaxExclusive.X &&
               Y >= Min.Y && Y < MaxExclusive.Y;
    }

    bool operator==(const FCubusDensityTileBounds2D& Other) const
    {
        return Min == Other.Min && MaxExclusive == Other.MaxExclusive;
    }

    bool operator!=(const FCubusDensityTileBounds2D& Other) const
    {
        return !(*this == Other);
    }
};

/**
 * Density-LOD scale rules shared by streaming, chunks and meshing.
 *
 * The procedural scalar field remains canonical and resolution-independent.
 * Subdivision changes only how densely a chunk samples that field for its
 * Marching Cubes mesh; generated terrain is never persisted at the finer
 * resolution. Player density edits remain a separate sparse field and are
 * evaluated continuously by FCubusDensityEditField.
 *
 * Runtime refinement is deliberately capped at 4 subdivisions per canonical
 * voxel. With an 80 cm canonical voxel this gives 20 cm near-field samples,
 * 40 cm transition samples, and 80 cm ordinary samples. BuildAdaptiveChunk()
 * rejects coarse cells that cannot contain a surface before marching the fine
 * lattice, avoiding the old cubic full-volume refinement cost.
 */
class ORAKAI_API FCubusDensityLod
{
public:
    static constexpr int32 MaximumRuntimeSubdivisions = 4;

    static int32 NormalizeSubdivisions(
        const int32 RequestedSubdivisions
    )
    {
        const int32 SafeRequested = FMath::Clamp(
            RequestedSubdivisions,
            1,
            MaximumRuntimeSubdivisions
        );

        // Keep neighbouring fine lattices on power-of-two sample intervals.
        // This gives stable 1x / 2x / 4x tiers and avoids arbitrary fractional
        // lattice relationships between streamed chunks.
        if (SafeRequested <= 1)
        {
            return 1;
        }

        if (SafeRequested <= 2)
        {
            return 2;
        }

        return 4;
    }

    static int32 ResolveSubdivisionsForSpacing(
        const float CanonicalVoxelSize,
        const float TargetSampleSpacing
    )
    {
        const float SafeVoxelSize = FMath::Max(1.0f, CanonicalVoxelSize);
        const float SafeTargetSpacing = FMath::Clamp(
            TargetSampleSpacing,
            1.0f,
            SafeVoxelSize
        );

        const int32 RequiredSubdivisions = FMath::CeilToInt(
            SafeVoxelSize / SafeTargetSpacing
        );

        return NormalizeSubdivisions(RequiredSubdivisions);
    }

    static float GetSampleSpacing(
        const float CanonicalVoxelSize,
        const int32 SubdivisionsPerVoxel
    )
    {
        return FMath::Max(1.0f, CanonicalVoxelSize) /
            static_cast<float>(NormalizeSubdivisions(SubdivisionsPerVoxel));
    }

    static int32 ChunkDistance(
        const FIntVector& A,
        const FIntVector& B
    )
    {
        const FIntVector Delta = A - B;
        return FMath::Max3(
            FMath::Abs(Delta.X),
            FMath::Abs(Delta.Y),
            FMath::Abs(Delta.Z)
        );
    }

    static int32 HorizontalChunkDistance(
        const FIntVector& A,
        const FIntVector& B
    )
    {
        return FMath::Max(
            FMath::Abs(A.X - B.X),
            FMath::Abs(A.Y - B.Y)
        );
    }

    static int32 FloorDivide(const int32 Value, const int32 Divisor)
    {
        check(Divisor > 0);
        int32 Quotient = Value / Divisor;
        const int32 Remainder = Value % Divisor;
        if (Remainder < 0)
        {
            --Quotient;
        }
        return Quotient;
    }

    static int32 AlignDown(const int32 Value, const int32 Alignment)
    {
        const int32 SafeAlignment = FMath::Max(1, Alignment);
        return FloorDivide(Value, SafeAlignment) * SafeAlignment;
    }

    static FCubusDensityTileBounds2D BuildAlignedCoverage(
        const FIntPoint& Centre,
        const int32 HalfSpan,
        const int32 Alignment = 2
    )
    {
        const int32 SafeHalfSpan = FMath::Max(1, HalfSpan);
        const int32 SafeAlignment = FMath::Max(1, Alignment);
        const int32 Width = SafeHalfSpan * 2;

        FCubusDensityTileBounds2D Result;
        Result.Min.X = AlignDown(Centre.X - SafeHalfSpan, SafeAlignment);
        Result.Min.Y = AlignDown(Centre.Y - SafeHalfSpan, SafeAlignment);
        Result.MaxExclusive = Result.Min + FIntPoint(Width, Width);

        if (!Result.Contains(Centre.X, Centre.Y))
        {
            if (Centre.X < Result.Min.X)
            {
                Result.Min.X -= SafeAlignment;
            }
            else if (Centre.X >= Result.MaxExclusive.X)
            {
                Result.Min.X += SafeAlignment;
            }

            if (Centre.Y < Result.Min.Y)
            {
                Result.Min.Y -= SafeAlignment;
            }
            else if (Centre.Y >= Result.MaxExclusive.Y)
            {
                Result.Min.Y += SafeAlignment;
            }
            Result.MaxExclusive = Result.Min + FIntPoint(Width, Width);
        }

        return Result;
    }

    static FCubusDensityTileBounds2D ScaleDownExact(
        const FCubusDensityTileBounds2D& Bounds,
        const int32 Factor = 2
    )
    {
        const int32 SafeFactor = FMath::Max(1, Factor);
        ensureMsgf(
            Bounds.Min.X % SafeFactor == 0 && Bounds.Min.Y % SafeFactor == 0 &&
            Bounds.MaxExclusive.X % SafeFactor == 0 && Bounds.MaxExclusive.Y % SafeFactor == 0,
            TEXT("Cubus density clipmap bounds must be exactly aligned before scaling")
        );

        FCubusDensityTileBounds2D Result;
        Result.Min = FIntPoint(
            FloorDivide(Bounds.Min.X, SafeFactor),
            FloorDivide(Bounds.Min.Y, SafeFactor)
        );
        Result.MaxExclusive = FIntPoint(
            FloorDivide(Bounds.MaxExclusive.X, SafeFactor),
            FloorDivide(Bounds.MaxExclusive.Y, SafeFactor)
        );
        return Result;
    }

    static FCubusDensityTileBounds2D BuildAlignedOuterBounds(
        const FCubusDensityTileBounds2D& InnerBounds,
        const int32 HalfSpan,
        const int32 Alignment = 2
    )
    {
        check(InnerBounds.IsValid());
        const int32 SafeHalfSpan = FMath::Max(2, HalfSpan);
        const int32 SafeAlignment = FMath::Max(1, Alignment);
        const int32 Width = SafeHalfSpan * 2;
        check(Width >= InnerBounds.WidthX() && Width >= InnerBounds.WidthY());

        auto ResolveAxis = [Width, SafeAlignment](const int32 InnerMin, const int32 InnerMax)
        {
            const int32 TwiceCentre = InnerMin + InnerMax;
            int32 Minimum = AlignDown((TwiceCentre - Width) / 2, SafeAlignment);
            while (Minimum > InnerMin)
            {
                Minimum -= SafeAlignment;
            }
            while (Minimum + Width < InnerMax)
            {
                Minimum += SafeAlignment;
            }
            return Minimum;
        };

        FCubusDensityTileBounds2D Result;
        Result.Min.X = ResolveAxis(InnerBounds.Min.X, InnerBounds.MaxExclusive.X);
        Result.Min.Y = ResolveAxis(InnerBounds.Min.Y, InnerBounds.MaxExclusive.Y);
        Result.MaxExclusive = Result.Min + FIntPoint(Width, Width);
        return Result;
    }
};
