#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Meshing/CubusMarchingCubesTables.h"

namespace CubusDensityMesher
{
    struct FInterpolatedVertex
    {
        FVector LocalPosition = FVector::ZeroVector;
        FVector GlobalSamplePosition = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
        int32 MaterialId = 1;
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

            return 0;
        }
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

    FTriangleMaterialPalette BuildPalette(
        const FInterpolatedVertex (&Vertices)[3]
    )
    {
        FTriangleMaterialPalette Palette;
        Palette.Count = 0;

        for (const FInterpolatedVertex& Vertex : Vertices)
        {
            const int32 MaterialId = ClampDensityMaterialId(Vertex.MaterialId);
            bool bAlreadyPresent = false;

            for (int32 Slot = 0; Slot < Palette.Count; ++Slot)
            {
                if (Palette.MaterialIds[Slot] == MaterialId)
                {
                    bAlreadyPresent = true;
                    break;
                }
            }

            if (!bAlreadyPresent && Palette.Count < 4)
            {
                Palette.MaterialIds[Palette.Count++] = MaterialId;
            }
        }

        if (Palette.Count <= 0)
        {
            Palette.MaterialIds[0] = 1;
            Palette.Count = 1;
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
            static_cast<double>(Palette.MaterialIds[0] + Palette.MaterialIds[1] * Base),
            static_cast<double>(Palette.MaterialIds[2] + Palette.MaterialIds[3] * Base)
        );
    }

    FLinearColor BuildWeights(
        const int32 MaterialId,
        const FTriangleMaterialPalette& Palette
    )
    {
        const int32 Slot = Palette.FindSlot(ClampDensityMaterialId(MaterialId));
        switch (Slot)
        {
            case 1: return FLinearColor(0.0f, 1.0f, 0.0f, 0.0f);
            case 2: return FLinearColor(0.0f, 0.0f, 1.0f, 0.0f);
            case 3: return FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
            default: return FLinearColor(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    FVector ResolveTangentBasis(const FVector& FaceNormal)
    {
        const FVector AbsoluteNormal(
            FMath::Abs(FaceNormal.X),
            FMath::Abs(FaceNormal.Y),
            FMath::Abs(FaceNormal.Z)
        );

        if (AbsoluteNormal.X >= AbsoluteNormal.Y && AbsoluteNormal.X >= AbsoluteNormal.Z)
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
        const FCubusDensitySample& SampleA = DensityBuffer.GetSampleChecked(LocalSampleA);
        const FCubusDensitySample& SampleB = DensityBuffer.GetSampleChecked(LocalSampleB);

        const float DensityDelta = SampleB.Density - SampleA.Density;
        const float Alpha = FMath::IsNearlyZero(DensityDelta)
            ? 0.5f
            : FMath::Clamp((IsoLevel - SampleA.Density) / DensityDelta, 0.0f, 1.0f);

        const FVector LocalSamplePosition =
            FMath::Lerp(ToVector(LocalSampleA), ToVector(LocalSampleB), Alpha) +
            DensityBuffer.GetSampleOffsetInVoxels();

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
        Result.LocalPosition = ChunkMinimum + LocalSamplePosition * VoxelSize;
        Result.GlobalSamplePosition = GlobalSampleOrigin + LocalSamplePosition;
        Result.Normal = (-InterpolatedGradient).GetSafeNormal();
        Result.MaterialId = ClampDensityMaterialId(
            bSampleAIsSolid ? SampleA.MaterialId : SampleB.MaterialId
        );

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
            (VertexA.Normal + VertexB.Normal + VertexC.Normal).GetSafeNormal();

        if (!AverageNormal.IsNearlyZero() &&
            FVector::DotProduct(WindingCrossNormal, AverageNormal) > 0.0)
        {
            Swap(VertexB, VertexC);
            WindingCrossNormal *= -1.0;
        }

        const FInterpolatedVertex Vertices[3] = { VertexA, VertexB, VertexC };
        const FTriangleMaterialPalette Palette = BuildPalette(Vertices);
        const FVector2D PackedPalette = PackPalette(Palette);
        const FVector TangentBasis = ResolveTangentBasis(WindingCrossNormal);
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
                (TangentBasis - Vertex.Normal * FVector::DotProduct(TangentBasis, Vertex.Normal))
                    .GetSafeNormal();

            if (TangentDirection.IsNearlyZero())
            {
                TangentDirection = FVector::CrossProduct(FVector::UpVector, Vertex.Normal)
                    .GetSafeNormal();
            }

            if (TangentDirection.IsNearlyZero())
            {
                TangentDirection = FVector::ForwardVector;
            }

            MeshData.Normals.Add(Vertex.Normal);
            MeshData.UV0.Add(PackedPalette);
            MeshData.VertexColors.Add(BuildWeights(Vertex.MaterialId, Palette));
            MeshData.Tangents.Add(FProcMeshTangent(TangentDirection, false));
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

    const float ChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * VoxelSize;
    const FVector ChunkMinimum(
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f
    );

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
                    CornerCoordinates[CornerIndex] = CellOrigin + CornerOffsets[CornerIndex];
                    CornerSamples[CornerIndex] =
                        DensityBuffer.GetSampleChecked(CornerCoordinates[CornerIndex]);

                    if (CornerSamples[CornerIndex].IsSolid(IsoLevel))
                    {
                        CaseIndex |= 1 << CornerIndex;
                    }
                }

                if (CubusMarchingCubesTables::GetTriangleEdge(CaseIndex, 0) < 0)
                {
                    continue;
                }

                FInterpolatedVertex EdgeVertices[12];
                bool bEdgeVertexBuilt[12] = {};

                for (int32 TriangleEdgeIndex = 0;
                    TriangleEdgeIndex < 16;
                    TriangleEdgeIndex += 3)
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
                            const int32 CornerIndexA = EdgeCornerIndices[EdgeIndex][0];
                            const int32 CornerIndexB = EdgeCornerIndices[EdgeIndex][1];

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

                        TriangleVertices[VertexIndex] = EdgeVertices[EdgeIndex];
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

    if (UnifiedMesh.IsEmpty())
    {
        OutMaterialMeshes.Remove(UnifiedDensityMaterialKey);
    }
}
