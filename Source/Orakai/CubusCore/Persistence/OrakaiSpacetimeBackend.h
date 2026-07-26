#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "CubusCore/Persistence/OrakaiPersistenceBackend.h"

class UOrakaiSpacetimeConnection;

/**
 * IOrakaiPersistenceBackend implementation backed by SpacetimeDB.
 *
 * Owns a UObject connection wrapper (kept alive from GC via TStrongObjectPtr)
 * and forwards each transport-agnostic call to the matching generated reducer.
 */
class FOrakaiSpacetimeBackend final : public IOrakaiPersistenceBackend
{
public:
    FOrakaiSpacetimeBackend(
        const FString& InUri,
        const FString& InDatabaseName,
        const FString& InTokenFilePath
    );
    virtual ~FOrakaiSpacetimeBackend() override;

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
    FString Uri;
    FString DatabaseName;
    FString TokenFilePath;

    TStrongObjectPtr<UOrakaiSpacetimeConnection> Connection;
};
