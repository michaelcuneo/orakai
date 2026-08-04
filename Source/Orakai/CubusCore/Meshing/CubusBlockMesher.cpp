#include "CubusCore/Meshing/CubusBlockMesher.h"

#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"

namespace CubusBlockMesher
{
    enum EFaceIndex : int32
    {
        PositiveX = 0,
        NegativeX,
        PositiveY,
        NegativeY,
        PositiveZ,
        NegativeZ,
        FaceCount
    };

    const FIntVector NeighbourOffsets[FaceCount] =
    {
        FIntVector(1, 0, 0),
        FIntVector(-1, 0, 0),
        FIntVector(0, 1, 0),
        FIntVector(0, -1, 0),
        FIntVector(0, 0, 1),
        FIntVector(0, 0, -1)
    };

    float GetMaterialSelector(const int32 FaceIndex)
    {
        if (FaceIndex == PositiveZ)
        {
            return 0.5f;
        }

        if (FaceIndex == NegativeZ)
        {
            return 1.0f;
        }

        return 0.0f;
    }

    FVector GetTriangleNormal(
        const FVector& Vertex0,
        const FVector& Vertex1,
        const FVector& Vertex2
    )
    {
        return FVector::CrossProduct(
            Vertex2 - Vertex0,
            Vertex1 - Vertex0
        ).GetSafeNormal();
    }
}

void FCubusBlockMesher::BuildChunk(
    const FCubusBlockChunkNeighborhood& Neighborhood,
    const UCubusMaterialRegistry* MaterialRegistry,
    const float VoxelSize,
    FCubusMaterialMeshMap& OutMaterialMeshes,
    int32& OutGeneratedFaceCount
)
{
    OutMaterialMeshes.Reset();
    OutMaterialMeshes.Reserve(
        MaterialRegistry != nullptr
            ? MaterialRegistry->Materials.Num()
            : 1
    );

    OutGeneratedFaceCount = 0;

    if (Neighborhood.Centre == nullptr || VoxelSize <= 0.0f)
    {
        return;
    }

    const FCubusBlockChunkData& Chunk = *Neighborhood.Centre;
    const float HalfVoxelSize = VoxelSize * 0.5f;
    const float ChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) * VoxelSize;

    const FVector ChunkMinimum(
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f,
        ChunkWorldSize * -0.5f
    );

    for (int32 Z = 0; Z < Cubus::ChunkSize; ++Z)
    {
        for (int32 Y = 0; Y < Cubus::ChunkSize; ++Y)
        {
            for (int32 X = 0; X < Cubus::ChunkSize; ++X)
            {
                const FCubusBlockVoxel* CurrentVoxel =
                    Chunk.GetVoxel(X, Y, Z);

                if (CurrentVoxel == nullptr || CurrentVoxel->IsEmpty())
                {
                    continue;
                }

                const int32 CurrentMaterialId = CurrentVoxel->MaterialId;
                const bool bCurrentIsWater = CurrentVoxel->IsWater();

                const bool bCurrentIsRenderable =
                    bCurrentIsWater ||
                    (
                        MaterialRegistry != nullptr
                            ? MaterialRegistry->IsRenderableSolid(
                                CurrentMaterialId
                            )
                            : true
                    );

                if (!bCurrentIsRenderable)
                {
                    continue;
                }

                FCubusMeshData& MaterialMesh =
                    OutMaterialMeshes.FindOrAdd(CurrentMaterialId);

                const FVector VoxelCentre =
                    ChunkMinimum +
                    FVector(
                        (static_cast<float>(X) + 0.5f) * VoxelSize,
                        (static_cast<float>(Y) + 0.5f) * VoxelSize,
                        (static_cast<float>(Z) + 0.5f) * VoxelSize
                    );

                FCubusBlockSurfaceClassification Classification;

                if (!bCurrentIsWater)
                {
                    Classification = FCubusBlockSurfaceClassifier::Classify(
                        Neighborhood,
                        MaterialRegistry,
                        X,
                        Y,
                        Z
                    );
                }

                if (Classification.IsShaped())
                {
                    OutGeneratedFaceCount += AddShapedVoxel(
                        MaterialMesh,
                        VoxelCentre,
                        HalfVoxelSize,
                        Classification.Shape
                    );
                    continue;
                }

                for (
                    int32 FaceIndex = 0;
                    FaceIndex < CubusBlockMesher::FaceCount;
                    ++FaceIndex
                )
                {
                    const FIntVector NeighbourPosition =
                        FIntVector(X, Y, Z) +
                        CubusBlockMesher::NeighbourOffsets[FaceIndex];

                    const FCubusBlockVoxel* NeighbourVoxel =
                        Neighborhood.GetVoxel(
                            NeighbourPosition.X,
                            NeighbourPosition.Y,
                            NeighbourPosition.Z
                        );

                    bool bRenderFace = true;

                    if (
                        NeighbourVoxel != nullptr &&
                        !NeighbourVoxel->IsEmpty()
                    )
                    {
                        const bool bNeighbourIsWater =
                            NeighbourVoxel->IsWater();

                        if (bCurrentIsWater)
                        {
                            bRenderFace = false;
                        }
                        else if (bNeighbourIsWater)
                        {
                            bRenderFace = true;
                        }
                        else
                        {
                            const bool bNeighbourOccludesFace =
                                MaterialRegistry != nullptr
                                    ? MaterialRegistry->OccludesBlockFaces(
                                        NeighbourVoxel->MaterialId
                                    )
                                    : true;

                            bRenderFace = !bNeighbourOccludesFace;

                            if (
                                !bRenderFace &&
                                FaceIndex <= CubusBlockMesher::NegativeY
                            )
                            {
                                const FCubusBlockSurfaceClassification
                                    NeighbourClassification =
                                        FCubusBlockSurfaceClassifier::Classify(
                                            Neighborhood,
                                            MaterialRegistry,
                                            NeighbourPosition.X,
                                            NeighbourPosition.Y,
                                            NeighbourPosition.Z
                                        );

                                bRenderFace =
                                    NeighbourClassification.IsShaped();
                            }
                        }
                    }

                    if (!bRenderFace)
                    {
                        continue;
                    }

                    AddVoxelFace(
                        MaterialMesh,
                        VoxelCentre,
                        HalfVoxelSize,
                        FaceIndex
                    );

                    ++OutGeneratedFaceCount;
                }
            }
        }
    }
}

