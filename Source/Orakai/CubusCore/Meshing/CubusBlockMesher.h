#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Meshing/CubusMeshData.h"

class UCubusMaterialRegistry;
struct FCubusBlockChunkNeighborhood;

using FCubusMaterialMeshMap = TMap<int32, FCubusMeshData>;

struct FCubusBlockChunkMeshSnapshot
{
	FIntVector				 ChunkCoordinate = FIntVector::ZeroValue;
	TArray<FCubusBlockVoxel> Voxels;

	const FCubusBlockVoxel* GetVoxel(int32 X, int32 Y, int32 Z) const;
};

struct FCubusBlockNeighborhoodMeshSnapshot
{
	TSharedPtr<const FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe> Centre;
	TSharedPtr<const FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe> PositiveX;
	TSharedPtr<const FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe> NegativeX;
	TSharedPtr<const FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe> PositiveY;
	TSharedPtr<const FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe> NegativeY;
	TSharedPtr<const FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe> PositiveZ;
	TSharedPtr<const FCubusBlockChunkMeshSnapshot, ESPMode::ThreadSafe> NegativeZ;

	const FCubusBlockVoxel* GetVoxel(int32 X, int32 Y, int32 Z) const;
};

struct FCubusBlockMaterialMeshSnapshot
{
	TSet<int32> RenderableSolidMaterialIds;

	bool IsRenderableSolid(const FCubusBlockVoxel* Voxel) const;
};

/**
 * Extracts a coarse geological surface from authoritative block occupancy.
 *
 * Storage, edits and materials remain block based. Rendering converts each
 * solid block centre to a positive density sample and each empty or liquid
 * block centre to a negative sample, then runs the shared density mesher at
 * exactly one sample per canonical voxel.
 */
class ORAKAI_API FCubusBlockMesher
{
public:
	static void BuildChunk(const FCubusBlockNeighborhoodMeshSnapshot& Neighborhood, const FCubusBlockMaterialMeshSnapshot& Materials,
						   float VoxelSize, FCubusMaterialMeshMap& OutMaterialMeshes, int32& OutGeneratedFaceCount);

	static void BuildChunk(const FCubusBlockChunkNeighborhood& Neighborhood, const UCubusMaterialRegistry* MaterialRegistry,
						   float VoxelSize, FCubusMaterialMeshMap& OutMaterialMeshes, int32& OutGeneratedFaceCount);
};
