#include "CubusCore/Meshing/CubusDensityMesher.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Meshing/CubusMarchingCubesTables.h"
#include "CubusCore/Rendering/CubusDensityMaterialKey.h"

namespace CubusDensityMesher
{
    struct FInterpolatedVertex
    {
        FVector LocalPosition = FVector::ZeroVector;
        FVector GlobalSamplePosition = FVector::ZeroVector;
        FVector Normal = FVector::UpVector;
        int32 MaterialId = 1;
    };

    const FIntVector CornerOffsets[8] =
    {
        FIntVector(0, 0, 0),
        FIntVector(1, 0, 0),
        FIntVector(1, 1, 0),
        FIntVector(0, 1, 0),
        FIntVector(0, 0, 1),
        FIntVector(1, 0, 1),
        FIntVector(1, 1, 1),
        FIntVector(0, 1, 1)
    };

    const int32 EdgeCornerIndices[12][2] =
    {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

    FVector ToVector(const FIntVector& Value)
    {
        return FVector(
            static_cast<double>(Value.X),
            static_cast<double>(Value.Y),
            static_cast<double>(Value.Z)
        );
    }

    void ResolveMaterialPair(
        const FInterpolatedVertex (&Vertices)[3],
        int32& OutPrimaryMaterialId,
        int32& OutSecondaryMaterialId
    )
    {
        int32 Counts[3] = { 0, 0, 0 };
        int32 Materials[3] = { 0, 0, 0 };
        int32 UniqueCount = 0;

        for (const FInterpolatedVertex& Vertex : Vertices)
        {
            const int32 MaterialId = FMath::Max(1, Vertex.MaterialId);
            int32 ExistingIndex = INDEX_NONE;

            for (int32 Index = 0; Index < UniqueCount; ++Index)
            {
                if (Materials[Index] == MaterialId)
                {
                    ExistingIndex = Index;
                    break;
                }
            }

            if (ExistingIndex == INDEX_NONE)
            {
                ExistingIndex = UniqueCount++;
                Materials[ExistingIndex] = MaterialId;
            }

            ++Counts[ExistingIndex];
        }

        for (int32 A = 0; A < UniqueCount; ++A)
        {
            for (int32 B = A + 1; B < UniqueCount; ++B)
            {
                if (
                    Counts[B] > Counts[A] ||
                    (Counts[B] == Counts[A] && Materials[B] < Materials[A])
                )
                {
                    Swap(Counts[A], Counts[B]);
                    Swap(Materials[A], Materials[B]);
                }
            }
        }

        OutPrimaryMaterialId = FMath::Max(1, Materials[0]);
        OutSecondaryMaterialId =
            UniqueCount > 1
                ? FMath::Max(1, Materials[1])
                : OutPrimaryMaterialId;

        if (OutSecondaryMaterialId < OutPrimaryMaterialId)
        {
            Swap(OutPrimaryMaterialId, OutSecondaryMaterialId);
        }
    }

    float ResolveBlendWeight(
        const int32 VertexMaterialId,
        const int32 PrimaryMaterialId,
        const int32 SecondaryMaterialId
    )
    {
        if (PrimaryMaterialId == SecondaryMaterialId)
        {
            return 0.0f;
        }

        return VertexMaterialId == SecondaryMaterialId
            ? 1.0f
            : 0.0f;
    }

    void ResolveProjection(
        const FVector& FaceNormal,
        const FVector& GlobalSamplePosition,
        FVector2D& OutUV,
        FVector& OutTangentBasis
    )
    {
        const FVector AbsoluteNormal(
            FMath::Abs(FaceNormal.X),
            FMath::Abs(FaceNormal.Y),
            FMath::Abs(FaceNormal.Z)
        );

        if (
            AbsoluteNormal.Z >= AbsoluteNormal.X &&
            AbsoluteNormal.Z >= AbsoluteNormal.Y
        )
        {
            OutUV = FVector2D(
                GlobalSamplePosition.X,
                GlobalSamplePosition.Y
            );
            OutTangentBasis = FVector::ForwardVector;
            return;
        }

        if (AbsoluteNormal.X >= AbsoluteNormal.Y)
        {
            OutUV = FVector2D(
                GlobalSamplePosition.Y,
                GlobalSamplePosition.Z
            );
            OutTangentBasis = FVector::RightVector;
            return;
        }

        OutUV = FVector2D(
            GlobalSamplePosition.X,
            GlobalSamplePosition.Z
        );
        OutTangentBasis = FVector::ForwardVector;
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
        const float Alpha =
            FMath::IsNearlyZero(DensityDelta)
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

        const FVector GlobalSampleOrigin =
            ToVector(
                DensityBuffer.GetChunkCoordinate() * Cubus::ChunkSize
            );

        const FVector InterpolatedGradient =
            FMath::Lerp(
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
        Result.MaterialId = FMath::Max(
            1,
            bSampleAIsSolid
                ? SampleA.MaterialId
                : SampleB.MaterialId
        );

        if (Result.Normal.IsNearlyZero())
        {
            const FVector SolidToEmpty =
                bSampleAIsSolid
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
        FInterpolatedVertex VertexC,
        const int32 PrimaryMaterialId,
        const int32 SecondaryMaterialId
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

        if (
            !AverageNormal.IsNearlyZero() &&
            FVector::DotProduct(WindingCrossNormal, AverageNormal) > 0.0
        )
        {
            Swap(VertexB, VertexC);
            WindingCrossNormal *= -1.0;
        }

        const int32 FirstVertexIndex = MeshData.Vertices.Num();

        MeshData.Vertices.Append(
        {
            VertexA.LocalPosition,
            VertexB.LocalPosition,
            VertexC.LocalPosition
        });

        MeshData.Triangles.Append(
        {
            FirstVertexIndex,
            FirstVertexIndex + 1,
            FirstVertexIndex + 2
        });

        const FInterpolatedVertex Vertices[3] =
        {
            VertexA,
            VertexB,
            VertexC
        };

        for (const FInterpolatedVertex& Vertex : Vertices)
        {
            FVector2D UV;
            FVector TangentBasis;

            ResolveProjection(
                WindingCrossNormal,
                Vertex.GlobalSamplePosition,
                UV,
                TangentBasis
            );

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
            MeshData.UV0.Add(UV);

            // Density materials use alpha exclusively as the biome/material
            // blend weight. Block top/side/bottom face selection is not used.
            MeshData.VertexColors.Add(
                FLinearColor(
                    1.0f,
                    1.0f,
                    1.0f,
                    ResolveBlendWeight(
                        Vertex.MaterialId,
                        PrimaryMaterialId,
                        SecondaryMaterialId
                    )
                )
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
    OutMaterialMeshes.Reset();
    OutGeneratedTriangleCount = 0;

    if (!DensityBuffer.IsBuilt() || VoxelSize <= 0.0f)
    {
        return;
    }

    const float ChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) * VoxelSize;

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

                if (
                    CubusMarchingCubesTables::GetTriangleEdge(
                        CaseIndex,
                        0
                    ) < 0
                )
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
                    if (
                        CubusMarchingCubesTables::GetTriangleEdge(
                            CaseIndex,
                            TriangleEdgeIndex
                        ) < 0
                    )
                    {
                        break;
                    }

                    FInterpolatedVertex TriangleVertices[3];

                    for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
                    {
                        const int32 EdgeIndex =
                            CubusMarchingCubesTables::GetTriangleEdge(
                                CaseIndex,
                                TriangleEdgeIndex + VertexIndex
                            );

                        check(EdgeIndex >= 0 && EdgeIndex < 12);

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

                    int32 PrimaryMaterialId = 1;
                    int32 SecondaryMaterialId = 1;

                    ResolveMaterialPair(
                        TriangleVertices,
                        PrimaryMaterialId,
                        SecondaryMaterialId
                    );

                    const int32 DensityMaterialKey =
                        FCubusDensityMaterialKey::Make(
                            PrimaryMaterialId,
                            SecondaryMaterialId
                        );

                    FCubusMeshData& MaterialMesh =
                        OutMaterialMeshes.FindOrAdd(DensityMaterialKey);

                    if (
                        AddTriangle(
                            MaterialMesh,
                            TriangleVertices[0],
                            TriangleVertices[1],
                            TriangleVertices[2],
                            PrimaryMaterialId,
                            SecondaryMaterialId
                        )
                    )
                    {
                        ++OutGeneratedTriangleCount;
                    }
                }
            }
        }
    }
}
