#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Meshing/CubusMeshData.h"

class FCubusDensitySamplingBuffer;
class ICubusDensityField;

/** Extracts a smooth isosurface from a buffered Cubus density field. */
class ORAKAI_API FCubusDensityMesher
{
public:
    /**
     * Density terrain is submitted as one shared material section. Positive
     * keys remain reserved for ordinary block material IDs.
     */
    static constexpr int32 UnifiedDensityMaterialKey = -1;

    /**
     * Two 12-bit IDs are packed into each UV component. 4095 therefore is the
     * largest density material ID that remains exactly representable after
     * conversion to the procedural mesh GPU vertex format.
     */
    static constexpr int32 MaximumDensityMaterialId = 4095;
    static constexpr int32 MaterialIdPackingBase = 4096;

    static void BuildChunk(
        const FCubusDensitySamplingBuffer& DensityBuffer,
        float VoxelSize,
        float IsoLevel,
        TMap<int32, FCubusMeshData>& OutMaterialMeshes,
        int32& OutGeneratedTriangleCount
    );

    /**
     * Builds the same fixed-size canonical chunk at a finer sampling interval.
     * SubdivisionsPerVoxel=4 means 25 cm samples when canonical voxels are
     * 100 cm. Only coarse cells near a possible zero crossing are refined, so
     * the mesher does not allocate a 128/320-cubed dense volume.
     */
    static void BuildAdaptiveChunk(
        const ICubusDensityField& DensityField,
        const FIntVector& ChunkCoordinate,
        float CanonicalVoxelSize,
        int32 SubdivisionsPerVoxel,
        float IsoLevel,
        TMap<int32, FCubusMeshData>& OutMaterialMeshes,
        int32& OutGeneratedTriangleCount
    );
};
