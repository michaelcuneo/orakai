#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Data/CubusVoxel.h"
#include "CubusCore/Meshing/CubusMeshData.h"

/**
 * Generates hard-edged block voxel geometry.
 *
 * This class will later accept chunk voxel data and emit only exposed faces.
 * The first implementation generates one complete voxel.
 */
class ORAKAI_API FCubusBlockMesher
{
public:
    /**
     * Builds a single block voxel centred on the local origin.
     *
     * @param Voxel       Voxel data to render.
     * @param VoxelSize   Width, depth and height of the voxel in Unreal units.
     * @param OutMeshData Generated mesh data.
     */
    static void BuildSingleVoxel(
        const FCubusVoxel& Voxel,
        float VoxelSize,
        FCubusMeshData& OutMeshData
    );

private:
    /**
     * Adds one quad consisting of four unique vertices and two triangles.
     */
    static void AddFace(
        FCubusMeshData& MeshData,
        const FVector& Vertex0,
        const FVector& Vertex1,
        const FVector& Vertex2,
        const FVector& Vertex3,
        const FVector& Normal
    );
};