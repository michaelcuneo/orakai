#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusDensityField.h"
#include "CubusCore/Meshing/CubusDensityLod.h"
#include "CubusCore/Meshing/CubusMarchingCubesTables.h"

namespace CubusDensityMesher
{
    struct FMaterialBlend
    {
        int32 MaterialIds[4] = { 1, 1, 1, 1 };
        float Weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    };

    struct FInterpolatedVertex
    {
        FVector LocalPosition = FVector::ZeroVector;
        FVector GlobalSamplePosition = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
        int32 MaterialId = 1;
        FMaterialBlend MaterialBlend;
    };

    struct FTriangleMaterialPalette
    {
        int32 MaterialIds[4] = { 1, 1, 1, 1 };
        int32 Count = 1;

        int32 FindSlot(const int32 MaterialId) const
        {
            for (int32 Slot = 0; Slot < Count; ++Slot)
            {
                if (MaterialIds[Slot] == MaterialId)
                {
                    return Slot;
                }
            }

            return INDEX_NONE;
        }
    };

    struct FWeightedMaterial
    {
        int32 MaterialId = 1;
        float Weight = 0.0f;
    };

    const FIntVector CornerOffsets[8] =
    {
        FIntVector(0, 0, 0), FIntVector(1, 0, 0),
        FIntVector(1, 1, 0), FIntVector(0, 1, 0),
        FIntVector(0, 0, 1), FIntVector(1, 0, 1),
        FIntVector(1, 1, 1), FIntVector(0, 1, 1)
    };

    const int32 EdgeCornerIndices[12][2] =
    {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

    FVector ToVector(const FIntVector& Value)
    {
        return FVector(Value.X, Value.Y, Value.Z);
    }

    int32 ClampDensityMaterialId(const int32 MaterialId)
    {
        return FMath::Clamp(
            MaterialId,
            1,
            FCubusDensityMesher::MaximumDensityMaterialId
        );
    }

    void AddWeightedMaterial(
        TArray<FWeightedMaterial>& Materials,
        const int32 MaterialId,
        const float Weight
    )
    {
        if (MaterialId <= 0 || Weight <= UE_SMALL_NUMBER)
        {
            return;
        }

        const int32 ClampedMaterialId = ClampDensityMaterialId(MaterialId);
        for (FWeightedMaterial& Existing : Materials)
        {
            if (Existing.MaterialId == ClampedMaterialId)
            {
                Existing.Weight += Weight;
                return;
            }
        }

        FWeightedMaterial& Added = Materials.AddDefaulted_GetRef();
        Added.MaterialId = ClampedMaterialId;
        Added.Weight = Weight;
    }

    void SortWeightedMaterials(TArray<FWeightedMaterial>& Materials)
    {
        Materials.Sort([](
            const FWeightedMaterial& A,
            const FWeightedMaterial& B
        )
        {
            if (!FMath::IsNearlyEqual(A.Weight, B.Weight))
            {
                return A.Weight > B.Weight;
            }

            return A.MaterialId < B.MaterialId;
        });
    }

    void SetSingleMaterialBlend(
        FMaterialBlend& Blend,
        const int32 MaterialId
    )
    {
        const int32 ClampedMaterialId = ClampDensityMaterialId(MaterialId);
        for (int32 Slot = 0; Slot < 4; ++Slot)
        {
            Blend.MaterialIds[Slot] = ClampedMaterialId;
            Blend.Weights[Slot] = Slot == 0 ? 1.0f : 0.0f;
        }
    }

    FMaterialBlend BuildCellMaterialBlend(
        const FCubusDensitySample (&CornerSamples)[8],
        const FVector& GlobalSamplePosition,
        const FVector& GlobalCellOrigin,
        const float CellSize,
        const float IsoLevel,
        const int32 FallbackMaterialId
    )
    {
        FMaterialBlend Blend;
        SetSingleMaterialBlend(Blend, FallbackMaterialId);

        if (CellSize <= UE_SMALL_NUMBER)
        {
            return Blend;
        }

        const FVector LocalAlpha =
            (GlobalSamplePosition - GlobalCellOrigin) / CellSize;
        const FVector Alpha(
            FMath::Clamp(LocalAlpha.X, 0.0, 1.0),
            FMath::Clamp(LocalAlpha.Y, 0.0, 1.0),
            FMath::Clamp(LocalAlpha.Z, 0.0, 1.0)
        );

        TArray<FWeightedMaterial> Accumulated;
        Accumulated.Reserve(8);

        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            const FCubusDensitySample& Sample = CornerSamples[CornerIndex];
            if (!Sample.IsSolid(IsoLevel) || Sample.MaterialId <= 0)
            {
                continue;
            }

            const FIntVector& Offset = CornerOffsets[CornerIndex];
            const float WeightX = Offset.X == 0
                ? 1.0f - static_cast<float>(Alpha.X)
                : static_cast<float>(Alpha.X);
            const float WeightY = Offset.Y == 0
                ? 1.0f - static_cast<float>(Alpha.Y)
                : static_cast<float>(Alpha.Y);
            const float WeightZ = Offset.Z == 0
                ? 1.0f - static_cast<float>(Alpha.Z)
                : static_cast<float>(Alpha.Z);

            AddWeightedMaterial(
                Accumulated,
                Sample.MaterialId,
                WeightX * WeightY * WeightZ
            );
        }

        if (Accumulated.IsEmpty())
        {
            return Blend;
        }

        SortWeightedMaterials(Accumulated);

        float TotalWeight = 0.0f;
        // Keep blends stable and readable by limiting each vertex to the
        // two strongest terrain materials.
        const int32 BlendCount = FMath::Min(Accumulated.Num(), 2);
        for (int32 Slot = 0; Slot < BlendCount; ++Slot)
        {
            Blend.MaterialIds[Slot] = Accumulated[Slot].MaterialId;
            Blend.Weights[Slot] = Accumulated[Slot].Weight;
            TotalWeight += Accumulated[Slot].Weight;
        }

        for (int32 Slot = BlendCount; Slot < 4; ++Slot)
        {
            Blend.MaterialIds[Slot] = Blend.MaterialIds[0];
            Blend.Weights[Slot] = 0.0f;
        }

        if (TotalWeight <= UE_SMALL_NUMBER)
        {
            SetSingleMaterialBlend(Blend, FallbackMaterialId);
            return Blend;
        }

        for (int32 Slot = 0; Slot < 4; ++Slot)
        {
            Blend.Weights[Slot] /= TotalWeight;
        }

        return Blend;
    }

