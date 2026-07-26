#pragma once

#include "CoreMinimal.h"
#include "ModuleBindings/SpacetimeDBClient.g.h"
#include "OrakaiSpacetimeConnection.generated.h"

/**
 * UObject wrapper around the generated UDbConnection.
 *
 * The SpacetimeDB SDK drives connection lifecycle through dynamic (UObject)
 * delegates, so the callbacks must live on a UObject. This class owns the
 * connection, handles connect/disconnect/subscription callbacks, and exposes
 * plain forwarders for each reducer. FOrakaiSpacetimeBackend keeps an instance
 * alive and forwards the transport-agnostic persistence calls here.
 */
UCLASS()
class ORAKAI_API UOrakaiSpacetimeConnection : public UObject
{
    GENERATED_BODY()

public:
    void BeginConnect(
        const FString& InUri,
        const FString& InDatabaseName,
        const FString& InTokenFilePath
    );
    void Shutdown();
    void FrameTick();

    bool IsConnected() const { return bConnected; }

    void UpdatePlayerPosition(double X, double Y, double Z, float Yaw, float Pitch);
    void ApplyVoxelEdit(
        int32 ChunkX,
        int32 ChunkY,
        int32 ChunkZ,
        int32 LocalX,
        int32 LocalY,
        int32 LocalZ,
        int32 MaterialId,
        bool bIsWater
    );
    void ClearVoxelEdit(
        int32 ChunkX,
        int32 ChunkY,
        int32 ChunkZ,
        int32 LocalX,
        int32 LocalY,
        int32 LocalZ
    );
    void ApplyFoliageEdit(
        int32 WorldX,
        int32 WorldY,
        int32 WorldZ,
        bool bRemoved,
        int32 TypeId,
        float Yaw,
        float Scale
    );
    void ClearFoliageEdit(int32 WorldX, int32 WorldY, int32 WorldZ);
    void SetWorldConfig(int64 WorldSeed, uint32 GenerationVersion);

private:
    UFUNCTION()
    void HandleConnect(UDbConnection* InConn, FSpacetimeDBIdentity Identity, const FString& Token);

    UFUNCTION()
    void HandleConnectError(const FString& Error);

    UFUNCTION()
    void HandleDisconnect(UDbConnection* InConn, const FString& Error);

    UFUNCTION()
    void HandleSubscriptionApplied(FSubscriptionEventContext Context);

    bool HasReducers() const;

    UPROPERTY()
    UDbConnection* Conn = nullptr;

    FString TokenFilePath;
    bool bConnected = false;
};
