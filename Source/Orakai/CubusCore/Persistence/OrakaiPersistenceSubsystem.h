#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "CubusCore/Persistence/OrakaiPersistenceBackend.h"
#include "CubusCore/Persistence/OrakaiPersistenceTypes.h"
#include "OrakaiPersistenceSubsystem.generated.h"

/**
 * Front door for persisting authoritative world state (player coordinates,
 * voxel edits, foliage edits, world config).
 *
 * Game code calls this subsystem; it throttles high-frequency player position
 * updates, forwards discrete edits immediately, and delegates all transport to
 * a swappable IOrakaiPersistenceBackend. A local delta backend is installed by
 * default so offline worlds survive restarts. Call
 * SetBackend() to install the SpacetimeDB backend once it is available.
 */
UCLASS()
class ORAKAI_API UOrakaiPersistenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UOrakaiPersistenceSubsystem();
    virtual ~UOrakaiPersistenceSubsystem() override;

    /** Convenience accessor from any world context object. */
    static UOrakaiPersistenceSubsystem* Get(const UObject* WorldContextObject);

    // USubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Replace the active backend (e.g. install the SpacetimeDB backend). */
    void SetBackend(TUniquePtr<IOrakaiPersistenceBackend> InBackend);

    /**
     * Install and connect the SpacetimeDB backend, replacing the default
     * local backend. Safe to call from Blueprints at startup.
     */
    UFUNCTION(BlueprintCallable, Category = "Orakai|Persistence")
    void ConnectToSpacetimeDB(
        const FString& Uri = TEXT("127.0.0.1:3000"),
        const FString& DatabaseName = TEXT("orakai"),
        const FString& TokenFilePath = TEXT(".spacetime_orakai")
    );

    /** Currently installed backend, never null after Initialize(). */
    IOrakaiPersistenceBackend* GetBackend() const { return Backend.Get(); }

    /** True when the active backend reports a live connection. */
    UFUNCTION(BlueprintPure, Category = "Orakai|Persistence")
    bool IsConnected() const;

    /** Publish the world seed / generation version singleton. */
    void SetWorldConfig(int64 WorldSeed, uint32 GenerationVersion);

    /** Queue the local player's transform (throttled before it is forwarded). */
    void RecordPlayerCoordinate(const FVector& Location, float Yaw, float Pitch);

    /** Blueprint-friendly wrapper for RecordPlayerCoordinate. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|Persistence")
    void RecordPlayerLocation(FVector Location, float Yaw, float Pitch);

    /** Forward a voxel delta immediately. */
    void RecordVoxelEdit(
        const FIntVector& ChunkCoordinate,
        const FIntVector& LocalCoordinate,
        int32 MaterialId,
        bool bIsWater
    );

    /** Remove a voxel delta immediately. */
    void ClearVoxelEdit(
        const FIntVector& ChunkCoordinate,
        const FIntVector& LocalCoordinate
    );

    void RecordDensityEdit(
        const FIntVector& WorldSample,
        float DensityDelta,
        int32 MaterialId
    );

    void ClearDensityEdit(const FIntVector& WorldSample);

    void GetVoxelEditsForChunk(
        const FIntVector& ChunkCoordinate,
        TArray<FOrakaiVoxelEdit>& OutEdits
    ) const;

    void GetDensityEdits(TArray<FOrakaiDensityEdit>& OutEdits) const;

    void GetFoliageEditsForChunk(
        const FIntVector& ChunkCoordinate,
        TArray<FOrakaiFoliageEdit>& OutEdits
    ) const;

    /** Forward a foliage delta immediately. */
    void RecordFoliageEdit(
        const FIntVector& WorldVoxel,
        bool bRemoved,
        int32 TypeId,
        float RotationYaw,
        float Scale
    );

    /** Remove a foliage delta immediately. */
    void ClearFoliageEdit(const FIntVector& WorldVoxel);

    UFUNCTION(BlueprintPure, Category = "Orakai|Persistence|Inventory")
    int32 GetInventoryQuantity(FName ItemId) const;

    UFUNCTION(BlueprintCallable, Category = "Orakai|Persistence|Inventory")
    void SetInventoryQuantity(FName ItemId, int32 Quantity);

    /** Stable ID for a deterministic generated object such as a tree or rock. */
    UFUNCTION(BlueprintPure, Category = "Orakai|Persistence|World Objects")
    FString MakeGeneratedWorldObjectId(
        int64 WorldSeed,
        FName TypeId,
        FIntVector StableCoordinate
    ) const;

    /** Store or replace a complete object delta. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|Persistence|World Objects")
    void RecordWorldObject(const FOrakaiWorldObjectRecord& Record);

    /** Create and persist a player-authored object record. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|Persistence|World Objects")
    FString RecordPlacedWorldObject(
        FName TypeId,
        FIntVector ChunkCoordinate,
        FTransform Transform,
        const FString& Payload
    );

    /** Persist a tombstone for a reproducible generated object. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|Persistence|World Objects")
    FString TombstoneGeneratedWorldObject(
        int64 WorldSeed,
        FName TypeId,
        FIntVector StableCoordinate,
        FIntVector ChunkCoordinate,
        FTransform GeneratedTransform
    );

    /**
     * Destroy an object delta. Generated objects become tombstones; placed
     * objects are removed because they have no generated baseline.
     */
    UFUNCTION(BlueprintCallable, Category = "Orakai|Persistence|World Objects")
    bool DestroyWorldObject(const FString& ObjectId);

    /** Remove a delta and reveal the deterministic generated baseline again. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|Persistence|World Objects")
    void ClearWorldObjectDelta(const FString& ObjectId);

    UFUNCTION(BlueprintPure, Category = "Orakai|Persistence|World Objects")
    bool GetWorldObject(
        const FString& ObjectId,
        FOrakaiWorldObjectRecord& OutRecord
    ) const;

    UFUNCTION(BlueprintPure, Category = "Orakai|Persistence|World Objects")
    TArray<FOrakaiWorldObjectRecord> GetWorldObjectsForChunk(
        FIntVector ChunkCoordinate
    ) const;

    /** Minimum seconds between forwarded player position updates. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|Persistence")
    float PositionSendInterval = 0.2f;

    /** Minimum movement (cm) before a queued position is forwarded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|Persistence")
    float PositionSendDistanceThreshold = 25.0f;

private:
    bool HandleTick(float DeltaSeconds);
    void FlushPendingCoordinate();

    TUniquePtr<IOrakaiPersistenceBackend> Backend;
    FTSTicker::FDelegateHandle TickerHandle;

    FOrakaiPlayerCoordinate PendingCoordinate;
    bool bHasPendingCoordinate = false;
    bool bHasSentCoordinate = false;
    FVector LastSentLocation = FVector::ZeroVector;
    float TimeSinceLastPositionSend = 0.0f;

    // Cached world config, resent when the backend (re)connects.
    int64 WorldSeedValue = 0;
    uint32 GenerationVersionValue = 0;
    bool bHasWorldConfig = false;
    bool bBackendWasConnected = false;
};