    FTriangleMaterialPalette BuildPalette(
        const FInterpolatedVertex (&Vertices)[3]
    )
    {
        TArray<int32> UniqueMaterialIds;
        UniqueMaterialIds.Reserve(12);

        for (const FInterpolatedVertex& Vertex : Vertices)
        {
            for (int32 Slot = 0; Slot < 4; ++Slot)
            {
                if (Vertex.MaterialBlend.Weights[Slot] <= UE_SMALL_NUMBER)
                {
                    continue;
                }

                const int32 MaterialId = ClampDensityMaterialId(
                    Vertex.MaterialBlend.MaterialIds[Slot]
                );
                UniqueMaterialIds.AddUnique(MaterialId);
            }
        }

        UniqueMaterialIds.Sort();

        FTriangleMaterialPalette Palette;
        Palette.Count = FMath::Clamp(UniqueMaterialIds.Num(), 1, 4);

        if (UniqueMaterialIds.IsEmpty())
        {
            Palette.MaterialIds[0] = ClampDensityMaterialId(
                Vertices[0].MaterialId
            );
            Palette.Count = 1;
        }
        else
        {
            for (int32 Slot = 0; Slot < Palette.Count; ++Slot)
            {
                Palette.MaterialIds[Slot] =
                    UniqueMaterialIds[Slot];
            }
        }

        for (int32 Slot = Palette.Count; Slot < 4; ++Slot)
        {
            Palette.MaterialIds[Slot] = Palette.MaterialIds[0];
        }

        return Palette;
    }

    FVector2D PackPalette(const FTriangleMaterialPalette& Palette)
    {
        const int32 Base = FCubusDensityMesher::MaterialIdPackingBase;
        return FVector2D(
            static_cast<double>(
                Palette.MaterialIds[0] +
                Palette.MaterialIds[1] * Base
            ),
            static_cast<double>(
                Palette.MaterialIds[2] +
                Palette.MaterialIds[3] * Base
            )
        );
    }

    FLinearColor BuildWeights(
        const FMaterialBlend& Blend,
        const FTriangleMaterialPalette& Palette
    )
    {
        float PaletteWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        for (int32 BlendSlot = 0; BlendSlot < 4; ++BlendSlot)
        {
            const float Weight = Blend.Weights[BlendSlot];
            if (Weight <= UE_SMALL_NUMBER)
            {
                continue;
            }

            const int32 PaletteSlot = Palette.FindSlot(
                Blend.MaterialIds[BlendSlot]
            );
            if (PaletteSlot != INDEX_NONE)
            {
                PaletteWeights[PaletteSlot] += Weight;
            }
        }

        const float TotalWeight =
            PaletteWeights[0] +
            PaletteWeights[1] +
            PaletteWeights[2] +
            PaletteWeights[3];

        if (TotalWeight <= UE_SMALL_NUMBER)
        {
            return FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
        }

        return FLinearColor(
            PaletteWeights[0] / TotalWeight,
            PaletteWeights[1] / TotalWeight,
            PaletteWeights[2] / TotalWeight,
            PaletteWeights[3] / TotalWeight
        );
    }

