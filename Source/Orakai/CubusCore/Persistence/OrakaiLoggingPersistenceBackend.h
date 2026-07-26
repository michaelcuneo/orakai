#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Persistence/OrakaiPersistenceBackend.h"

/**
 * Default backend used until the SpacetimeDB backend is available.
 *
 * It is always "connected", performs no network I/O, and logs each record at
 * Verbose so the capture pipeline can be validated end to end without the SDK.
 */
class FOrakaiLoggingPersistenceBackend final : public IOrakaiPersistenceBackend
{
public:
    virtual FString GetBackendName() const override;

    virtual void Connect() override;
    virtual void Disconnect() override;
    virtual bool IsConnected() const override;
    virtual void Tick(float DeltaSeconds) override;

    virtual void SetWorldConfig(int64 WorldSeed, uint32 GenerationVersion) override;
    virtual void RecordPlayerCoordinate(const FOrakaiPlayerCoordinate& Coordinate) override;
    virtual void RecordVoxelEdit(const FOrakaiVoxelEdit& Edit) override;
    virtual void ClearVoxelEdit(
        const FIntVector& ChunkCoordinate,
        const FIntVector& LocalCoordinate
    ) override;
    virtual void RecordFoliageEdit(const FOrakaiFoliageEdit& Edit) override;
    virtual void ClearFoliageEdit(const FIntVector& WorldVoxel) override;

private:
    bool bConnected = false;
};
