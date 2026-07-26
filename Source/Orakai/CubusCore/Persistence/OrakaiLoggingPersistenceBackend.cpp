#include "CubusCore/Persistence/OrakaiLoggingPersistenceBackend.h"

#include "CubusCore/Persistence/OrakaiPersistenceLog.h"

FString FOrakaiLoggingPersistenceBackend::GetBackendName() const
{
    return TEXT("Logging");
}

void FOrakaiLoggingPersistenceBackend::Connect()
{
    bConnected = true;
    UE_LOG(LogOrakaiPersistence, Log, TEXT("Logging backend connected (no network I/O)."));
}

void FOrakaiLoggingPersistenceBackend::Disconnect()
{
    bConnected = false;
    UE_LOG(LogOrakaiPersistence, Log, TEXT("Logging backend disconnected."));
}

bool FOrakaiLoggingPersistenceBackend::IsConnected() const
{
    return bConnected;
}

void FOrakaiLoggingPersistenceBackend::Tick(float /*DeltaSeconds*/)
{
}

void FOrakaiLoggingPersistenceBackend::SetWorldConfig(
    const int64 WorldSeed,
    const uint32 GenerationVersion
)
{
    UE_LOG(
        LogOrakaiPersistence,
        Verbose,
        TEXT("SetWorldConfig seed=%lld genVersion=%u"),
        WorldSeed,
        GenerationVersion
    );
}

void FOrakaiLoggingPersistenceBackend::RecordPlayerCoordinate(
    const FOrakaiPlayerCoordinate& Coordinate
)
{
    UE_LOG(
        LogOrakaiPersistence,
        Verbose,
        TEXT("RecordPlayerCoordinate loc=(%.1f, %.1f, %.1f) yaw=%.1f pitch=%.1f"),
        Coordinate.Location.X,
        Coordinate.Location.Y,
        Coordinate.Location.Z,
        Coordinate.Yaw,
        Coordinate.Pitch
    );
}

void FOrakaiLoggingPersistenceBackend::RecordVoxelEdit(const FOrakaiVoxelEdit& Edit)
{
    UE_LOG(
        LogOrakaiPersistence,
        Verbose,
        TEXT("RecordVoxelEdit chunk=(%d, %d, %d) local=(%d, %d, %d) material=%d water=%d"),
        Edit.ChunkCoordinate.X,
        Edit.ChunkCoordinate.Y,
        Edit.ChunkCoordinate.Z,
        Edit.LocalCoordinate.X,
        Edit.LocalCoordinate.Y,
        Edit.LocalCoordinate.Z,
        Edit.MaterialId,
        Edit.bIsWater ? 1 : 0
    );
}

void FOrakaiLoggingPersistenceBackend::ClearVoxelEdit(
    const FIntVector& ChunkCoordinate,
    const FIntVector& LocalCoordinate
)
{
    UE_LOG(
        LogOrakaiPersistence,
        Verbose,
        TEXT("ClearVoxelEdit chunk=(%d, %d, %d) local=(%d, %d, %d)"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        LocalCoordinate.X,
        LocalCoordinate.Y,
        LocalCoordinate.Z
    );
}

void FOrakaiLoggingPersistenceBackend::RecordFoliageEdit(const FOrakaiFoliageEdit& Edit)
{
    UE_LOG(
        LogOrakaiPersistence,
        Verbose,
        TEXT("RecordFoliageEdit world=(%d, %d, %d) removed=%d type=%d yaw=%.1f scale=%.2f"),
        Edit.WorldVoxel.X,
        Edit.WorldVoxel.Y,
        Edit.WorldVoxel.Z,
        Edit.bRemoved ? 1 : 0,
        Edit.TypeId,
        Edit.RotationYaw,
        Edit.Scale
    );
}

void FOrakaiLoggingPersistenceBackend::ClearFoliageEdit(const FIntVector& WorldVoxel)
{
    UE_LOG(
        LogOrakaiPersistence,
        Verbose,
        TEXT("ClearFoliageEdit world=(%d, %d, %d)"),
        WorldVoxel.X,
        WorldVoxel.Y,
        WorldVoxel.Z
    );
}