    FVector ResolveTangentBasis(const FVector& FaceNormal)
    {
        const FVector AbsoluteNormal(
            FMath::Abs(FaceNormal.X),
            FMath::Abs(FaceNormal.Y),
            FMath::Abs(FaceNormal.Z)
        );

        if (AbsoluteNormal.X >= AbsoluteNormal.Y &&
            AbsoluteNormal.X >= AbsoluteNormal.Z)
        {
            return FVector::RightVector;
        }

        return FVector::ForwardVector;
    }

    FInterpolatedVertex InterpolateEdge(
        const FCubusDensitySamplingBuffer& DensityBuffer,
        const FIntVector& LocalSampleA,
        const FIntVector& LocalSampleB,
        const FVector& ChunkMinimum,
        const float VoxelSize,
        const float IsoLevel
    )
    {
        const FCubusDensitySample& SampleA =
            DensityBuffer.GetSampleChecked(LocalSampleA);
        const FCubusDensitySample& SampleB =
            DensityBuffer.GetSampleChecked(LocalSampleB);

        const float DensityDelta = SampleB.Density - SampleA.Density;
        const float Alpha = FMath::IsNearlyZero(DensityDelta)
            ? 0.5f
            : FMath::Clamp(
                (IsoLevel - SampleA.Density) / DensityDelta,
                0.0f,
                1.0f
            );

        const FVector LocalSamplePosition =
            FMath::Lerp(
                ToVector(LocalSampleA),
                ToVector(LocalSampleB),
                Alpha
            ) + DensityBuffer.GetSampleOffsetInVoxels();

        const FVector GlobalSampleOrigin = ToVector(
            DensityBuffer.GetChunkCoordinate() * Cubus::ChunkSize
        );

        const FVector InterpolatedGradient = FMath::Lerp(
            DensityBuffer.GetGradientChecked(LocalSampleA),
            DensityBuffer.GetGradientChecked(LocalSampleB),
            Alpha
        );

        const bool bSampleAIsSolid = SampleA.IsSolid(IsoLevel);

        FInterpolatedVertex Result;
        Result.LocalPosition =
            ChunkMinimum + LocalSamplePosition * VoxelSize;
        Result.GlobalSamplePosition =
            GlobalSampleOrigin + LocalSamplePosition;
        Result.Normal = (-InterpolatedGradient).GetSafeNormal();
        Result.MaterialId = ClampDensityMaterialId(
            bSampleAIsSolid ? SampleA.MaterialId : SampleB.MaterialId
        );
        SetSingleMaterialBlend(Result.MaterialBlend, Result.MaterialId);

        if (Result.Normal.IsNearlyZero())
        {
            const FVector SolidToEmpty = bSampleAIsSolid
                ? ToVector(LocalSampleB - LocalSampleA)
                : ToVector(LocalSampleA - LocalSampleB);
            Result.Normal = SolidToEmpty.GetSafeNormal();
        }

        if (Result.Normal.IsNearlyZero())
        {
            Result.Normal = FVector::UpVector;
        }

        return Result;
    }

    class FAdaptiveSampleCache
    {
    public:
        FAdaptiveSampleCache(
            const ICubusDensityField& InDensityField,
            const FIntVector& InChunkCoordinate,
            const int32 InSubdivisions
        )
            : DensityField(InDensityField)
            , GlobalSampleOrigin(
                ToVector(InChunkCoordinate * Cubus::ChunkSize)
            )
            , SampleSpacing(
                1.0f / static_cast<float>(InSubdivisions)
            )
            , FineChunkSize(Cubus::ChunkSize * InSubdivisions)
        {
        }

        const FCubusDensitySample& GetSample(
            const FIntVector& FineCoordinate
        )
        {
            if (const FCubusDensitySample* Existing =
                Samples.Find(FineCoordinate))
            {
                return *Existing;
            }

            const FVector GlobalCoordinate =
                GetGlobalCoordinate(FineCoordinate);

            const bool bOnChunkBoundary =
                FineCoordinate.X == 0 ||
                FineCoordinate.X == FineChunkSize ||
                FineCoordinate.Y == 0 ||
                FineCoordinate.Y == FineChunkSize ||
                FineCoordinate.Z == 0 ||
                FineCoordinate.Z == FineChunkSize;

            Samples.Add(
                FineCoordinate,
                bOnChunkBoundary
                    ? SampleCanonicalBoundary(GlobalCoordinate)
                    : DensityField.SampleContinuous(GlobalCoordinate)
            );

            return Samples.FindChecked(FineCoordinate);
        }

