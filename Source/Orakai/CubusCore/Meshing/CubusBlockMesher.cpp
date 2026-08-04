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
        FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
        FIntVector(0, 1, 0), FIntVector(0, -1, 0),
        FIntVector(0, 0, 1), FIntVector(0, 0, -1)
    };

    float GetMaterialSelector(const int32 FaceIndex)
    {
        if (FaceIndex == PositiveZ) return 0.5f;
        if (FaceIndex == NegativeZ) return 1.0f;
        return 0.0f;
    }

    bool ShouldRenderSolidFace(
        const FCubusBlockVoxel* NeighbourVoxel,
        const UCubusMaterialRegistry* MaterialRegistry
    )
    {
        if (NeighbourVoxel == nullptr || NeighbourVoxel->IsEmpty()) return true;
        if (NeighbourVoxel->IsWater()) return true;

        const bool bNeighbourOccludesFace =
            MaterialRegistry != nullptr
                ? MaterialRegistry->OccludesBlockFaces(NeighbourVoxel->MaterialId)
                : true;

        return !bNeighbourOccludesFace;
    }

    void ResolveRampAxes(
        const ECubusBlockSurfaceShape Shape,
        FVector& OutUphillDirection,
        FVector& OutSideDirection,
        FIntVector& OutNegativeSideOffset,
        FIntVector& OutPositiveSideOffset
    )
    {
        switch (Shape)
        {
            case ECubusBlockSurfaceShape::RampPositiveX:
                OutUphillDirection = FVector(-1.0f, 0.0f, 0.0f);
                OutSideDirection = FVector(0.0f, 1.0f, 0.0f);
                OutNegativeSideOffset = FIntVector(0, -1, 0);
                OutPositiveSideOffset = FIntVector(0, 1, 0);
                break;
            case ECubusBlockSurfaceShape::RampNegativeX:
                OutUphillDirection = FVector(1.0f, 0.0f, 0.0f);
                OutSideDirection = FVector(0.0f, 1.0f, 0.0f);
                OutNegativeSideOffset = FIntVector(0, -1, 0);
                OutPositiveSideOffset = FIntVector(0, 1, 0);
                break;
            case ECubusBlockSurfaceShape::RampPositiveY:
                OutUphillDirection = FVector(0.0f, -1.0f, 0.0f);
                OutSideDirection = FVector(1.0f, 0.0f, 0.0f);
                OutNegativeSideOffset = FIntVector(-1, 0, 0);
                OutPositiveSideOffset = FIntVector(1, 0, 0);
                break;
            case ECubusBlockSurfaceShape::RampNegativeY:
                OutUphillDirection = FVector(0.0f, 1.0f, 0.0f);
                OutSideDirection = FVector(1.0f, 0.0f, 0.0f);
                OutNegativeSideOffset = FIntVector(-1, 0, 0);
                OutPositiveSideOffset = FIntVector(1, 0, 0);
                break;
            default:
                OutUphillDirection = FVector::ZeroVector;
                OutSideDirection = FVector::ZeroVector;
                OutNegativeSideOffset = FIntVector::ZeroValue;
                OutPositiveSideOffset = FIntVector::ZeroValue;
                break;
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
    OutMaterialMeshes.Reserve(MaterialRegistry != nullptr ? MaterialRegistry->Materials.Num() : 1);
    OutGeneratedFaceCount = 0;

    if (Neighborhood.Centre == nullptr || VoxelSize <= 0.0f) return;

    const FCubusBlockChunkData& Chunk = *Neighborhood.Centre;
    const float HalfVoxelSize = VoxelSize * 0.5f;
    const float ChunkWorldSize = static_cast<float>(Cubus::ChunkSize) * VoxelSize;
    const FVector ChunkMinimum(ChunkWorldSize * -0.5f, ChunkWorldSize * -0.5f, ChunkWorldSize * -0.5f);

    for (int32 Z = 0; Z < Cubus::ChunkSize; ++Z)
    {
        for (int32 Y = 0; Y < Cubus::ChunkSize; ++Y)
        {
            for (int32 X = 0; X < Cubus::ChunkSize; ++X)
            {
                const FCubusBlockVoxel* CurrentVoxel = Chunk.GetVoxel(X, Y, Z);
                if (CurrentVoxel == nullptr || CurrentVoxel->IsEmpty()) continue;

                const int32 CurrentMaterialId = CurrentVoxel->MaterialId;
                const bool bCurrentIsWater = CurrentVoxel->IsWater();
                const bool bCurrentIsRenderable =
                    bCurrentIsWater ||
                    (MaterialRegistry != nullptr
                        ? MaterialRegistry->IsRenderableSolid(CurrentMaterialId)
                        : true);

                if (!bCurrentIsRenderable) continue;

                FCubusMeshData& MaterialMesh = OutMaterialMeshes.FindOrAdd(CurrentMaterialId);
                const FVector VoxelCentre = ChunkMinimum + FVector(
                    (static_cast<float>(X) + 0.5f) * VoxelSize,
                    (static_cast<float>(Y) + 0.5f) * VoxelSize,
                    (static_cast<float>(Z) + 0.5f) * VoxelSize);

                if (!bCurrentIsWater)
                {
                    const FCubusBlockSurfaceClassification Classification =
                        FCubusBlockSurfaceClassifier::Classify(
                            Neighborhood, MaterialRegistry, X, Y, Z);

                    if (Classification.IsRamp())
                    {
                        OutGeneratedFaceCount += AddRamp(
                            MaterialMesh, Neighborhood, MaterialRegistry,
                            VoxelCentre, HalfVoxelSize, X, Y, Z,
                            Classification.Shape);
                        continue;
                    }
                }

                for (int32 FaceIndex = 0; FaceIndex < CubusBlockMesher::FaceCount; ++FaceIndex)
                {
                    const FIntVector NeighbourPosition =
                        FIntVector(X, Y, Z) + CubusBlockMesher::NeighbourOffsets[FaceIndex];
                    const FCubusBlockVoxel* NeighbourVoxel = Neighborhood.GetVoxel(
                        NeighbourPosition.X, NeighbourPosition.Y, NeighbourPosition.Z);

                    bool bRenderFace = true;

                    if (NeighbourVoxel != nullptr && !NeighbourVoxel->IsEmpty())
                    {
                        const bool bNeighbourIsWater = NeighbourVoxel->IsWater();

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
                                    ? MaterialRegistry->OccludesBlockFaces(NeighbourVoxel->MaterialId)
                                    : true;

                            bRenderFace = !bNeighbourOccludesFace;

                            // A derived ramp does not occupy its complete cube.
                            // Preserve horizontal square faces on neighbouring
                            // cube geometry so the ramp's empty triangular half
                            // cannot expose the interior of the terrain.
                            if (!bRenderFace && FaceIndex <= CubusBlockMesher::NegativeY)
                            {
                                const FCubusBlockSurfaceClassification NeighbourClassification =
                                    FCubusBlockSurfaceClassifier::Classify(
                                        Neighborhood,
                                        MaterialRegistry,
                                        NeighbourPosition.X,
                                        NeighbourPosition.Y,
                                        NeighbourPosition.Z);

                                if (NeighbourClassification.IsRamp())
                                {
                                    bRenderFace = true;
                                }
                            }
                        }
                    }

                    if (!bRenderFace) continue;

                    AddVoxelFace(MaterialMesh, VoxelCentre, HalfVoxelSize, FaceIndex);
                    ++OutGeneratedFaceCount;
                }
            }
        }
    }
}