int32 FCubusBlockMesher::AddShapedVoxel(
    FCubusMeshData& MeshData,
    const FVector& VoxelCentre,
    const float HalfVoxelSize,
    const ECubusBlockSurfaceShape Shape
)
{
    enum ECornerIndex : int32
    {
        NegativeXNegativeY = 0,
        NegativeXPositiveY,
        PositiveXPositiveY,
        PositiveXNegativeY,
        CornerCount
    };

    bool bHighCorners[CornerCount] =
    {
        false,
        false,
        false,
        false
    };

    switch (Shape)
    {
        case ECubusBlockSurfaceShape::RampPositiveX:
            bHighCorners[NegativeXNegativeY] = true;
            bHighCorners[NegativeXPositiveY] = true;
            break;

        case ECubusBlockSurfaceShape::RampNegativeX:
            bHighCorners[PositiveXPositiveY] = true;
            bHighCorners[PositiveXNegativeY] = true;
            break;

        case ECubusBlockSurfaceShape::RampPositiveY:
            bHighCorners[NegativeXNegativeY] = true;
            bHighCorners[PositiveXNegativeY] = true;
            break;

        case ECubusBlockSurfaceShape::RampNegativeY:
            bHighCorners[NegativeXPositiveY] = true;
            bHighCorners[PositiveXPositiveY] = true;
            break;

        case ECubusBlockSurfaceShape::CornerLowNegativeXNegativeY:
            for (int32 Index = 0; Index < CornerCount; ++Index)
            {
                bHighCorners[Index] = true;
            }
            bHighCorners[NegativeXNegativeY] = false;
            break;

        case ECubusBlockSurfaceShape::CornerLowNegativeXPositiveY:
            for (int32 Index = 0; Index < CornerCount; ++Index)
            {
                bHighCorners[Index] = true;
            }
            bHighCorners[NegativeXPositiveY] = false;
            break;

        case ECubusBlockSurfaceShape::CornerLowPositiveXNegativeY:
            for (int32 Index = 0; Index < CornerCount; ++Index)
            {
                bHighCorners[Index] = true;
            }
            bHighCorners[PositiveXNegativeY] = false;
            break;

        case ECubusBlockSurfaceShape::CornerLowPositiveXPositiveY:
            for (int32 Index = 0; Index < CornerCount; ++Index)
            {
                bHighCorners[Index] = true;
            }
            bHighCorners[PositiveXPositiveY] = false;
            break;

        default:
            return 0;
    }

    static const FVector2D CornerOffsets[CornerCount] =
    {
        FVector2D(-1.0f, -1.0f),
        FVector2D(-1.0f, 1.0f),
        FVector2D(1.0f, 1.0f),
        FVector2D(1.0f, -1.0f)
    };

    FVector BottomVertices[CornerCount];
    FVector TopVertices[CornerCount];

    for (int32 CornerIndex = 0; CornerIndex < CornerCount; ++CornerIndex)
    {
        const FVector HorizontalOffset(
            CornerOffsets[CornerIndex].X * HalfVoxelSize,
            CornerOffsets[CornerIndex].Y * HalfVoxelSize,
            0.0f
        );

        BottomVertices[CornerIndex] =
            VoxelCentre +
            HorizontalOffset -
            FVector::UpVector * HalfVoxelSize;

        TopVertices[CornerIndex] =
            VoxelCentre +
            HorizontalOffset +
            FVector::UpVector *
                (bHighCorners[CornerIndex]
                    ? HalfVoxelSize
                    : -HalfVoxelSize);
    }

    int32 GeneratedFaceCount = 0;
    const float TopSelector =
        CubusBlockMesher::GetMaterialSelector(
            CubusBlockMesher::PositiveZ
        );

    auto AddTopTriangle =
        [&](const int32 A, const int32 B, const int32 C)
        {
            const FVector Normal = CubusBlockMesher::GetTriangleNormal(
                TopVertices[A],
                TopVertices[B],
                TopVertices[C]
            );

            AddTriangle(
                MeshData,
                TopVertices[A],
                TopVertices[B],
                TopVertices[C],
                Normal,
                TopSelector
            );

            ++GeneratedFaceCount;
        };

    if (
        bHighCorners[NegativeXPositiveY] ||
        bHighCorners[PositiveXNegativeY]
    )
    {
        AddTopTriangle(
            NegativeXNegativeY,
            NegativeXPositiveY,
            PositiveXNegativeY
        );
        AddTopTriangle(
            NegativeXPositiveY,
            PositiveXPositiveY,
            PositiveXNegativeY
        );
    }
    else
    {
        AddTopTriangle(
            NegativeXNegativeY,
            NegativeXPositiveY,
            PositiveXPositiveY
        );
        AddTopTriangle(
            NegativeXNegativeY,
            PositiveXPositiveY,
            PositiveXNegativeY
        );
    }

    struct FSideDefinition
    {
        int32 StartCorner;
        int32 EndCorner;
        FVector Normal;
    };

    static const FSideDefinition Sides[] =
    {
        {
            PositiveXNegativeY,
            PositiveXPositiveY,
            FVector(1.0f, 0.0f, 0.0f)
        },
        {
            NegativeXPositiveY,
            NegativeXNegativeY,
            FVector(-1.0f, 0.0f, 0.0f)
        },
        {
            PositiveXPositiveY,
            NegativeXPositiveY,
            FVector(0.0f, 1.0f, 0.0f)
        },
        {
            NegativeXNegativeY,
            PositiveXNegativeY,
            FVector(0.0f, -1.0f, 0.0f)
        }
    };

    const float SideSelector =
        CubusBlockMesher::GetMaterialSelector(
            CubusBlockMesher::PositiveX
        );

    for (const FSideDefinition& Side : Sides)
    {
        const bool bStartHigh = bHighCorners[Side.StartCorner];
        const bool bEndHigh = bHighCorners[Side.EndCorner];

        if (!bStartHigh && !bEndHigh)
        {
            continue;
        }

        if (bStartHigh && bEndHigh)
        {
            AddFace(
                MeshData,
                BottomVertices[Side.StartCorner],
                TopVertices[Side.StartCorner],
                TopVertices[Side.EndCorner],
                BottomVertices[Side.EndCorner],
                Side.Normal,
                SideSelector
            );
        }
        else if (bStartHigh)
        {
            AddTriangle(
                MeshData,
                BottomVertices[Side.StartCorner],
                TopVertices[Side.StartCorner],
                BottomVertices[Side.EndCorner],
                Side.Normal,
                SideSelector
            );
        }
        else
        {
            AddTriangle(
                MeshData,
                BottomVertices[Side.StartCorner],
                TopVertices[Side.EndCorner],
                BottomVertices[Side.EndCorner],
                Side.Normal,
                SideSelector
            );
        }

        ++GeneratedFaceCount;
    }

    AddFace(
        MeshData,
        BottomVertices[NegativeXPositiveY],
        BottomVertices[NegativeXNegativeY],
        BottomVertices[PositiveXNegativeY],
        BottomVertices[PositiveXPositiveY],
        FVector(0.0f, 0.0f, -1.0f),
        CubusBlockMesher::GetMaterialSelector(
            CubusBlockMesher::NegativeZ
        )
    );

    ++GeneratedFaceCount;
    return GeneratedFaceCount;
}

