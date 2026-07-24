#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;

/**
 * Metadata that makes a locally cached chunk safe to reuse.
 *
 * The local store is a disposable materialized cache. SpaceTimeDB remains
 * authoritative for shared voxel deltas; a cache miss or metadata mismatch
 * simply causes deterministic regeneration from the world seed.
 */
struct ORAKAI_API FCubusChunkStoreContext
{
    int64 WorldSeed = 1;
    uint32 GenerationVersion = 1;
};

/**
 * Versioned binary storage for generated Cubus chunks.
 */
class ORAKAI_API FCubusChunkStore
{
public:
    static constexpr uint32 CurrentFormatVersion = 1;

    static bool SaveChunk(
        const FCubusBlockChunkData& Chunk,
        const FCubusChunkStoreContext& Context
    );

    static bool LoadChunk(
        FCubusBlockChunkData& Chunk,
        const FCubusChunkStoreContext& Context
    );

    static bool HasChunk(
        const FIntVector& ChunkCoordinate,
        const FCubusChunkStoreContext& Context
    );

    static bool DeleteChunk(
        const FIntVector& ChunkCoordinate,
        const FCubusChunkStoreContext& Context
    );

    static FString GetChunkPath(
        const FIntVector& ChunkCoordinate,
        const FCubusChunkStoreContext& Context
    );

    static FString GetWorldStoreDirectory(
        const FCubusChunkStoreContext& Context
    );
};