        FVector GetGradient(const FIntVector& FineCoordinate)
        {
            if (const FVector* Existing = Gradients.Find(FineCoordinate))
            {
                return *Existing;
            }

            const float NegativeX = GetSample(
                FineCoordinate - FIntVector(1, 0, 0)
            ).Density;
            const float PositiveX = GetSample(
                FineCoordinate + FIntVector(1, 0, 0)
            ).Density;
            const float NegativeY = GetSample(
                FineCoordinate - FIntVector(0, 1, 0)
            ).Density;
            const float PositiveY = GetSample(
                FineCoordinate + FIntVector(0, 1, 0)
            ).Density;
            const float NegativeZ = GetSample(
                FineCoordinate - FIntVector(0, 0, 1)
            ).Density;
            const float PositiveZ = GetSample(
                FineCoordinate + FIntVector(0, 0, 1)
            ).Density;

            const FVector Gradient(
                PositiveX - NegativeX,
                PositiveY - NegativeY,
                PositiveZ - NegativeZ
            );

            Gradients.Add(
                FineCoordinate,
                Gradient / FMath::Max(
                    2.0f * SampleSpacing,
                    UE_SMALL_NUMBER
                )
            );

            return Gradients.FindChecked(FineCoordinate);
        }

        FVector GetGlobalCoordinate(
            const FIntVector& FineCoordinate
        ) const
        {
            return GlobalSampleOrigin +
                ToVector(FineCoordinate) * SampleSpacing;
        }

        FVector GetLocalCoordinate(
            const FIntVector& FineCoordinate
        ) const
        {
            return ToVector(FineCoordinate) * SampleSpacing +
                DensityField.GetSampleOffsetInVoxels();
        }

        float GetSampleSpacing() const
        {
            return SampleSpacing;
        }

    private:
        FCubusDensitySample SampleCanonicalBoundary(
            const FVector& GlobalCoordinate
        ) const
        {
            const FIntVector MinimumCoordinate(
                FMath::FloorToInt(GlobalCoordinate.X),
                FMath::FloorToInt(GlobalCoordinate.Y),
                FMath::FloorToInt(GlobalCoordinate.Z)
            );

            const FVector Alpha(
                GlobalCoordinate.X -
                    static_cast<double>(MinimumCoordinate.X),
                GlobalCoordinate.Y -
                    static_cast<double>(MinimumCoordinate.Y),
                GlobalCoordinate.Z -
                    static_cast<double>(MinimumCoordinate.Z)
            );

            FCubusDensitySample Result;
            Result.Density = 0.0f;
            Result.MaterialId = 0;

            float StrongestSolidWeight = -1.0f;

            for (int32 Z = 0; Z <= 1; ++Z)
            {
                const float WeightZ = Z == 0
                    ? 1.0f - static_cast<float>(Alpha.Z)
                    : static_cast<float>(Alpha.Z);

                for (int32 Y = 0; Y <= 1; ++Y)
                {
                    const float WeightY = Y == 0
                        ? 1.0f - static_cast<float>(Alpha.Y)
                        : static_cast<float>(Alpha.Y);

                    for (int32 X = 0; X <= 1; ++X)
                    {
                        const float WeightX = X == 0
                            ? 1.0f - static_cast<float>(Alpha.X)
                            : static_cast<float>(Alpha.X);
                        const float Weight = WeightX * WeightY * WeightZ;

                        if (Weight <= 0.0f)
                        {
                            continue;
                        }

                        const FCubusDensitySample Corner =
                            DensityField.Sample(
                                MinimumCoordinate + FIntVector(X, Y, Z)
                            );

                        Result.Density += Corner.Density * Weight;

                        if (Corner.MaterialId > 0 &&
                            Weight > StrongestSolidWeight)
                        {
                            StrongestSolidWeight = Weight;
                            Result.MaterialId = Corner.MaterialId;
                        }
                    }
                }
            }

            if (Result.Density <= 0.0f)
            {
                Result.MaterialId = 0;
            }
            else
            {
                Result.MaterialId = FMath::Max(1, Result.MaterialId);
            }

            return Result;
        }

