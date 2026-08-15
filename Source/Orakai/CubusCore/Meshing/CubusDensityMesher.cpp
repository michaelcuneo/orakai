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

    constexpr int32 DensitySampleRowStride =
    FCubusDensitySamplingBuffer::SampleDimension;

    constexpr int32 DensitySampleSliceStride =
        FCubusDensitySamplingBuffer::SampleDimension *
        FCubusDensitySamplingBuffer::SampleDimension;

    /*
    * Flat offsets matching CornerOffsets exactly:
    *
    * 0 = (0,0,0)
    * 1 = (1,0,0)
    * 2 = (1,1,0)
    * 3 = (0,1,0)
    * 4 = (0,0,1)
    * 5 = (1,0,1)
    * 6 = (1,1,1)
    * 7 = (0,1,1)
    */
    constexpr int32 CornerFlatOffsets[8] =
    {
        0,
        1,
        1 + DensitySampleRowStride,
        DensitySampleRowStride,
        DensitySampleSliceStride,
        DensitySampleSliceStride + 1,
        DensitySampleSliceStride +
            DensitySampleRowStride +
            1,
        DensitySampleSliceStride +
            DensitySampleRowStride
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
        FWeightedMaterial (&Materials)[8],
        int32& MaterialCount,
        const int32 MaterialId,
        const float Weight
    )
    {
        if (
            MaterialId <= 0 ||
            Weight <= UE_SMALL_NUMBER
        )
        {
            return;
        }

        const int32 ClampedMaterialId =
            ClampDensityMaterialId(MaterialId);

        for (
            int32 Index = 0;
            Index < MaterialCount;
            ++Index
        )
        {
            if (
                Materials[Index].MaterialId ==
                ClampedMaterialId
            )
            {
                Materials[Index].Weight += Weight;
                return;
            }
        }

        if (MaterialCount >= 8)
        {
            return;
        }

        Materials[MaterialCount].MaterialId =
            ClampedMaterialId;

        Materials[MaterialCount].Weight =
            Weight;

        ++MaterialCount;
    }

    void SortWeightedMaterials(
        FWeightedMaterial (&Materials)[8],
        const int32 MaterialCount
    )
    {
        for (
            int32 Index = 1;
            Index < MaterialCount;
            ++Index
        )
        {
            const FWeightedMaterial Value =
                Materials[Index];

            int32 InsertIndex = Index;

            while (InsertIndex > 0)
            {
                const FWeightedMaterial& Previous =
                    Materials[InsertIndex - 1];

                const bool bValueComesFirst =
                    !FMath::IsNearlyEqual(
                        Value.Weight,
                        Previous.Weight
                    )
                        ? Value.Weight >
                            Previous.Weight
                        : Value.MaterialId <
                            Previous.MaterialId;

                if (!bValueComesFirst)
                {
                    break;
                }

                Materials[InsertIndex] =
                    Previous;

                --InsertIndex;
            }

            Materials[InsertIndex] = Value;
        }
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

        FWeightedMaterial Accumulated[8];
        int32 AccumulatedCount = 0;

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
                AccumulatedCount,
                Sample.MaterialId,
                WeightX * WeightY * WeightZ
            );
        }

        if (AccumulatedCount <= 0)
        {
            return Blend;
        }

        SortWeightedMaterials(
            Accumulated,
            AccumulatedCount
        );

        float TotalWeight = 0.0f;

        // Keep blends stable and readable by limiting each vertex to the
        // two strongest terrain materials.
        const int32 BlendCount =
            FMath::Min(
                AccumulatedCount,
                2
            );

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
        int32 UniqueMaterialIds[12] = {};
        int32 UniqueMaterialCount = 0;

        for (
            const FInterpolatedVertex& Vertex
            : Vertices
        )
        {
            for (int32 Slot = 0; Slot < 4; ++Slot)
            {
                if (
                    Vertex.MaterialBlend.Weights[Slot] <=
                    UE_SMALL_NUMBER
                )
                {
                    continue;
                }

                const int32 MaterialId =
                    ClampDensityMaterialId(
                        Vertex.MaterialBlend
                            .MaterialIds[Slot]
                    );

                bool bAlreadyPresent = false;

                for (
                    int32 ExistingIndex = 0;
                    ExistingIndex <
                        UniqueMaterialCount;
                    ++ExistingIndex
                )
                {
                    if (
                        UniqueMaterialIds[
                            ExistingIndex
                        ] == MaterialId
                    )
                    {
                        bAlreadyPresent = true;
                        break;
                    }
                }

                if (
                    !bAlreadyPresent &&
                    UniqueMaterialCount < 12
                )
                {
                    UniqueMaterialIds[
                        UniqueMaterialCount
                    ] = MaterialId;

                    ++UniqueMaterialCount;
                }
            }
        }

        for (
            int32 Index = 1;
            Index < UniqueMaterialCount;
            ++Index
        )
        {
            const int32 Value =
                UniqueMaterialIds[Index];

            int32 InsertIndex = Index;

            while (
                InsertIndex > 0 &&
                UniqueMaterialIds[
                    InsertIndex - 1
                ] > Value
            )
            {
                UniqueMaterialIds[
                    InsertIndex
                ] =
                    UniqueMaterialIds[
                        InsertIndex - 1
                    ];

                --InsertIndex;
            }

            UniqueMaterialIds[
                InsertIndex
            ] = Value;
        }

        FTriangleMaterialPalette Palette;

        if (UniqueMaterialCount <= 0)
        {
            Palette.MaterialIds[0] =
                ClampDensityMaterialId(
                    Vertices[0].MaterialId
                );

            Palette.Count = 1;
        }
        else
        {
            Palette.Count =
                FMath::Clamp(
                    UniqueMaterialCount,
                    1,
                    4
                );

            for (
                int32 Slot = 0;
                Slot < Palette.Count;
                ++Slot
            )
            {
                Palette.MaterialIds[Slot] =
                    UniqueMaterialIds[Slot];
            }
        }

        for (
            int32 Slot = Palette.Count;
            Slot < 4;
            ++Slot
        )
        {
            Palette.MaterialIds[Slot] =
                Palette.MaterialIds[0];
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
        const FCubusDensitySample& SampleA,
        const FCubusDensitySample& SampleB,
        const FVector& GradientA,
        const FVector& GradientB,
        const FIntVector& LocalSampleA,
        const FIntVector& LocalSampleB,
        const FVector& ChunkMinimum,
        const float VoxelSize,
        const float IsoLevel
    )
    {
        const float DensityDelta =
            SampleB.Density -
            SampleA.Density;

        const float Alpha =
            FMath::IsNearlyZero(
                DensityDelta
            )
                ? 0.5f
                : FMath::Clamp(
                    (
                        IsoLevel -
                        SampleA.Density
                    ) /
                    DensityDelta,
                    0.0f,
                    1.0f
                );

        const FVector LocalSamplePosition =
            FMath::Lerp(
                ToVector(
                    LocalSampleA
                ),
                ToVector(
                    LocalSampleB
                ),
                Alpha
            ) +
            DensityBuffer.GetSampleOffsetInVoxels();

        const FVector GlobalSampleOrigin =
            ToVector(
                DensityBuffer.GetChunkCoordinate() *
                Cubus::ChunkSize
            );

        const FVector InterpolatedGradient =
            FMath::Lerp(
                GradientA,
                GradientB,
                Alpha
            );

        const bool bSampleAIsSolid =
            SampleA.IsSolid(
                IsoLevel
            );

        FInterpolatedVertex Result;

        Result.LocalPosition =
            ChunkMinimum +
            LocalSamplePosition *
            VoxelSize;

        Result.GlobalSamplePosition =
            GlobalSampleOrigin +
            LocalSamplePosition;

        Result.Normal =
            (
                -InterpolatedGradient
            ).GetSafeNormal();

        Result.MaterialId =
            ClampDensityMaterialId(
                bSampleAIsSolid
                    ? SampleA.MaterialId
                    : SampleB.MaterialId
            );

        SetSingleMaterialBlend(
            Result.MaterialBlend,
            Result.MaterialId
        );

        if (Result.Normal.IsNearlyZero())
        {
            const FVector SolidToEmpty =
                bSampleAIsSolid
                    ? ToVector(
                        LocalSampleB -
                        LocalSampleA
                    )
                    : ToVector(
                        LocalSampleA -
                        LocalSampleB
                    );

            Result.Normal =
                SolidToEmpty.GetSafeNormal();
        }

        if (Result.Normal.IsNearlyZero())
        {
            Result.Normal =
                FVector::UpVector;
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
                ToVector(
                    InChunkCoordinate *
                    Cubus::ChunkSize
                )
            )
            , SampleSpacing(
                1.0f /
                static_cast<float>(
                    InSubdivisions
                )
            )
            , FineChunkSize(
                Cubus::ChunkSize *
                InSubdivisions
            )
            , PackedCoordinateExtent(
                FineChunkSize + 3
            )
        {
            /*
            * Only a fraction of the full fine lattice is sampled because coarse
            * cells are rejected before fine marching cubes runs.
            *
            * Reserve enough for the common surface band so the maps avoid repeated
            * allocation/rehash without allocating a dense 3D volume.
            */
            Samples.Reserve(
                8192
            );

            Gradients.Reserve(
                4096
            );
        }

        int32 PackCoordinate(
            const FIntVector& FineCoordinate
        ) const
        {
            /*
            * Gradient evaluation can request one fine sample beyond the nominal
            * chunk range, so shift by +1 before packing:
            *
            *     -1 .. FineChunkSize + 1
            *
            * becomes:
            *
            *      0 .. FineChunkSize + 2
            */
            const int32 X =
                FineCoordinate.X + 1;

            const int32 Y =
                FineCoordinate.Y + 1;

            const int32 Z =
                FineCoordinate.Z + 1;

            return
                X +
                Y * PackedCoordinateExtent +
                Z *
                    PackedCoordinateExtent *
                    PackedCoordinateExtent;
        }

        const FCubusDensitySample& GetSample(
            const FIntVector& FineCoordinate
        )
        {
            const int32 PackedCoordinate =
                PackCoordinate(
                    FineCoordinate
                );

            if (
                const FCubusDensitySample* Existing =
                    Samples.Find(
                        PackedCoordinate
                    )
            )
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

            FCubusDensitySample& AddedSample =
                Samples.Add(
                    PackedCoordinate,
                    bOnChunkBoundary
                        ? SampleCanonicalBoundary(
                            GlobalCoordinate
                        )
                        : DensityField.SampleContinuous(
                            GlobalCoordinate
                        )
                );

            return AddedSample;
        }

        FVector GetGradient(
            const FIntVector& FineCoordinate
        )
        {
            const int32 PackedCoordinate =
                PackCoordinate(
                    FineCoordinate
                );

            if (
                const FVector* Existing =
                    Gradients.Find(
                        PackedCoordinate
                    )
            )
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

            FVector& AddedGradient =
                Gradients.Add(
                    PackedCoordinate,
                    Gradient / FMath::Max(
                        2.0f * SampleSpacing,
                        UE_SMALL_NUMBER
                    )
                );

            return AddedGradient;
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

        FVector GlobalSampleOrigin =
            FVector::ZeroVector;

        float SampleSpacing =
            1.0f;

        int32 FineChunkSize =
            Cubus::ChunkSize;

        int32 PackedCoordinateExtent =
            Cubus::ChunkSize + 3;

        TMap<
            int32,
            FCubusDensitySample
        > Samples;

        TMap<
            int32,
            FVector
        > Gradients;
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

        bool bAnySolid = false;
        bool bAnyEmpty = false;
        float MinimumDistanceFromIso = MAX_flt;

        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            const FIntVector FineCorner =
                FineCellOrigin +
                CornerOffsets[CornerIndex] * Subdivisions;
            const FCubusDensitySample CornerSample =
                SampleCache.GetSample(FineCorner);
            bAnySolid |= CornerSample.IsSolid(IsoLevel);
            bAnyEmpty |= !CornerSample.IsSolid(IsoLevel);
            MinimumDistanceFromIso = FMath::Min(
                MinimumDistanceFromIso,
                FMath::Abs(CornerSample.Density - IsoLevel)
            );
        }

        if (bAnySolid && bAnyEmpty)
        {
            return true;
        }

        /*
         * Do not let the canonical 80 cm lattice decide whether 20 cm detail is
         * allowed to exist. Terrain fine relief is capped at 0.34 voxel and
         * geology at 0.72 voxel; a 2.0-voxel conservative band therefore
         * guarantees that a bounded interior zero-crossing is refined rather
         * than discarded before the fine lattice is sampled.
         */
        if (MinimumDistanceFromIso <= 2.0f)
        {
            return true;
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
            const FCubusDensitySample Probe = SampleCache.GetSample(
                FineCellOrigin + ProbeOffset
            );
            if (Probe.IsSolid(IsoLevel) != bAnySolid ||
                FMath::Abs(Probe.Density - IsoLevel) <= 1.0f)
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
    const FVector EdgeAB =
        VertexB.LocalPosition -
        VertexA.LocalPosition;

    const FVector EdgeAC =
        VertexC.LocalPosition -
        VertexA.LocalPosition;

    const FVector EdgeBC =
        VertexC.LocalPosition -
        VertexB.LocalPosition;

    /*
     * Reject non-finite geometry before it can reach rendering or Chaos.
     */
    if (
        !FMath::IsFinite(VertexA.LocalPosition.X) ||
        !FMath::IsFinite(VertexA.LocalPosition.Y) ||
        !FMath::IsFinite(VertexA.LocalPosition.Z) ||
        !FMath::IsFinite(VertexB.LocalPosition.X) ||
        !FMath::IsFinite(VertexB.LocalPosition.Y) ||
        !FMath::IsFinite(VertexB.LocalPosition.Z) ||
        !FMath::IsFinite(VertexC.LocalPosition.X) ||
        !FMath::IsFinite(VertexC.LocalPosition.Y) ||
        !FMath::IsFinite(VertexC.LocalPosition.Z)
    )
    {
        return false;
    }

    /*
     * Reject collapsed edges.
     *
     * Density vertices are expressed in centimetres, so anything below one
     * hundredth of a centimetre is not useful terrain geometry or collision.
     */
    constexpr double MinimumEdgeLengthSquared =
        0.01 * 0.01;

    if (
        EdgeAB.SizeSquared() <= MinimumEdgeLengthSquared ||
        EdgeAC.SizeSquared() <= MinimumEdgeLengthSquared ||
        EdgeBC.SizeSquared() <= MinimumEdgeLengthSquared
    )
    {
        return false;
    }

    FVector WindingCrossNormal =
        FVector::CrossProduct(
            EdgeAB,
            EdgeAC
        );

    /*
     * Cross-product magnitude is twice the triangle area.
     *
     * SMALL_NUMBER is much too small for centimetre-scale collision geometry
     * and allows extremely thin sliver triangles through to Chaos.
     */
    constexpr double MinimumDoubleAreaSquared =
        0.01 * 0.01;

    if (
        !FMath::IsFinite(WindingCrossNormal.X) ||
        !FMath::IsFinite(WindingCrossNormal.Y) ||
        !FMath::IsFinite(WindingCrossNormal.Z) ||
        WindingCrossNormal.SizeSquared() <=
            MinimumDoubleAreaSquared
    )
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
    int32& OutGeneratedTriangleCount,
    const ICubusDensityField* SurfaceMaterialField
)
{
    using namespace CubusDensityMesher;

    OutMaterialMeshes.Reset();
    OutGeneratedTriangleCount = 0;

    if (
        !DensityBuffer.IsBuilt() ||
        VoxelSize <= 0.0f
    )
    {
        return;
    }

    FCubusMeshData& UnifiedMesh =
        OutMaterialMeshes.FindOrAdd(
            UnifiedDensityMaterialKey
        );

    const float ChunkWorldSize =
        static_cast<float>(
            Cubus::ChunkSize
        ) *
        VoxelSize;

    const FVector ChunkMinimum(
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f
    );

    const FIntVector GlobalChunkOrigin =
        DensityBuffer.GetChunkCoordinate() *
        Cubus::ChunkSize;

    for (
        int32 LocalZ = 0;
        LocalZ < Cubus::ChunkSize;
        ++LocalZ
    )
    {
        for (
            int32 LocalY = 0;
            LocalY < Cubus::ChunkSize;
            ++LocalY
        )
        {
            for (
                int32 LocalX = 0;
                LocalX < Cubus::ChunkSize;
                ++LocalX
            )
            {
                const FIntVector CellOrigin(
                    LocalX,
                    LocalY,
                    LocalZ
                );

                /*
                 * Buffered local coordinates begin at -1.
                 *
                 * Canonical local sample (0,0,0) therefore occupies buffered
                 * coordinate (1,1,1).
                 */
                const int32 BufferedX =
                    LocalX -
                    FCubusDensitySamplingBuffer::
                        MinimumLocalSample;

                const int32 BufferedY =
                    LocalY -
                    FCubusDensitySamplingBuffer::
                        MinimumLocalSample;

                const int32 BufferedZ =
                    LocalZ -
                    FCubusDensitySamplingBuffer::
                        MinimumLocalSample;

                const int32 BaseSampleIndex =
                    BufferedX +
                    DensitySampleRowStride *
                    (
                        BufferedY +
                        DensitySampleRowStride *
                        BufferedZ
                    );

                FCubusDensitySample CornerSamples[8];
                FIntVector CornerCoordinates[8];
                int32 CornerFlatIndices[8];

                int32 CaseIndex = 0;

                for (
                    int32 CornerIndex = 0;
                    CornerIndex < 8;
                    ++CornerIndex
                )
                {
                    CornerCoordinates[
                        CornerIndex
                    ] =
                        CellOrigin +
                        CornerOffsets[
                            CornerIndex
                        ];

                    CornerFlatIndices[
                        CornerIndex
                    ] =
                        BaseSampleIndex +
                        CornerFlatOffsets[
                            CornerIndex
                        ];

                    CornerSamples[
                        CornerIndex
                    ] =
                        DensityBuffer
                            .GetSampleByFlatIndexChecked(
                                CornerFlatIndices[
                                    CornerIndex
                                ]
                            );

                    if (
                        CornerSamples[
                            CornerIndex
                        ].IsSolid(
                            IsoLevel
                        )
                    )
                    {
                        CaseIndex |=
                            1 << CornerIndex;
                    }
                }

                if (
                    CubusMarchingCubesTables::
                        GetTriangleEdge(
                            CaseIndex,
                            0
                        ) < 0
                )
                {
                    continue;
                }

                const FVector GlobalCellOrigin =
                    ToVector(
                        GlobalChunkOrigin +
                        CellOrigin
                    );

                FInterpolatedVertex EdgeVertices[12];
                bool bEdgeVertexBuilt[12] = {};

                FVector CornerGradients[8];
                bool bCornerGradientBuilt[8] = {};

                for (
                    int32 TriangleEdgeIndex = 0;
                    TriangleEdgeIndex < 16;
                    TriangleEdgeIndex += 3
                )
                {
                    if (
                        CubusMarchingCubesTables::
                            GetTriangleEdge(
                                CaseIndex,
                                TriangleEdgeIndex
                            ) < 0
                    )
                    {
                        break;
                    }

                    FInterpolatedVertex
                        TriangleVertices[3];

                    bool bTriangleIsValid = true;

                    for (
                        int32 VertexIndex = 0;
                        VertexIndex < 3;
                        ++VertexIndex
                    )
                    {
                        const int32 EdgeIndex =
                            CubusMarchingCubesTables::
                                GetTriangleEdge(
                                    CaseIndex,
                                    TriangleEdgeIndex +
                                        VertexIndex
                                );

                        if (
                            EdgeIndex < 0 ||
                            EdgeIndex >= 12
                        )
                        {
                            ensureMsgf(
                                false,
                                TEXT(
                                    "Invalid Marching Cubes edge %d "
                                    "for case %d at table index %d."
                                ),
                                EdgeIndex,
                                CaseIndex,
                                TriangleEdgeIndex +
                                    VertexIndex
                            );

                            bTriangleIsValid = false;
                            break;
                        }

                        if (
                            !bEdgeVertexBuilt[
                                EdgeIndex
                            ]
                        )
                        {
                            const int32 CornerIndexA =
                                EdgeCornerIndices[
                                    EdgeIndex
                                ][0];

                            const int32 CornerIndexB =
                                EdgeCornerIndices[
                                    EdgeIndex
                                ][1];

                            if (
                                !bCornerGradientBuilt[
                                    CornerIndexA
                                ]
                            )
                            {
                                CornerGradients[
                                    CornerIndexA
                                ] =
                                    DensityBuffer
                                        .GetGradientByFlatIndexChecked(
                                            CornerFlatIndices[
                                                CornerIndexA
                                            ]
                                        );

                                bCornerGradientBuilt[
                                    CornerIndexA
                                ] = true;
                            }

                            if (
                                !bCornerGradientBuilt[
                                    CornerIndexB
                                ]
                            )
                            {
                                CornerGradients[
                                    CornerIndexB
                                ] =
                                    DensityBuffer
                                        .GetGradientByFlatIndexChecked(
                                            CornerFlatIndices[
                                                CornerIndexB
                                            ]
                                        );

                                bCornerGradientBuilt[
                                    CornerIndexB
                                ] = true;
                            }

                            EdgeVertices[
                                EdgeIndex
                            ] =
                                InterpolateEdge(
                                    DensityBuffer,
                                    CornerSamples[
                                        CornerIndexA
                                    ],
                                    CornerSamples[
                                        CornerIndexB
                                    ],
                                    CornerGradients[
                                        CornerIndexA
                                    ],
                                    CornerGradients[
                                        CornerIndexB
                                    ],
                                    CornerCoordinates[
                                        CornerIndexA
                                    ],
                                    CornerCoordinates[
                                        CornerIndexB
                                    ],
                                    ChunkMinimum,
                                    VoxelSize,
                                    IsoLevel
                                );

                            if (SurfaceMaterialField != nullptr)
                            {
                                FInterpolatedVertex& SurfaceVertex =
                                    EdgeVertices[EdgeIndex];

                                // Probe a tiny distance toward the solid side.
                                // Coarse LOD passes its scaled canonical field,
                                // so biome and slope are resolved at the actual
                                // interpolated surface position instead of at
                                // cell corners separated by an entire LOD stride.
                                constexpr float MaterialProbeDepth = 0.001f;
                                const FVector MaterialProbePosition =
                                    SurfaceVertex.GlobalSamplePosition -
                                    SurfaceVertex.Normal * MaterialProbeDepth;

                                const FCubusDensitySample SurfaceSample =
                                    SurfaceMaterialField->SampleContinuous(
                                        MaterialProbePosition
                                    );

                                if (SurfaceSample.MaterialId > 0)
                                {
                                    SurfaceVertex.MaterialId =
                                        ClampDensityMaterialId(
                                            SurfaceSample.MaterialId
                                        );

                                    SetSingleMaterialBlend(
                                        SurfaceVertex.MaterialBlend,
                                        SurfaceVertex.MaterialId
                                    );
                                }
                                else
                                {
                                    SurfaceVertex.MaterialBlend =
                                        BuildCellMaterialBlend(
                                            CornerSamples,
                                            SurfaceVertex.GlobalSamplePosition,
                                            GlobalCellOrigin,
                                            1.0f,
                                            IsoLevel,
                                            SurfaceVertex.MaterialId
                                        );
                                }
                            }
                            else
                            {
                                EdgeVertices[
                                    EdgeIndex
                                ].MaterialBlend =
                                    BuildCellMaterialBlend(
                                        CornerSamples,
                                        EdgeVertices[
                                            EdgeIndex
                                        ].GlobalSamplePosition,
                                        GlobalCellOrigin,
                                        1.0f,
                                        IsoLevel,
                                        EdgeVertices[
                                            EdgeIndex
                                        ].MaterialId
                                    );
                            }

                            bEdgeVertexBuilt[
                                EdgeIndex
                            ] = true;
                        }

                        TriangleVertices[
                            VertexIndex
                        ] =
                            EdgeVertices[
                                EdgeIndex
                            ];
                    }

                    if (!bTriangleIsValid)
                    {
                        continue;
                    }

                    if (
                        AddTriangle(
                            UnifiedMesh,
                            TriangleVertices[0],
                            TriangleVertices[1],
                            TriangleVertices[2]
                        )
                    )
                    {
                        ++OutGeneratedTriangleCount;
                    }
                }
            }
        }
    }

    if (UnifiedMesh.IsEmpty())
    {
        OutMaterialMeshes.Remove(
            UnifiedDensityMaterialKey
        );
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

                            const FVector GlobalCellOrigin =
                                SampleCache.GetGlobalCoordinate(
                                    FineCellOrigin
                                );

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

                                        EdgeVertices[EdgeIndex]
                                            .MaterialBlend =
                                                BuildCellMaterialBlend(
                                                    CornerSamples,
                                                    EdgeVertices[EdgeIndex]
                                                        .GlobalSamplePosition,
                                                    GlobalCellOrigin,
                                                    SampleCache
                                                        .GetSampleSpacing(),
                                                    IsoLevel,
                                                    EdgeVertices[EdgeIndex]
                                                        .MaterialId
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