void FCubusBlockMesher::AddVoxelFace(
    FCubusMeshData& MeshData,
    const FVector& VoxelCentre,
    const float HalfVoxelSize,
    const int32 FaceIndex
)
{
    static const FVector FaceNormals[6] =
    {
        FVector(1.0f, 0.0f, 0.0f),
        FVector(-1.0f, 0.0f, 0.0f),
        FVector(0.0f, 1.0f, 0.0f),
        FVector(0.0f, -1.0f, 0.0f),
        FVector(0.0f, 0.0f, 1.0f),
        FVector(0.0f, 0.0f, -1.0f)
    };

    static const FVector FaceVertices[6][4] =
    {
        {
            FVector(1.0f, -1.0f, -1.0f),
            FVector(1.0f, -1.0f, 1.0f),
            FVector(1.0f, 1.0f, 1.0f),
            FVector(1.0f, 1.0f, -1.0f)
        },
        {
            FVector(-1.0f, 1.0f, -1.0f),
            FVector(-1.0f, 1.0f, 1.0f),
            FVector(-1.0f, -1.0f, 1.0f),
            FVector(-1.0f, -1.0f, -1.0f)
        },
        {
            FVector(1.0f, 1.0f, -1.0f),
            FVector(1.0f, 1.0f, 1.0f),
            FVector(-1.0f, 1.0f, 1.0f),
            FVector(-1.0f, 1.0f, -1.0f)
        },
        {
            FVector(-1.0f, -1.0f, -1.0f),
            FVector(-1.0f, -1.0f, 1.0f),
            FVector(1.0f, -1.0f, 1.0f),
            FVector(1.0f, -1.0f, -1.0f)
        },
        {
            FVector(-1.0f, -1.0f, 1.0f),
            FVector(-1.0f, 1.0f, 1.0f),
            FVector(1.0f, 1.0f, 1.0f),
            FVector(1.0f, -1.0f, 1.0f)
        },
        {
            FVector(-1.0f, 1.0f, -1.0f),
            FVector(-1.0f, -1.0f, -1.0f),
            FVector(1.0f, -1.0f, -1.0f),
            FVector(1.0f, 1.0f, -1.0f)
        }
    };

    check(FaceIndex >= 0 && FaceIndex < CubusBlockMesher::FaceCount);

    FVector Vertices[4];

    for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
    {
        Vertices[VertexIndex] =
            VoxelCentre +
            FaceVertices[FaceIndex][VertexIndex] * HalfVoxelSize;
    }

    AddFace(
        MeshData,
        Vertices[0],
        Vertices[1],
        Vertices[2],
        Vertices[3],
        FaceNormals[FaceIndex],
        CubusBlockMesher::GetMaterialSelector(FaceIndex)
    );
}