        const ICubusDensityField& DensityField;
        FVector GlobalSampleOrigin = FVector::ZeroVector;
        float SampleSpacing = 1.0f;
        int32 FineChunkSize = Cubus::ChunkSize;
        TMap<FIntVector, FCubusDensitySample> Samples;
        TMap<FIntVector, FVector> Gradients;
    };

    bool CellMayContainFineSurface(
        FAdaptiveSampleCache& SampleCache,
        const FIntVector& CoarseCellOrigin,
        const int32 Subdivisions,
        const float IsoLevel
    )
    {
        const FIntVector FineCellOrigin =
            CoarseCellOrigin * Subdivisions;

        const bool bFirstCornerSolid =
            SampleCache.GetSample(FineCellOrigin).IsSolid(IsoLevel);

        for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
        {
            const FIntVector FineCorner =
                FineCellOrigin +
                CornerOffsets[CornerIndex] * Subdivisions;

            if (SampleCache.GetSample(FineCorner).IsSolid(IsoLevel) !=
                bFirstCornerSolid)
            {
                return true;
            }
        }

        const int32 Half = Subdivisions / 2;
        const FIntVector ProbeOffsets[] =
        {
            FIntVector(Half, Half, Half),
            FIntVector(0, Half, Half),
            FIntVector(Subdivisions, Half, Half),
            FIntVector(Half, 0, Half),
            FIntVector(Half, Subdivisions, Half),
            FIntVector(Half, Half, 0),
            FIntVector(Half, Half, Subdivisions)
        };

        for (const FIntVector& ProbeOffset : ProbeOffsets)
        {
            if (SampleCache.GetSample(
                FineCellOrigin + ProbeOffset
            ).IsSolid(IsoLevel) != bFirstCornerSolid)
            {
                return true;
            }
        }

        return false;
    }

    FInterpolatedVertex InterpolateAdaptiveEdge(
        FAdaptiveSampleCache& SampleCache,
        const FIntVector& FineSampleA,
        const FIntVector& FineSampleB,
        const FVector& ChunkMinimum,
        const float CanonicalVoxelSize,
        const float IsoLevel
    )
    {
        const FCubusDensitySample SampleA =
            SampleCache.GetSample(FineSampleA);
        const FCubusDensitySample SampleB =
            SampleCache.GetSample(FineSampleB);

        const float DensityDelta = SampleB.Density - SampleA.Density;
        const float Alpha = FMath::IsNearlyZero(DensityDelta)
            ? 0.5f
            : FMath::Clamp(
                (IsoLevel - SampleA.Density) / DensityDelta,
                0.0f,
                1.0f
            );

        const FVector LocalSamplePosition = FMath::Lerp(
            SampleCache.GetLocalCoordinate(FineSampleA),
            SampleCache.GetLocalCoordinate(FineSampleB),
            Alpha
        );

        const FVector GlobalSamplePosition = FMath::Lerp(
            SampleCache.GetGlobalCoordinate(FineSampleA),
            SampleCache.GetGlobalCoordinate(FineSampleB),
            Alpha
        );

        const FVector InterpolatedGradient = FMath::Lerp(
            SampleCache.GetGradient(FineSampleA),
            SampleCache.GetGradient(FineSampleB),
            Alpha
        );

        const bool bSampleAIsSolid = SampleA.IsSolid(IsoLevel);

        FInterpolatedVertex Result;
        Result.LocalPosition =
            ChunkMinimum + LocalSamplePosition * CanonicalVoxelSize;
        Result.GlobalSamplePosition = GlobalSamplePosition;
        Result.Normal = (-InterpolatedGradient).GetSafeNormal();
        Result.MaterialId = ClampDensityMaterialId(
            bSampleAIsSolid ? SampleA.MaterialId : SampleB.MaterialId
        );
        SetSingleMaterialBlend(Result.MaterialBlend, Result.MaterialId);

        if (Result.Normal.IsNearlyZero())
        {
            const FVector SolidToEmpty = bSampleAIsSolid
                ? SampleCache.GetLocalCoordinate(FineSampleB) -
                    SampleCache.GetLocalCoordinate(FineSampleA)
                : SampleCache.GetLocalCoordinate(FineSampleA) -
                    SampleCache.GetLocalCoordinate(FineSampleB);
            Result.Normal = SolidToEmpty.GetSafeNormal();
        }

        if (Result.Normal.IsNearlyZero())
        {
            Result.Normal = FVector::UpVector;
        }

        return Result;
    }

