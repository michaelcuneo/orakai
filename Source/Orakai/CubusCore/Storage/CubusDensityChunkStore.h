#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Meshing/CubusMeshData.h"

class FCubusDensitySamplingBuffer;

/**
 * Identity of one generated density baseline on local disk.
 *
 * Player edits are intentionally excluded: the cached baseline is deterministic
 * generated world state, while sparse edits remain authoritative in persistence
 * and are applied after the baseline is loaded.
 */
struct ORAKAI_API FCubusDensityChunkStoreContext
{
	int64  WorldSeed				= 1;
	uint32 GenerationVersion		= 1;
	float  VoxelSize				= 100.0f;
	int32  SubdivisionsPerVoxel		= 1;
	uint32 MaterialRulesFingerprint = 0;
};

/** Versioned binary storage for canonical density sampling buffers. */
class ORAKAI_API FCubusDensityChunkStore
{
public:
	// Format 2 intentionally invalidates all previously written .cubusd files.
	// Generation v21 is the first namespace written with this format.
	static constexpr uint32 CurrentFormatVersion	 = 2;
	static constexpr uint32 CurrentMeshFormatVersion = 2;

	static bool SaveMesh(const FIntVector& ChunkCoordinate, const FCubusDensityChunkStoreContext& Context, uint32 TransitionSignature,
						 const TMap<int32, FCubusMeshData>& MaterialMeshes, int32 GeneratedTriangleCount);

	static bool LoadMesh(const FIntVector& ChunkCoordinate, const FCubusDensityChunkStoreContext& Context, uint32 TransitionSignature,
						 TMap<int32, FCubusMeshData>& OutMaterialMeshes, int32& OutGeneratedTriangleCount);

	static bool SaveBuffer(const FCubusDensitySamplingBuffer& Buffer, const FCubusDensityChunkStoreContext& Context);

	static bool LoadBuffer(const FIntVector& ChunkCoordinate, const FCubusDensityChunkStoreContext& Context,
						   FCubusDensitySamplingBuffer& OutBuffer);

	static bool HasBuffer(const FIntVector& ChunkCoordinate, const FCubusDensityChunkStoreContext& Context);

	static bool DeleteBuffer(const FIntVector& ChunkCoordinate, const FCubusDensityChunkStoreContext& Context);

	static FString GetBufferPath(const FIntVector& ChunkCoordinate, const FCubusDensityChunkStoreContext& Context);

	static FString GetDensityStoreDirectory(const FCubusDensityChunkStoreContext& Context);
};
