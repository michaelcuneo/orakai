#include "CubusCore/Meshing/CubusBlockMesher.h"

#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Data/CubusVoxelVolume.h"
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
}

void FCubusBlockMesher::BuildSingleVoxel(
    const FCubusVoxel& Voxel,
    const float VoxelSize,
    FCubusMeshData& OutMeshData
)
{
    OutMeshData.Reset();

    if (Voxel.IsEmpty() || VoxelSize <= 0.0f)
    {
        return;
    }

    OutMeshData.Reserve(24, 36);

    const float HalfVoxelSize = VoxelSize * 0.5f;

    for (
        int32 FaceIndex = 0;
        FaceIndex < CubusBlockMesher::FaceCount;
        ++FaceIndex
    )
    {
        AddVoxelFace(
            OutMeshData,
            FVector::ZeroVector,
            HalfVoxelSize,
            FaceIndex
        );
    }
}

void FCubusBlockMesher::BuildVolume(
    const FCubusVoxelVolume& Volume,
    const UCubusMaterialRegistry* MaterialRegistry,
    const float VoxelSize,
    FCubusMaterialMeshMap& OutMaterialMeshes,
    int32& OutGeneratedFaceCount
)
{
    OutMaterialMeshes.Reset();
    OutGeneratedFaceCount = 0;

    if (VoxelSize <= 0.0f)
    {
        return;
    }

    const FIntVector Dimensions =
        Volume.GetDimensions();

    const float HalfVoxelSize =
        VoxelSize * 0.5f;

    const FVector VolumeSize(
        static_cast<float>(Dimensions.X) * VoxelSize,
        static_cast<float>(Dimensions.Y) * VoxelSize,
        static_cast<float>(Dimensions.Z) * VoxelSize
    );

    const FVector VolumeMinimum =
        VolumeSize * -0.5f;

    for (int32 Z = 0; Z < Dimensions.Z; ++Z)
    {
        for (int32 Y = 0; Y < Dimensions.Y; ++Y)
        {
            for (int32 X = 0; X < Dimensions.X; ++X)
            {
                const FCubusVoxel* CurrentVoxel =
                    Volume.GetVoxel(X, Y, Z);

                if (
                    CurrentVoxel == nullptr ||
                    CurrentVoxel->IsEmpty()
                )
                {
                    continue;
                }

                const int32 CurrentMaterialId =
                    CurrentVoxel->MaterialId;

                const bool bRenderableSolid =
                    MaterialRegistry != nullptr
                        ? MaterialRegistry->IsRenderableSolid(
                            CurrentMaterialId
                        )
                        : true;

                if (!bRenderableSolid)
                {
                    continue;
                }

                FCubusMeshData& MaterialMesh =
                    OutMaterialMeshes.FindOrAdd(
                        CurrentMaterialId
                    );

                const FVector VoxelCentre =
                    VolumeMinimum +
                    FVector(
                        (
                            static_cast<float>(X) +
                            0.5f
                        ) * VoxelSize,
                        (
                            static_cast<float>(Y) +
                            0.5f
                        ) * VoxelSize,
                        (
                            static_cast<float>(Z) +
                            0.5f
                        ) * VoxelSize
                    );

                for (
                    int32 FaceIndex = 0;
                    FaceIndex < CubusBlockMesher::FaceCount;
                    ++FaceIndex
                )
                {
                    const FIntVector NeighbourPosition =
                        FIntVector(X, Y, Z) +
                        CubusBlockMesher::NeighbourOffsets[
                            FaceIndex
                        ];

                    const FCubusVoxel* NeighbourVoxel =
                        Volume.GetVoxel(
                            NeighbourPosition
                        );

                    bool bNeighbourOccludesFace = false;

                    if (
                        NeighbourVoxel != nullptr &&
                        !NeighbourVoxel->IsEmpty()
                    )
                    {
                        bNeighbourOccludesFace =
                            MaterialRegistry != nullptr
                                ? MaterialRegistry
                                    ->OccludesBlockFaces(
                                        NeighbourVoxel
                                            ->MaterialId
                                    )
                                : true;
                    }

                    if (bNeighbourOccludesFace)
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

void FCubusBlockMesher::BuildChunk(
    const FCubusBlockChunkNeighborhood& Neighborhood,
    const UCubusMaterialRegistry* MaterialRegistry,
    const float VoxelSize,
    FCubusMaterialMeshMap& OutMaterialMeshes,
    int32& OutGeneratedFaceCount
)
{
    OutMaterialMeshes.Reset();
    OutGeneratedFaceCount = 0;

    if (
        Neighborhood.Centre == nullptr ||
        VoxelSize <= 0.0f
    )
    {
        return;
    }

    const FCubusBlockChunkData& Chunk =
        *Neighborhood.Centre;

    const float HalfVoxelSize =
        VoxelSize * 0.5f;

    const float ChunkWorldSize =
        static_cast<float>(Cubus::ChunkSize) *
        VoxelSize;

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

                if (
                    CurrentVoxel == nullptr ||
                    CurrentVoxel->IsEmpty()
                )
                {
                    continue;
                }

                const int32 CurrentMaterialId =
                    CurrentVoxel->MaterialId;

                const bool bCurrentIsWater =
                    CurrentVoxel->IsWater();

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
                    OutMaterialMeshes.FindOrAdd(
                        CurrentMaterialId
                    );

                const FVector VoxelCentre =
                    ChunkMinimum +
                    FVector(
                        (
                            static_cast<float>(X) +
                            0.5f
                        ) * VoxelSize,
                        (
                            static_cast<float>(Y) +
                            0.5f
                        ) * VoxelSize,
                        (
                            static_cast<float>(Z) +
                            0.5f
                        ) * VoxelSize
                    );

                for (
                    int32 FaceIndex = 0;
                    FaceIndex < CubusBlockMesher::FaceCount;
                    ++FaceIndex
                )
                {
                    const FIntVector NeighbourPosition =
                        FIntVector(X, Y, Z) +
                        CubusBlockMesher::NeighbourOffsets[
                            FaceIndex
                        ];

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
                            /*
                            * Water faces are hidden against both water and solid terrain.
                            *
                            * This removes:
                            * - internal water-to-water faces;
                            * - water faces buried against terrain.
                            *
                            * The terrain voxel on the opposite side will render its own face.
                            */
                            bRenderFace = false;
                        }
                        else if (bNeighbourIsWater)
                        {
                            /*
                            * Water does not occlude solid terrain.
                            *
                            * This keeps shoreline walls, lake beds and underwater terrain
                            * visible through a translucent water material.
                            */
                            bRenderFace = true;
                        }
                        else
                        {
                            /*
                            * Ordinary solid-to-solid behaviour remains controlled by the
                            * material registry.
                            */
                            const bool bNeighbourOccludesFace =
                                MaterialRegistry != nullptr
                                    ? MaterialRegistry
                                        ->OccludesBlockFaces(
                                            NeighbourVoxel->MaterialId
                                        )
                                    : true;

                            bRenderFace =
                                !bNeighbourOccludesFace;
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
        // Positive X
        {
            FVector(1.0f, -1.0f, -1.0f),
            FVector(1.0f, -1.0f, 1.0f),
            FVector(1.0f, 1.0f, 1.0f),
            FVector(1.0f, 1.0f, -1.0f)
        },

        // Negative X
        {
            FVector(-1.0f, 1.0f, -1.0f),
            FVector(-1.0f, 1.0f, 1.0f),
            FVector(-1.0f, -1.0f, 1.0f),
            FVector(-1.0f, -1.0f, -1.0f)
        },

        // Positive Y
        {
            FVector(1.0f, 1.0f, -1.0f),
            FVector(1.0f, 1.0f, 1.0f),
            FVector(-1.0f, 1.0f, 1.0f),
            FVector(-1.0f, 1.0f, -1.0f)
        },

        // Negative Y
        {
            FVector(-1.0f, -1.0f, -1.0f),
            FVector(-1.0f, -1.0f, 1.0f),
            FVector(1.0f, -1.0f, 1.0f),
            FVector(1.0f, -1.0f, -1.0f)
        },

        // Positive Z
        {
            FVector(-1.0f, -1.0f, 1.0f),
            FVector(-1.0f, 1.0f, 1.0f),
            FVector(1.0f, 1.0f, 1.0f),
            FVector(1.0f, -1.0f, 1.0f)
        },

        // Negative Z
        {
            FVector(-1.0f, 1.0f, -1.0f),
            FVector(-1.0f, -1.0f, -1.0f),
            FVector(1.0f, -1.0f, -1.0f),
            FVector(1.0f, 1.0f, -1.0f)
        }
    };

    check(
        FaceIndex >= 0 &&
        FaceIndex < CubusBlockMesher::FaceCount
    );

    FVector Vertices[4];

    for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
    {
        Vertices[VertexIndex] =
            VoxelCentre +
            FaceVertices[FaceIndex][VertexIndex] *
            HalfVoxelSize;
    }

    AddFace(
        MeshData,
        Vertices[0],
        Vertices[1],
        Vertices[2],
        Vertices[3],
        FaceNormals[FaceIndex]
    );
}

void FCubusBlockMesher::AddFace(
    FCubusMeshData& MeshData,
    const FVector& Vertex0,
    const FVector& Vertex1,
    const FVector& Vertex2,
    const FVector& Vertex3,
    const FVector& Normal
)
{
    const int32 FirstVertexIndex =
        MeshData.Vertices.Num();

    MeshData.Vertices.Add(Vertex0);
    MeshData.Vertices.Add(Vertex1);
    MeshData.Vertices.Add(Vertex2);
    MeshData.Vertices.Add(Vertex3);

    // Unreal clockwise front-facing winding.
    MeshData.Triangles.Append(
    {
        FirstVertexIndex + 0,
        FirstVertexIndex + 1,
        FirstVertexIndex + 2,

        FirstVertexIndex + 0,
        FirstVertexIndex + 2,
        FirstVertexIndex + 3
    });

    MeshData.Normals.Add(Normal);
    MeshData.Normals.Add(Normal);
    MeshData.Normals.Add(Normal);
    MeshData.Normals.Add(Normal);

    MeshData.UV0.Add(FVector2D(0.0f, 0.0f));
    MeshData.UV0.Add(FVector2D(1.0f, 0.0f));
    MeshData.UV0.Add(FVector2D(1.0f, 1.0f));
    MeshData.UV0.Add(FVector2D(0.0f, 1.0f));

    MeshData.VertexColors.Add(FLinearColor::White);
    MeshData.VertexColors.Add(FLinearColor::White);
    MeshData.VertexColors.Add(FLinearColor::White);
    MeshData.VertexColors.Add(FLinearColor::White);

    const FVector TangentDirection =
        (Vertex1 - Vertex0).GetSafeNormal();

    const FProcMeshTangent Tangent(
        TangentDirection,
        false
    );

    MeshData.Tangents.Add(Tangent);
    MeshData.Tangents.Add(Tangent);
    MeshData.Tangents.Add(Tangent);
    MeshData.Tangents.Add(Tangent);
}