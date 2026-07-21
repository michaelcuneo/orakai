#include "CubusCore/Meshing/CubusBlockMesher.h"

void FCubusBlockMesher::BuildSingleVoxel(
    const FCubusVoxel& Voxel,
    const float VoxelSize,
    FCubusMeshData& OutMeshData
)
{
    OutMeshData.Reset();

    if (!Voxel.IsSolid() || VoxelSize <= 0.0f)
    {
        return;
    }

    // One independent quad per face:
    // 6 faces × 4 vertices = 24 vertices.
    // 6 faces × 2 triangles × 3 indices = 36 indices.
    OutMeshData.Reserve(24, 36);

    const float HalfSize = VoxelSize * 0.5f;

    const float MinX = -HalfSize;
    const float MaxX = HalfSize;
    const float MinY = -HalfSize;
    const float MaxY = HalfSize;
    const float MinZ = -HalfSize;
    const float MaxZ = HalfSize;

    // Positive X
    AddFace(
        OutMeshData,
        FVector(MaxX, MinY, MinZ),
        FVector(MaxX, MaxY, MinZ),
        FVector(MaxX, MaxY, MaxZ),
        FVector(MaxX, MinY, MaxZ),
        FVector::XAxisVector
    );

    // Negative X
    AddFace(
        OutMeshData,
        FVector(MinX, MaxY, MinZ),
        FVector(MinX, MinY, MinZ),
        FVector(MinX, MinY, MaxZ),
        FVector(MinX, MaxY, MaxZ),
        -FVector::XAxisVector
    );

    // Positive Y
    AddFace(
        OutMeshData,
        FVector(MaxX, MaxY, MinZ),
        FVector(MinX, MaxY, MinZ),
        FVector(MinX, MaxY, MaxZ),
        FVector(MaxX, MaxY, MaxZ),
        FVector::YAxisVector
    );

    // Negative Y
    AddFace(
        OutMeshData,
        FVector(MinX, MinY, MinZ),
        FVector(MaxX, MinY, MinZ),
        FVector(MaxX, MinY, MaxZ),
        FVector(MinX, MinY, MaxZ),
        -FVector::YAxisVector
    );

    // Positive Z
    AddFace(
        OutMeshData,
        FVector(MinX, MinY, MaxZ),
        FVector(MaxX, MinY, MaxZ),
        FVector(MaxX, MaxY, MaxZ),
        FVector(MinX, MaxY, MaxZ),
        FVector::ZAxisVector
    );

    // Negative Z
    AddFace(
        OutMeshData,
        FVector(MinX, MaxY, MinZ),
        FVector(MaxX, MaxY, MinZ),
        FVector(MaxX, MinY, MinZ),
        FVector(MinX, MinY, MinZ),
        -FVector::ZAxisVector
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
    const int32 FirstVertexIndex = MeshData.Vertices.Num();

    MeshData.Vertices.Add(Vertex0);
    MeshData.Vertices.Add(Vertex1);
    MeshData.Vertices.Add(Vertex2);
    MeshData.Vertices.Add(Vertex3);

    // First triangle.
    MeshData.Triangles.Add(FirstVertexIndex + 0);
    MeshData.Triangles.Add(FirstVertexIndex + 1);
    MeshData.Triangles.Add(FirstVertexIndex + 2);

    // Second triangle.
    MeshData.Triangles.Add(FirstVertexIndex + 0);
    MeshData.Triangles.Add(FirstVertexIndex + 2);
    MeshData.Triangles.Add(FirstVertexIndex + 3);

    MeshData.Normals.Add(Normal);
    MeshData.Normals.Add(Normal);
    MeshData.Normals.Add(Normal);
    MeshData.Normals.Add(Normal);

    MeshData.UV0.Add(FVector2D(0.0f, 0.0f));
    MeshData.UV0.Add(FVector2D(1.0f, 0.0f));
    MeshData.UV0.Add(FVector2D(1.0f, 1.0f));
    MeshData.UV0.Add(FVector2D(0.0f, 1.0f));

    const FLinearColor DefaultVertexColor = FLinearColor::White;

    MeshData.VertexColors.Add(DefaultVertexColor);
    MeshData.VertexColors.Add(DefaultVertexColor);
    MeshData.VertexColors.Add(DefaultVertexColor);
    MeshData.VertexColors.Add(DefaultVertexColor);

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