void FCubusBlockMesher::AddFace(
    FCubusMeshData& MeshData,
    const FVector& Vertex0,
    const FVector& Vertex1,
    const FVector& Vertex2,
    const FVector& Vertex3,
    const FVector& Normal,
    const float MaterialSelector
)
{
    const int32 FirstVertexIndex = MeshData.Vertices.Num();

    MeshData.Vertices.Add(Vertex0);
    MeshData.Vertices.Add(Vertex1);
    MeshData.Vertices.Add(Vertex2);
    MeshData.Vertices.Add(Vertex3);

    MeshData.Triangles.Append(
    {
        FirstVertexIndex + 0,
        FirstVertexIndex + 1,
        FirstVertexIndex + 2,
        FirstVertexIndex + 0,
        FirstVertexIndex + 2,
        FirstVertexIndex + 3
    });

    for (int32 Index = 0; Index < 4; ++Index)
    {
        MeshData.Normals.Add(Normal);
    }

    MeshData.UV0.Add(FVector2D(0.0f, 0.0f));
    MeshData.UV0.Add(FVector2D(1.0f, 0.0f));
    MeshData.UV0.Add(FVector2D(1.0f, 1.0f));
    MeshData.UV0.Add(FVector2D(0.0f, 1.0f));

    const FLinearColor FaceColor(
        1.0f,
        1.0f,
        1.0f,
        MaterialSelector
    );

    for (int32 Index = 0; Index < 4; ++Index)
    {
        MeshData.VertexColors.Add(FaceColor);
    }

    const FProcMeshTangent Tangent(
        (Vertex1 - Vertex0).GetSafeNormal(),
        false
    );

    for (int32 Index = 0; Index < 4; ++Index)
    {
        MeshData.Tangents.Add(Tangent);
    }
}