    bool AddTriangle(
        FCubusMeshData& MeshData,
        FInterpolatedVertex VertexA,
        FInterpolatedVertex VertexB,
        FInterpolatedVertex VertexC
    )
    {
        FVector WindingCrossNormal = FVector::CrossProduct(
            VertexB.LocalPosition - VertexA.LocalPosition,
            VertexC.LocalPosition - VertexA.LocalPosition
        );

        if (WindingCrossNormal.SizeSquared() <= SMALL_NUMBER)
        {
            return false;
        }

        WindingCrossNormal.Normalize();

        const FVector AverageNormal =
            (VertexA.Normal + VertexB.Normal + VertexC.Normal)
                .GetSafeNormal();

        if (!AverageNormal.IsNearlyZero() &&
            FVector::DotProduct(
                WindingCrossNormal,
                AverageNormal
            ) > 0.0)
        {
            Swap(VertexB, VertexC);
            WindingCrossNormal *= -1.0;
        }

        const FInterpolatedVertex Vertices[3] =
        {
            VertexA,
            VertexB,
            VertexC
        };
        const FTriangleMaterialPalette Palette =
            BuildPalette(Vertices);
        const FVector2D PackedPalette = PackPalette(Palette);
        const FVector TangentBasis =
            ResolveTangentBasis(WindingCrossNormal);
        const int32 FirstVertexIndex = MeshData.Vertices.Num();

        MeshData.Vertices.Append({
            VertexA.LocalPosition,
            VertexB.LocalPosition,
            VertexC.LocalPosition
        });
        MeshData.Triangles.Append({
            FirstVertexIndex,
            FirstVertexIndex + 1,
            FirstVertexIndex + 2
        });

        for (const FInterpolatedVertex& Vertex : Vertices)
        {
            FVector TangentDirection =
                (
                    TangentBasis -
                    Vertex.Normal * FVector::DotProduct(
                        TangentBasis,
                        Vertex.Normal
                    )
                ).GetSafeNormal();

            if (TangentDirection.IsNearlyZero())
            {
                TangentDirection = FVector::CrossProduct(
                    FVector::UpVector,
                    Vertex.Normal
                ).GetSafeNormal();
            }

            if (TangentDirection.IsNearlyZero())
            {
                TangentDirection = FVector::ForwardVector;
            }

            MeshData.Normals.Add(Vertex.Normal);
            MeshData.UV0.Add(PackedPalette);
            MeshData.VertexColors.Add(
                BuildWeights(Vertex.MaterialBlend, Palette)
            );
            MeshData.Tangents.Add(
                FProcMeshTangent(TangentDirection, false)
            );
        }

        return true;
    }
}

