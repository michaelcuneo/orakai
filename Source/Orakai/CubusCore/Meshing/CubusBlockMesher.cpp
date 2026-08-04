#include "CubusCore/Meshing/CubusBlockMesher.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusBlockChunkNeighborhood.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusMaterialRegistry.h"
#include "CubusCore/Generation/CubusBlockDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"

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

    const FIntVector ChunkCoordinate =
        Neighborhood.Centre->GetChunkCoordinate();

    const FIntVector GlobalVoxelOrigin =
        ChunkCoordinate * Cubus::ChunkSize;

    const FCubusBlockDensityField BlockDensityField(
        [
            &Neighborhood,
            MaterialRegistry,
            GlobalVoxelOrigin
        ](const FIntVector& GlobalVoxelCoordinate)
        {
            const FIntVector LocalVoxelCoordinate =
                GlobalVoxelCoordinate -
                GlobalVoxelOrigin;

            const FCubusBlockVoxel* SourceVoxel =
                Neighborhood.GetVoxel(
                    LocalVoxelCoordinate.X,
                    LocalVoxelCoordinate.Y,
                    LocalVoxelCoordinate.Z
                );

            if (
                SourceVoxel == nullptr ||
                SourceVoxel->IsEmpty() ||
                SourceVoxel->IsWater()
            )
            {
                return FCubusBlockVoxel();
            }

            if (
                MaterialRegistry != nullptr &&
                !MaterialRegistry->IsRenderableSolid(
                    SourceVoxel->MaterialId
                )
            )
            {
                return FCubusBlockVoxel();
            }

            return *SourceVoxel;
        },
        true,
        1.0f
    );

    FCubusDensitySamplingBuffer SamplingBuffer;
    SamplingBuffer.Build(
        ChunkCoordinate,
        BlockDensityField
    );

    int32 GeneratedTriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        SamplingBuffer,
        VoxelSize,
        0.0f,
        OutMaterialMeshes,
        GeneratedTriangleCount
    );

    // Kept for compatibility with existing block diagnostics. Geological
    // block mode now reports generated triangles rather than cube faces.
    OutGeneratedFaceCount = GeneratedTriangleCount;
}
