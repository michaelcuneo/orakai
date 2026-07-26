#include "CubusCore/Persistence/OrakaiSpacetimeConnection.h"

#include "CubusCore/Persistence/OrakaiPersistenceLog.h"
#include "Connection/Credentials.h"

void UOrakaiSpacetimeConnection::BeginConnect(
    const FString& InUri,
    const FString& InDatabaseName,
    const FString& InTokenFilePath
)
{
    TokenFilePath = InTokenFilePath;

    UCredentials::Init(*TokenFilePath);
    const FString Token = UCredentials::LoadToken();

    FOnConnectDelegate ConnectDelegate;
    BIND_DELEGATE_SAFE(ConnectDelegate, this, UOrakaiSpacetimeConnection, HandleConnect);

    FOnConnectErrorDelegate ConnectErrorDelegate;
    BIND_DELEGATE_SAFE(ConnectErrorDelegate, this, UOrakaiSpacetimeConnection, HandleConnectError);

    FOnDisconnectDelegate DisconnectDelegate;
    BIND_DELEGATE_SAFE(DisconnectDelegate, this, UOrakaiSpacetimeConnection, HandleDisconnect);

    UDbConnectionBuilder* Builder = UDbConnection::Builder()
        ->WithUri(InUri)
        ->WithDatabaseName(InDatabaseName)
        ->OnConnect(ConnectDelegate)
        ->OnConnectError(ConnectErrorDelegate)
        ->OnDisconnect(DisconnectDelegate);

    if (!Token.IsEmpty())
    {
        Builder = Builder->WithToken(Token);
    }

    Conn = Builder->Build();

    UE_LOG(
        LogOrakaiPersistence,
        Log,
        TEXT("SpacetimeDB connecting to %s (database '%s')."),
        *InUri,
        *InDatabaseName
    );
}

void UOrakaiSpacetimeConnection::Shutdown()
{
    if (Conn != nullptr && Conn->IsActive())
    {
        Conn->Disconnect();
    }

    Conn = nullptr;
    bConnected = false;
}

void UOrakaiSpacetimeConnection::FrameTick()
{
    if (Conn != nullptr)
    {
        Conn->FrameTick();
    }
}

bool UOrakaiSpacetimeConnection::HasReducers() const
{
    return bConnected && Conn != nullptr && Conn->Reducers != nullptr;
}

void UOrakaiSpacetimeConnection::HandleConnect(
    UDbConnection* InConn,
    FSpacetimeDBIdentity /*Identity*/,
    const FString& Token
)
{
    UCredentials::SaveToken(Token);
    bConnected = true;

    UE_LOG(LogOrakaiPersistence, Log, TEXT("SpacetimeDB connected."));

    if (InConn == nullptr)
    {
        return;
    }

    FOnSubscriptionApplied AppliedDelegate;
    BIND_DELEGATE_SAFE(AppliedDelegate, this, UOrakaiSpacetimeConnection, HandleSubscriptionApplied);

    InConn->SubscriptionBuilder()
        ->OnApplied(AppliedDelegate)
        ->SubscribeToAllTables();
}

void UOrakaiSpacetimeConnection::HandleConnectError(const FString& Error)
{
    UE_LOG(LogOrakaiPersistence, Error, TEXT("SpacetimeDB connection error: %s"), *Error);
}

void UOrakaiSpacetimeConnection::HandleDisconnect(
    UDbConnection* /*InConn*/,
    const FString& Error
)
{
    bConnected = false;

    if (Error.IsEmpty())
    {
        UE_LOG(LogOrakaiPersistence, Log, TEXT("SpacetimeDB disconnected."));
    }
    else
    {
        UE_LOG(LogOrakaiPersistence, Warning, TEXT("SpacetimeDB disconnected: %s"), *Error);
    }
}

void UOrakaiSpacetimeConnection::HandleSubscriptionApplied(
    FSubscriptionEventContext /*Context*/
)
{
    UE_LOG(LogOrakaiPersistence, Log, TEXT("SpacetimeDB subscription applied."));
}

void UOrakaiSpacetimeConnection::UpdatePlayerPosition(
    const double X,
    const double Y,
    const double Z,
    const float Yaw,
    const float Pitch
)
{
    if (HasReducers())
    {
        Conn->Reducers->UpdatePlayerPosition(X, Y, Z, Yaw, Pitch);
    }
}

void UOrakaiSpacetimeConnection::ApplyVoxelEdit(
    const int32 ChunkX,
    const int32 ChunkY,
    const int32 ChunkZ,
    const int32 LocalX,
    const int32 LocalY,
    const int32 LocalZ,
    const int32 MaterialId,
    const bool bIsWater
)
{
    if (HasReducers())
    {
        Conn->Reducers->ApplyVoxelEdit(
            ChunkX,
            ChunkY,
            ChunkZ,
            LocalX,
            LocalY,
            LocalZ,
            MaterialId,
            bIsWater
        );
    }
}

void UOrakaiSpacetimeConnection::ClearVoxelEdit(
    const int32 ChunkX,
    const int32 ChunkY,
    const int32 ChunkZ,
    const int32 LocalX,
    const int32 LocalY,
    const int32 LocalZ
)
{
    if (HasReducers())
    {
        Conn->Reducers->ClearVoxelEdit(ChunkX, ChunkY, ChunkZ, LocalX, LocalY, LocalZ);
    }
}

void UOrakaiSpacetimeConnection::ApplyFoliageEdit(
    const int32 WorldX,
    const int32 WorldY,
    const int32 WorldZ,
    const bool bRemoved,
    const int32 TypeId,
    const float Yaw,
    const float Scale
)
{
    if (HasReducers())
    {
        Conn->Reducers->ApplyFoliageEdit(WorldX, WorldY, WorldZ, bRemoved, TypeId, Yaw, Scale);
    }
}

void UOrakaiSpacetimeConnection::ClearFoliageEdit(
    const int32 WorldX,
    const int32 WorldY,
    const int32 WorldZ
)
{
    if (HasReducers())
    {
        Conn->Reducers->ClearFoliageEdit(WorldX, WorldY, WorldZ);
    }
}

void UOrakaiSpacetimeConnection::SetWorldConfig(
    const int64 WorldSeed,
    const uint32 GenerationVersion
)
{
    if (HasReducers())
    {
        Conn->Reducers->SetWorldConfig(WorldSeed, GenerationVersion);
    }
}