void FCubusDensityMesher::BuildChunk(
    const FCubusDensitySamplingBuffer& DensityBuffer,
    const float VoxelSize,
    const float IsoLevel,
    TMap<int32, FCubusMeshData>& OutMaterialMeshes,
    int32& OutGeneratedTriangleCount
)
{
    using namespace CubusDensityMesher;

    OutMaterialMeshes.Reset();
    OutGeneratedTriangleCount = 0;

    if (!DensityBuffer.IsBuilt() || VoxelSize <= 0.0f)
    {
        return;
    }

    FCubusMeshData& UnifiedMesh =
        OutMaterialMeshes.FindOrAdd(UnifiedDensityMaterialKey);

    const float ChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) * VoxelSize;
    const FVector ChunkMinimum(
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f
    );
    const FIntVector GlobalChunkOrigin =
        DensityBuffer.GetChunkCoordinate() * Cubus::ChunkSize;

    for (int32 LocalZ = 0; LocalZ < Cubus::ChunkSize; ++LocalZ)
    {
        for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
        {
            for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
            {
                const FIntVector CellOrigin(LocalX, LocalY, LocalZ);
                FCubusDensitySample CornerSamples[8];
                FIntVector CornerCoordinates[8];
                int32 CaseIndex = 0;

                for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
                {
                    CornerCoordinates[CornerIndex] =
                        CellOrigin + CornerOffsets[CornerIndex];
                    CornerSamples[CornerIndex] =
                        DensityBuffer.GetSampleChecked(
                            CornerCoordinates[CornerIndex]
                        );

                    if (CornerSamples[CornerIndex].IsSolid(IsoLevel))
                    {
                        CaseIndex |= 1 << CornerIndex;
                    }
                }

                if (CubusMarchingCubesTables::GetTriangleEdge(
                    CaseIndex,
                    0
                ) < 0)
                {
                    continue;
                }

                FInterpolatedVertex EdgeVertices[12];
                bool bEdgeVertexBuilt[12] = {};

                for (
                    int32 TriangleEdgeIndex = 0;
                    TriangleEdgeIndex < 16;
                    TriangleEdgeIndex += 3
                )
                {
                    if (CubusMarchingCubesTables::GetTriangleEdge(
                        CaseIndex,
                        TriangleEdgeIndex
                    ) < 0)
                    {
                        break;
                    }

                    FInterpolatedVertex TriangleVertices[3];
                    bool bTriangleIsValid = true;

                    for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
                    {
                        const int32 EdgeIndex =
                            CubusMarchingCubesTables::GetTriangleEdge(
                                CaseIndex,
                                TriangleEdgeIndex + VertexIndex
                            );

                        if (EdgeIndex < 0 || EdgeIndex >= 12)
                        {
                            ensureMsgf(
                                false,
                                TEXT("Invalid Marching Cubes edge %d for case %d at table index %d."),
                                EdgeIndex,
                                CaseIndex,
                                TriangleEdgeIndex + VertexIndex
                            );
                            bTriangleIsValid = false;
                            break;
                        }

                        if (!bEdgeVertexBuilt[EdgeIndex])
                        {
                            const int32 CornerIndexA =
                                EdgeCornerIndices[EdgeIndex][0];
                            const int32 CornerIndexB =
                                EdgeCornerIndices[EdgeIndex][1];

                            EdgeVertices[EdgeIndex] = InterpolateEdge(
                                DensityBuffer,
                                CornerCoordinates[CornerIndexA],
                                CornerCoordinates[CornerIndexB],
                                ChunkMinimum,
                                VoxelSize,
                                IsoLevel
                            );
                            bEdgeVertexBuilt[EdgeIndex] = true;
                        }

                        TriangleVertices[VertexIndex] =
                            EdgeVertices[EdgeIndex];
                    }

                    if (!bTriangleIsValid)
                    {
                        continue;
                    }

                    const FVector GlobalCellOrigin = ToVector(
                        GlobalChunkOrigin + CellOrigin
                    );
                    for (FInterpolatedVertex& Vertex : TriangleVertices)
                    {
                        Vertex.MaterialBlend = BuildCellMaterialBlend(
                            CornerSamples,
                            Vertex.GlobalSamplePosition,
                            GlobalCellOrigin,
                            1.0f,
                            IsoLevel,
                            Vertex.MaterialId
                        );
                    }

                    if (AddTriangle(
                        UnifiedMesh,
                        TriangleVertices[0],
                        TriangleVertices[1],
                        TriangleVertices[2]
                    ))
                    {
                        ++OutGeneratedTriangleCount;
                    }
                }
            }
        }
    }

    if (UnifiedMesh.IsEmpty())
    {
        OutMaterialMeshes.Remove(UnifiedDensityMaterialKey);
    }
}

