#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Meshing/CubusMeshData.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Meshing/CubusBlockSurfaceShape.h"

class FCubusBlockChunkData;
class UCubusMaterialRegistry;
struct FCubusBlockChunkNeighborhood;

/**
 * Generated mesh data grouped by voxel material ID.
 */
using FCubusMaterialMeshMap = TMap<int32, FCubusMeshData>;

/**
 * Generates block voxel geometry, including neighbour-derived geological
 * surface shapes for exposed natural terrain.
 */
class ORAKAI_API FCubusBlockMesher
{
public:
    static void BuildChunk(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        float VoxelSize,
        FCubusMaterialMeshMap& OutMaterialMeshes,
        int32& OutGeneratedFaceCount
    );

private:
    static void AddFace(
        FCubusMeshData& MeshData,
        const FVector& Vertex0,
        const FVector& Vertex1,
        const FVector& Vertex2,
        const FVector& Vertex3,
        const FVector& Normal,
        float MaterialSelector
    );

    static void AddTriangle(
        FCubusMeshData& MeshData,
        const FVector& Vertex0,
        const FVector& Vertex1,
        const FVector& Vertex2,
        const FVector& Normal,
        float MaterialSelector
    );

    static void AddVoxelFace(
        FCubusMeshData& MeshData,
        const FVector& VoxelCentre,
        float HalfVoxelSize,
        int32 FaceIndex
    );

    static int32 AddRamp(
        FCubusMeshData& MeshData,
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        const FVector& VoxelCentre,
        float HalfVoxelSize,
        int32 X,
        int32 Y,
        int32 Z,
        ECubusBlockSurfaceShape Shape
    );
};