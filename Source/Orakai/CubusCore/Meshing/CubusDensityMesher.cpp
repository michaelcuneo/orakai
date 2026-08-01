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
        { 0, 1 },
        { 1, 2 },
        { 2, 3 },
        { 3, 0 },
        { 4, 5 },
        { 5, 6 },
        { 6, 7 },
        { 7, 4 },
        { 0, 4 },
        { 1, 5 },
        { 2, 6 },
        { 3, 7 }
    };

    FVector ToVector(const FIntVector& Value)
    {
        return FVector(
            static_cast<double>(Value.X),
            static_cast<double>(Value.Y),
            static_cast<double>(Value.Z)
        );
    }

    int32 ResolveMaterialId(
        const int32 MaterialA,
        const int32 MaterialB,
        const int32 MaterialC
    )
    {
        if (MaterialA == MaterialB || MaterialA == MaterialC)
        {
            return FMath::Max(1, MaterialA);
        }

        if (MaterialB == MaterialC)
        {
            return FMath::Max(1, MaterialB);
        }

        return FMath::Max(1, MaterialA);
    }

    float ResolveFaceSelector(const FVector& Normal)
    {
        if (Normal.Z >= 0.6)
        {
            return 0.5f;
        }

        if (Normal.Z <= -0.6)
        {
            return 1.0f;
        }

        return 0.0f;
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

        const float DensityDelta =
            SampleB.Density -
            SampleA.Density;

        const float Alpha =
            FMath::IsNearlyZero(DensityDelta)
                ? 0.5f
                : FMath::Clamp(
                    (IsoLevel - SampleA.Density) /
                    DensityDelta,
                    0.0f,
                    1.0f
                );

        const FVector LocalSamplePosition =
            FMath::Lerp(
                ToVector(LocalSampleA),
                ToVector(LocalSampleB),
                Alpha
            ) +
            DensityBuffer.GetSampleOffsetInVoxels();

        const FVector GlobalSampleOrigin =
            ToVector(
                DensityBuffer.GetChunkCoordinate() *
                Cubus::ChunkSize
            );

        const FVector GradientA =
            DensityBuffer.GetGradientChecked(LocalSampleA);

        const FVector GradientB =
            DensityBuffer.GetGradientChecked(LocalSampleB);

        const FVector InterpolatedGradient =
            FMath::Lerp(
                GradientA,
                GradientB,
                Alpha
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
            (-InterpolatedGradient).GetSafeNormal();

        const bool bSampleAIsSolid =
            SampleA.IsSolid(IsoLevel);

        Result.MaterialId =
            FMath::Max(
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

            Result.Normal =
                SolidToEmpty.GetSafeNormal();
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
        FVector FaceNormal =
            FVector::CrossProduct(
                VertexB.LocalPosition -
                    VertexA.LocalPosition,
                VertexC.LocalPosition -
                    VertexA.LocalPosition
            );

        if (FaceNormal.SizeSquared() <= SMALL_NUMBER)
        {
            return false;
        }

        FaceNormal.Normalize();

        const FVector AverageNormal =
            (
                VertexA.Normal +
                VertexB.Normal +
                VertexC.Normal
            ).GetSafeNormal();

        if (
            !AverageNormal.IsNearlyZero() &&
            FVector::DotProduct(
                FaceNormal,
                AverageNormal
            ) < 0.0
        )
        {
            Swap(VertexB, VertexC);
            FaceNormal *= -1.0;
        }

        const int32 FirstVertexIndex =
            MeshData.Vertices.Num();

        MeshData.Vertices.Add(VertexA.LocalPosition);
        MeshData.Vertices.Add(VertexB.LocalPosition);
        MeshData.Vertices.Add(VertexC.LocalPosition);

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
                FaceNormal,
                Vertex.GlobalSamplePosition,
                UV,
                TangentBasis
            );

            FVector TangentDirection =
                (
                    TangentBasis -
                    Vertex.Normal *
                    FVector::DotProduct(
                        TangentBasis,
                        Vertex.Normal
                    )
                ).GetSafeNormal();

            if (TangentDirection.IsNearlyZero())
            {
                TangentDirection =
                    FVector::CrossProduct(
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
            MeshData.VertexColors.Add(
                FLinearColor(
                    1.0f,
                    1.0f,
                    1.0f,
                    ResolveFaceSelector(Vertex.Normal)
                )
            );
            MeshData.Tangents.Add(
                FProcMeshTangent(
                    TangentDirection,
                    false
                )
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

    if (
        !DensityBuffer.IsBuilt() ||
        VoxelSize <= 0.0f
    )
    {
        return;
    }

    const float ChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) *
        VoxelSize;

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
                const FIntVector CellOrigin(
                    LocalX,
                    LocalY,
                    LocalZ
                );

                FCubusDensitySample CornerSamples[8];
                FIntVector CornerCoordinates[8];
                int32 CaseIndex = 0;

                for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
                {
                    CornerCoordinates[CornerIndex] =
                        CellOrigin +
                        CubusDensityMesher::CornerOffsets[CornerIndex];

                    CornerSamples[CornerIndex] =
                        DensityBuffer.GetSampleChecked(
                            CornerCoordinates[CornerIndex]
                        );

                    if (CornerSamples[CornerIndex].IsSolid(IsoLevel))
                    {
                        CaseIndex |=
                            1 << CornerIndex;
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

                CubusDensityMesher::FInterpolatedVertex EdgeVertices[12];
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

                    CubusDensityMesher::FInterpolatedVertex TriangleVertices[3];

                    for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
                    {
                        const int32 EdgeIndex =
                            CubusMarchingCubesTables::GetTriangleEdge(
                                CaseIndex,
                                TriangleEdgeIndex +
                                VertexIndex
                            );

                        check(EdgeIndex >= 0 && EdgeIndex < 12);

                        if (!bEdgeVertexBuilt[EdgeIndex])
                        {
                            const int32 CornerIndexA =
                                CubusDensityMesher::EdgeCornerIndices[
                                    EdgeIndex
                                ][0];

                            const int32 CornerIndexB =
                                CubusDensityMesher::EdgeCornerIndices[
                                    EdgeIndex
                                ][1];

                            EdgeVertices[EdgeIndex] =
                                CubusDensityMesher::InterpolateEdge(
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

                    const int32 MaterialId =
                        CubusDensityMesher::ResolveMaterialId(
                            TriangleVertices[0].MaterialId,
                            TriangleVertices[1].MaterialId,
                            TriangleVertices[2].MaterialId
                        );

                    FCubusMeshData& MaterialMesh =
                        OutMaterialMeshes.FindOrAdd(MaterialId);

                    if (
                        CubusDensityMesher::AddTriangle(
                            MaterialMesh,
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
}