void FCubusDensityMesher::BuildAdaptiveChunk(
    const ICubusDensityField& DensityField,
    const FIntVector& ChunkCoordinate,
    const float CanonicalVoxelSize,
    const int32 SubdivisionsPerVoxel,
    const float IsoLevel,
    TMap<int32, FCubusMeshData>& OutMaterialMeshes,
    int32& OutGeneratedTriangleCount
)
{
    using namespace CubusDensityMesher;

    const int32 Subdivisions =
        FCubusDensityLod::NormalizeSubdivisions(
            SubdivisionsPerVoxel
        );

    if (Subdivisions <= 1)
    {
        FCubusDensitySamplingBuffer DensityBuffer;
        DensityBuffer.Build(ChunkCoordinate, DensityField);
        BuildChunk(
            DensityBuffer,
            CanonicalVoxelSize,
            IsoLevel,
            OutMaterialMeshes,
            OutGeneratedTriangleCount
        );
        return;
    }

    OutMaterialMeshes.Reset();
    OutGeneratedTriangleCount = 0;

    if (CanonicalVoxelSize <= 0.0f)
    {
        return;
    }

    FCubusMeshData& UnifiedMesh =
        OutMaterialMeshes.FindOrAdd(UnifiedDensityMaterialKey);

    const float ChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) * CanonicalVoxelSize;

    const FVector ChunkMinimum(
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f
    );

    FAdaptiveSampleCache SampleCache(
        DensityField,
        ChunkCoordinate,
        Subdivisions
    );

    for (int32 CoarseZ = 0; CoarseZ < Cubus::ChunkSize; ++CoarseZ)
    {
        for (int32 CoarseY = 0; CoarseY < Cubus::ChunkSize; ++CoarseY)
        {
            for (int32 CoarseX = 0; CoarseX < Cubus::ChunkSize; ++CoarseX)
            {
                const FIntVector CoarseCellOrigin(
                    CoarseX,
                    CoarseY,
                    CoarseZ
                );

                if (!CellMayContainFineSurface(
                    SampleCache,
                    CoarseCellOrigin,
                    Subdivisions,
                    IsoLevel
                ))
                {
                    continue;
                }

                const FIntVector FineCoarseOrigin =
                    CoarseCellOrigin * Subdivisions;

                for (int32 SubZ = 0; SubZ < Subdivisions; ++SubZ)
                {
                    for (int32 SubY = 0; SubY < Subdivisions; ++SubY)
                    {
                        for (int32 SubX = 0; SubX < Subdivisions; ++SubX)
                        {
                            const FIntVector FineCellOrigin =
                                FineCoarseOrigin +
                                FIntVector(SubX, SubY, SubZ);

                            FCubusDensitySample CornerSamples[8];
                            FIntVector CornerCoordinates[8];
                            int32 CaseIndex = 0;

                            for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
                            {
                                CornerCoordinates[CornerIndex] =
                                    FineCellOrigin +
                                    CornerOffsets[CornerIndex];

                                CornerSamples[CornerIndex] =
                                    SampleCache.GetSample(
                                        CornerCoordinates[CornerIndex]
                                    );

                                if (CornerSamples[CornerIndex].IsSolid(IsoLevel))
                                {
                                    CaseIndex |= 1 << CornerIndex;
                                }
                            }

                            if (CubusMarchingCubesTables::GetTriangleEdge(
                                CaseIndex,
                                0
                            ) < 0)
                            {
                                continue;
                            }

                            FInterpolatedVertex EdgeVertices[12];
                            bool bEdgeVertexBuilt[12] = {};

                            for (
                                int32 TriangleEdgeIndex = 0;
                                TriangleEdgeIndex < 16;
                                TriangleEdgeIndex += 3
                            )
                            {
                                if (CubusMarchingCubesTables::GetTriangleEdge(
                                    CaseIndex,
                                    TriangleEdgeIndex
                                ) < 0)
                                {
                                    break;
                                }

                                FInterpolatedVertex TriangleVertices[3];
                                bool bTriangleIsValid = true;

                                for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
                                {
                                    const int32 EdgeIndex =
                                        CubusMarchingCubesTables::GetTriangleEdge(
                                            CaseIndex,
                                            TriangleEdgeIndex + VertexIndex
                                        );

                                    if (EdgeIndex < 0 || EdgeIndex >= 12)
                                    {
                                        ensureMsgf(
                                            false,
                                            TEXT("Invalid adaptive Marching Cubes edge %d for case %d at table index %d."),
                                            EdgeIndex,
                                            CaseIndex,
                                            TriangleEdgeIndex + VertexIndex
                                        );
                                        bTriangleIsValid = false;
                                        break;
                                    }

                                    if (!bEdgeVertexBuilt[EdgeIndex])
                                    {
                                        const int32 CornerIndexA =
                                            EdgeCornerIndices[EdgeIndex][0];
                                        const int32 CornerIndexB =
                                            EdgeCornerIndices[EdgeIndex][1];

                                        EdgeVertices[EdgeIndex] =
                                            InterpolateAdaptiveEdge(
                                                SampleCache,
                                                CornerCoordinates[CornerIndexA],
                                                CornerCoordinates[CornerIndexB],
                                                ChunkMinimum,
                                                CanonicalVoxelSize,
                                                IsoLevel
                                            );
                                        bEdgeVertexBuilt[EdgeIndex] = true;
                                    }

                                    TriangleVertices[VertexIndex] =
                                        EdgeVertices[EdgeIndex];
                                }

                                if (!bTriangleIsValid)
                                {
                                    continue;
                                }

                                const FVector GlobalCellOrigin =
                                    SampleCache.GetGlobalCoordinate(
                                        FineCellOrigin
                                    );
                                for (FInterpolatedVertex& Vertex : TriangleVertices)
                                {
                                    Vertex.MaterialBlend =
                                        BuildCellMaterialBlend(
                                            CornerSamples,
                                            Vertex.GlobalSamplePosition,
                                            GlobalCellOrigin,
                                            SampleCache.GetSampleSpacing(),
                                            IsoLevel,
                                            Vertex.MaterialId
                                        );
                                }

                                if (AddTriangle(
                                    UnifiedMesh,
                                    TriangleVertices[0],
                                    TriangleVertices[1],
                                    TriangleVertices[2]
                                ))
                                {
                                    ++OutGeneratedTriangleCount;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (UnifiedMesh.IsEmpty())
    {
        OutMaterialMeshes.Remove(UnifiedDensityMaterialKey);
    }
}
