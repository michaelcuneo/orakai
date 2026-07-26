#include "CubusCore/Persistence/OrakaiSpacetimeBackend.h"

#include "CubusCore/Persistence/OrakaiSpacetimeConnection.h"

FOrakaiSpacetimeBackend::FOrakaiSpacetimeBackend(
    const FString& InUri,
    const FString& InDatabaseName,
    const FString& InTokenFilePath
)
    : Uri(InUri)
    , DatabaseName(InDatabaseName)
    , TokenFilePath(InTokenFilePath)
{
}

FOrakaiSpacetimeBackend::~FOrakaiSpacetimeBackend()
{
    Disconnect();
}

FString FOrakaiSpacetimeBackend::GetBackendName() const
{
    return TEXT("SpacetimeDB");
}

void FOrakaiSpacetimeBackend::Connect()
{
    if (!Connection.IsValid())
    {
        Connection.Reset(NewObject<UOrakaiSpacetimeConnection>());
    }

    Connection->BeginConnect(Uri, DatabaseName, TokenFilePath);
}

void FOrakaiSpacetimeBackend::Disconnect()
{
    if (Connection.IsValid())
    {
        Connection->Shutdown();
        Connection.Reset();
    }
}

bool FOrakaiSpacetimeBackend::IsConnected() const
{
    return Connection.IsValid() && Connection->IsConnected();
}

void FOrakaiSpacetimeBackend::Tick(float /*DeltaSeconds*/)
{
    if (Connection.IsValid())
    {
        Connection->FrameTick();
    }
}

void FOrakaiSpacetimeBackend::SetWorldConfig(
    const int64 WorldSeed,
    const uint32 GenerationVersion
)
{
    if (Connection.IsValid())
    {
        Connection->SetWorldConfig(WorldSeed, GenerationVersion);
    }
}

void FOrakaiSpacetimeBackend::RecordPlayerCoordinate(
    const FOrakaiPlayerCoordinate& Coordinate
)
{
    if (Connection.IsValid())
    {
        Connection->UpdatePlayerPosition(
            Coordinate.Location.X,
            Coordinate.Location.Y,
            Coordinate.Location.Z,
            Coordinate.Yaw,
            Coordinate.Pitch
        );
    }
}

void FOrakaiSpacetimeBackend::RecordVoxelEdit(const FOrakaiVoxelEdit& Edit)
{
    if (Connection.IsValid())
    {
        Connection->ApplyVoxelEdit(
            Edit.ChunkCoordinate.X,
            Edit.ChunkCoordinate.Y,
            Edit.ChunkCoordinate.Z,
            Edit.LocalCoordinate.X,
            Edit.LocalCoordinate.Y,
            Edit.LocalCoordinate.Z,
            Edit.MaterialId,
            Edit.bIsWater
        );
    }
}

void FOrakaiSpacetimeBackend::ClearVoxelEdit(
    const FIntVector& ChunkCoordinate,
    const FIntVector& LocalCoordinate
)
{
    if (Connection.IsValid())
    {
        Connection->ClearVoxelEdit(
            ChunkCoordinate.X,
            ChunkCoordinate.Y,
            ChunkCoordinate.Z,
            LocalCoordinate.X,
            LocalCoordinate.Y,
            LocalCoordinate.Z
        );
    }
}

void FOrakaiSpacetimeBackend::RecordFoliageEdit(const FOrakaiFoliageEdit& Edit)
{
    if (Connection.IsValid())
    {
        Connection->ApplyFoliageEdit(
            Edit.WorldVoxel.X,
            Edit.WorldVoxel.Y,
            Edit.WorldVoxel.Z,
            Edit.bRemoved,
            Edit.TypeId,
            Edit.RotationYaw,
            Edit.Scale
        );
    }
}

void FOrakaiSpacetimeBackend::ClearFoliageEdit(const FIntVector& WorldVoxel)
{
    if (Connection.IsValid())
    {
        Connection->ClearFoliageEdit(WorldVoxel.X, WorldVoxel.Y, WorldVoxel.Z);
    }
}
