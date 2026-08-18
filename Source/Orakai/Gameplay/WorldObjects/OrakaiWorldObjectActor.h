#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusCore/Persistence/OrakaiPersistenceTypes.h"
#include "OrakaiWorldObjectActor.generated.h"

/**
 * Replicated runtime base for persistent objects that are not terrain.
 *
 * Pickups, rocks and constructions can derive from this actor. Generated
 * objects use deterministic IDs and only persist changed state/tombstones;
 * player-placed objects persist their complete record. Terrain chunks never
 * own or replicate these actors.
 */
UCLASS(BlueprintType, Blueprintable)
class ORAKAI_API AOrakaiWorldObjectActor : public AActor
{
    GENERATED_BODY()

public:
    AOrakaiWorldObjectActor();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    /** Assign a reproducible identity to a generated object. Does not save. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|World Objects")
    void InitializeGeneratedObject(
        int64 WorldSeed,
        FName TypeId,
        FIntVector StableCoordinate,
        FIntVector ChunkCoordinate,
        const FString& InPayload
    );

    /** Assign a new identity to a player-placed object and persist it. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|World Objects")
    bool InitializePlacedObject(
        FName TypeId,
        FIntVector ChunkCoordinate,
        const FString& InPayload
    );

    /** Restore identity/state from a streamed persistence record. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|World Objects")
    void InitializeFromRecord(const FOrakaiWorldObjectRecord& Record);

    /** Persist the actor's current transform, chunk and payload. */
    UFUNCTION(BlueprintCallable, Category = "Orakai|World Objects")
    bool PersistWorldObject();

    /**
     * Destroy this runtime object through the persistence layer. Generated
     * objects leave tombstones; placed objects remove their authored record.
     */
    UFUNCTION(BlueprintCallable, Category = "Orakai|World Objects")
    bool DestroyPersistentWorldObject();

    UFUNCTION(BlueprintPure, Category = "Orakai|World Objects")
    FOrakaiWorldObjectRecord BuildPersistenceRecord() const;

    UFUNCTION(BlueprintPure, Category = "Orakai|World Objects")
    FString GetPersistentObjectId() const { return PersistentObjectId; }

    UFUNCTION(BlueprintPure, Category = "Orakai|World Objects")
    FName GetWorldObjectTypeId() const { return WorldObjectTypeId; }

    UFUNCTION(BlueprintCallable, Category = "Orakai|World Objects")
    void SetOwningChunkCoordinate(FIntVector ChunkCoordinate);

    UFUNCTION(BlueprintCallable, Category = "Orakai|World Objects")
    void SetObjectPayload(const FString& InPayload);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Orakai|World Objects")
    FString PersistentObjectId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Orakai|World Objects")
    FName WorldObjectTypeId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Orakai|World Objects")
    FIntVector OwningChunkCoordinate = FIntVector::ZeroValue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Orakai|World Objects")
    bool bGeneratedObject = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Orakai|World Objects")
    FString ObjectPayload;
};
