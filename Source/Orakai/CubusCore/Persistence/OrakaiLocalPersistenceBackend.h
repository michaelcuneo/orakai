#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Persistence/OrakaiPersistenceBackend.h"

/**
 * Durable local delta store for a single generated world.
 *
 * The file contains player-authored deltas only. Generated chunk payloads stay
 * in CubusChunkStore and can be deleted/rebuilt independently.
 */
class FOrakaiLocalPersistenceBackend final : public IOrakaiPersistenceBackend
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
    virtual void RecordDensityEdit(const FOrakaiDensityEdit& Edit) override;
    virtual void ClearDensityEdit(const FIntVector& WorldSample) override;

    virtual void GetVoxelEditsForChunk(
        const FIntVector& ChunkCoordinate,
        TArray<FOrakaiVoxelEdit>& OutEdits
    ) const override;
    virtual void GetDensityEdits(TArray<FOrakaiDensityEdit>& OutEdits) const override;
    virtual void GetFoliageEditsForChunk(
        const FIntVector& ChunkCoordinate,
        TArray<FOrakaiFoliageEdit>& OutEdits
    ) const override;
    virtual int32 GetInventoryQuantity(FName ItemId) const override;
    virtual void SetInventoryQuantity(FName ItemId, int32 Quantity) override;

private:
    bool Load();
    bool Save() const;
    FString GetStorePath() const;
    void MarkDirty();

    bool bConnected = false;
    bool bWorldConfigured = false;
    bool bDirty = false;
    float TimeSinceLastMutation = 0.0f;
    int64 WorldSeed = 0;
    uint32 GenerationVersion = 0;

    FOrakaiPlayerCoordinate PlayerCoordinate;
    bool bHasPlayerCoordinate = false;
    TMap<FString, FOrakaiVoxelEdit> VoxelEdits;
    TMap<FIntVector, FOrakaiDensityEdit> DensityEdits;
    TMap<FString, FOrakaiFoliageEdit> FoliageEdits;
    TMap<FName, int32> Inventory;
};
