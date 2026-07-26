#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Persistence/OrakaiPersistenceTypes.h"

/**
 * Transport-agnostic authoritative-store backend.
 *
 * The persistence subsystem talks only to this interface. A logging backend is
 * used by default so the game compiles and runs without the SpacetimeDB SDK; a
 * SpacetimeDB backend implementing this same interface is dropped in once the
 * SpacetimeDbSdk plugin and generated bindings are available.
 *
 * All methods are called on the game thread.
 */
class IOrakaiPersistenceBackend
{
public:
    virtual ~IOrakaiPersistenceBackend() = default;

    /** Human-readable backend name for logging. */
    virtual FString GetBackendName() const = 0;

    /** Begin establishing a connection to the authoritative store. */
    virtual void Connect() = 0;

    /** Tear down the connection. */
    virtual void Disconnect() = 0;

    /** True once the backend can accept records without buffering. */
    virtual bool IsConnected() const = 0;

    /** Pump the transport; called once per frame while active. */
    virtual void Tick(float DeltaSeconds) = 0;

    /** Publish the world seed / generation version singleton. */
    virtual void SetWorldConfig(int64 WorldSeed, uint32 GenerationVersion) = 0;

    /** Publish the local player's transform. */
    virtual void RecordPlayerCoordinate(const FOrakaiPlayerCoordinate& Coordinate) = 0;

    /** Publish a voxel delta. */
    virtual void RecordVoxelEdit(const FOrakaiVoxelEdit& Edit) = 0;

    /** Remove a voxel delta, restoring the generated voxel. */
    virtual void ClearVoxelEdit(
        const FIntVector& ChunkCoordinate,
        const FIntVector& LocalCoordinate
    ) = 0;

    /** Publish a foliage delta. */
    virtual void RecordFoliageEdit(const FOrakaiFoliageEdit& Edit) = 0;

    /** Remove a foliage delta, restoring the generated foliage. */
    virtual void ClearFoliageEdit(const FIntVector& WorldVoxel) = 0;
};