void FCubusBlockMesher::AddTriangle(
    FCubusMeshData& MeshData,
    const FVector& Vertex0,
    const FVector& Vertex1,
    const FVector& Vertex2,
    const FVector& Normal,
    const float MaterialSelector
)
{
    const int32 FirstVertexIndex = MeshData.Vertices.Num();

    MeshData.Vertices.Add(Vertex0);
    MeshData.Vertices.Add(Vertex1);
    MeshData.Vertices.Add(Vertex2);

    MeshData.Triangles.Append(
    {
        FirstVertexIndex + 0,
        FirstVertexIndex + 1,
        FirstVertexIndex + 2
    });

    for (int32 Index = 0; Index < 3; ++Index)
    {
        MeshData.Normals.Add(Normal);
    }

    MeshData.UV0.Add(FVector2D(0.0f, 1.0f));
    MeshData.UV0.Add(FVector2D(0.0f, 0.0f));
    MeshData.UV0.Add(FVector2D(1.0f, 0.0f));

    const FLinearColor FaceColor(
        1.0f,
        1.0f,
        1.0f,
        MaterialSelector
    );

    for (int32 Index = 0; Index < 3; ++Index)
    {
        MeshData.VertexColors.Add(FaceColor);
    }

    const FProcMeshTangent Tangent(
        (Vertex1 - Vertex0).GetSafeNormal(),
        false
    );

    for (int32 Index = 0; Index < 3; ++Index)
    {
        MeshData.Tangents.Add(Tangent);
    }
}