int32 FCubusBlockMesher::AddRamp(
    FCubusMeshData& MeshData,
    const FCubusBlockChunkNeighborhood& Neighborhood,
    const UCubusMaterialRegistry* MaterialRegistry,
    const FVector& VoxelCentre,
    const float HalfVoxelSize,
    const int32 X,
    const int32 Y,
    const int32 Z,
    const ECubusBlockSurfaceShape Shape
)
{
    FVector UphillDirection;
    FVector SideDirection;
    FIntVector NegativeSideOffset;
    FIntVector PositiveSideOffset;
    CubusBlockMesher::ResolveRampAxes(
        Shape, UphillDirection, SideDirection,
        NegativeSideOffset, PositiveSideOffset);

    if (UphillDirection.IsNearlyZero() || SideDirection.IsNearlyZero()) return 0;

    const FVector UpDirection = FVector::UpVector;
    const FVector HighNegativeSide = VoxelCentre + UphillDirection * HalfVoxelSize - SideDirection * HalfVoxelSize + UpDirection * HalfVoxelSize;
    const FVector HighPositiveSide = VoxelCentre + UphillDirection * HalfVoxelSize + SideDirection * HalfVoxelSize + UpDirection * HalfVoxelSize;
    const FVector LowNegativeSide = VoxelCentre - UphillDirection * HalfVoxelSize - SideDirection * HalfVoxelSize - UpDirection * HalfVoxelSize;
    const FVector LowPositiveSide = VoxelCentre - UphillDirection * HalfVoxelSize + SideDirection * HalfVoxelSize - UpDirection * HalfVoxelSize;
    const FVector BottomHighNegativeSide = VoxelCentre + UphillDirection * HalfVoxelSize - SideDirection * HalfVoxelSize - UpDirection * HalfVoxelSize;
    const FVector BottomHighPositiveSide = VoxelCentre + UphillDirection * HalfVoxelSize + SideDirection * HalfVoxelSize - UpDirection * HalfVoxelSize;
    const FVector SlopeNormal = (-UphillDirection + UpDirection).GetSafeNormal();

    AddFace(
        MeshData, HighNegativeSide, LowNegativeSide,
        LowPositiveSide, HighPositiveSide, SlopeNormal,
        CubusBlockMesher::GetMaterialSelector(CubusBlockMesher::PositiveZ));

    int32 GeneratedFaceCount = 1;

    const FCubusBlockVoxel* NegativeSideNeighbour = Neighborhood.GetVoxel(
        X + NegativeSideOffset.X, Y + NegativeSideOffset.Y, Z + NegativeSideOffset.Z);
    if (CubusBlockMesher::ShouldRenderSolidFace(NegativeSideNeighbour, MaterialRegistry))
    {
        AddTriangle(
            MeshData, HighNegativeSide, BottomHighNegativeSide,
            LowNegativeSide, -SideDirection,
            CubusBlockMesher::GetMaterialSelector(CubusBlockMesher::PositiveX));
        ++GeneratedFaceCount;
    }

    const FCubusBlockVoxel* PositiveSideNeighbour = Neighborhood.GetVoxel(
        X + PositiveSideOffset.X, Y + PositiveSideOffset.Y, Z + PositiveSideOffset.Z);
    if (CubusBlockMesher::ShouldRenderSolidFace(PositiveSideNeighbour, MaterialRegistry))
    {
        AddTriangle(
            MeshData, HighPositiveSide, LowPositiveSide,
            BottomHighPositiveSide, SideDirection,
            CubusBlockMesher::GetMaterialSelector(CubusBlockMesher::PositiveX));
        ++GeneratedFaceCount;
    }

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
        FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f),
        FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f),
        FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 0.0f, -1.0f)
    };

    static const FVector FaceVertices[6][4] =
    {
        { FVector(1,-1,-1), FVector(1,-1,1), FVector(1,1,1), FVector(1,1,-1) },
        { FVector(-1,1,-1), FVector(-1,1,1), FVector(-1,-1,1), FVector(-1,-1,-1) },
        { FVector(1,1,-1), FVector(1,1,1), FVector(-1,1,1), FVector(-1,1,-1) },
        { FVector(-1,-1,-1), FVector(-1,-1,1), FVector(1,-1,1), FVector(1,-1,-1) },
        { FVector(-1,-1,1), FVector(-1,1,1), FVector(1,1,1), FVector(1,-1,1) },
        { FVector(-1,1,-1), FVector(-1,-1,-1), FVector(1,-1,-1), FVector(1,1,-1) }
    };

    check(FaceIndex >= 0 && FaceIndex < CubusBlockMesher::FaceCount);
    FVector Vertices[4];
    for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
    {
        Vertices[VertexIndex] = VoxelCentre + FaceVertices[FaceIndex][VertexIndex] * HalfVoxelSize;
    }

    AddFace(
        MeshData, Vertices[0], Vertices[1], Vertices[2], Vertices[3],
        FaceNormals[FaceIndex], CubusBlockMesher::GetMaterialSelector(FaceIndex));
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
    MeshData.Vertices.Append({Vertex0, Vertex1, Vertex2, Vertex3});
    MeshData.Triangles.Append({
        FirstVertexIndex, FirstVertexIndex + 1, FirstVertexIndex + 2,
        FirstVertexIndex, FirstVertexIndex + 2, FirstVertexIndex + 3});
    MeshData.Normals.Append({Normal, Normal, Normal, Normal});
    MeshData.UV0.Append({
        FVector2D(0,0), FVector2D(1,0),
        FVector2D(1,1), FVector2D(0,1)});

    const FLinearColor FaceColor(1,1,1,MaterialSelector);
    MeshData.VertexColors.Append({FaceColor, FaceColor, FaceColor, FaceColor});
    const FProcMeshTangent Tangent((Vertex1 - Vertex0).GetSafeNormal(), false);
    MeshData.Tangents.Append({Tangent, Tangent, Tangent, Tangent});
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
    MeshData.Vertices.Append({Vertex0, Vertex1, Vertex2});
    MeshData.Triangles.Append({FirstVertexIndex, FirstVertexIndex + 1, FirstVertexIndex + 2});
    MeshData.Normals.Append({Normal, Normal, Normal});
    MeshData.UV0.Append({FVector2D(0,1), FVector2D(0,0), FVector2D(1,0)});

    const FLinearColor FaceColor(1,1,1,MaterialSelector);
    MeshData.VertexColors.Append({FaceColor, FaceColor, FaceColor});
    const FProcMeshTangent Tangent((Vertex1 - Vertex0).GetSafeNormal(), false);
    MeshData.Tangents.Append({Tangent, Tangent, Tangent});
}
