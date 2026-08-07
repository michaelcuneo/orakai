#pragma once

#include "CoreMinimal.h"
#include "OrakaiPersistenceTypes.generated.h"

/**
 * Transport-agnostic value types shared by the persistence subsystem and its
 * backends. These intentionally avoid any SpacetimeDB SDK dependency so the
 * capture layer compiles without the plugin installed.
 */

/** A single authoritative voxel delta applied on top of generated terrain. */
struct FOrakaiVoxelEdit
{
    FIntVector ChunkCoordinate = FIntVector::ZeroValue;
    FIntVector LocalCoordinate = FIntVector::ZeroValue;
    int32 MaterialId = 0;
    bool bIsWater = false;
};

/** A sparse scalar-field delta applied on top of generated density terrain. */
struct FOrakaiDensityEdit
{
    FIntVector WorldSample = FIntVector::ZeroValue;
    float DensityDelta = 0.0f;
    int32 MaterialId = 0;
};

/** A single authoritative foliage delta applied on top of generated foliage. */
struct FOrakaiFoliageEdit
{
    FIntVector WorldVoxel = FIntVector::ZeroValue;

    // When true the player removed foliage the generator would place here and
    // the remaining fields are ignored.
    bool bRemoved = false;

    int32 TypeId = 0;
    float RotationYaw = 0.0f;
    float Scale = 1.0f;
};

/** Player transform snapshot forwarded to the authoritative store. */
struct FOrakaiPlayerCoordinate
{
    FVector Location = FVector::ZeroVector;
    float Yaw = 0.0f;
    float Pitch = 0.0f;
};

/** Minimal item stack used by the first local survival interaction. */
struct FOrakaiInventoryEntry
{
    FName ItemId = NAME_None;
    int32 Quantity = 0;
};

/**
 * A persistent non-terrain object delta.
 *
 * Generated objects only need a record when their generated state changes
 * (normally a tombstone). Player-placed objects store their complete authored
 * state here. Runtime actors are reconstructed separately from these records;
 * they are deliberately not baked into chunk meshes.
 */
USTRUCT(BlueprintType)
struct FOrakaiWorldObjectRecord
{
    GENERATED_BODY()

    /** Stable primary key. Generated IDs are reproducible from seed + location. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|World Objects")
    FString ObjectId;

    /** Catalog/type key used to choose the runtime actor or object definition. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|World Objects")
    FName TypeId = NAME_None;

    /** Spatial index used to load this record with its owning terrain chunk. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|World Objects")
    FIntVector ChunkCoordinate = FIntVector::ZeroValue;

    /** Authored or generated world transform. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|World Objects")
    FTransform Transform = FTransform::Identity;

    /** True when the baseline generator owns the object. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|World Objects")
    bool bGenerated = false;

    /** A generated-object tombstone; no actor should be reconstructed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|World Objects")
    bool bDestroyed = false;

    /** Small type-specific payload. Large state belongs in a dedicated system. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orakai|World Objects")
    FString Payload;
};

namespace OrakaiPersistence
{
    // Must match Cubus::ChunkSize and the server module's ChunkSize constant.
    inline constexpr int32 ChunkSize = 32;

    /**
     * Packs a chunk coordinate into a single int64, mirroring PackChunkKey in
     * the SpacetimeDB module (each component masked to 21 bits).
     */
    inline int64 PackChunkKey(const FIntVector& ChunkCoordinate)
    {
        const int64 X = static_cast<int64>(ChunkCoordinate.X) & 0x1FFFFF;
        const int64 Y = static_cast<int64>(ChunkCoordinate.Y) & 0x1FFFFF;
        const int64 Z = static_cast<int64>(ChunkCoordinate.Z) & 0x1FFFFF;
        return (X << 42) | (Y << 21) | Z;
    }

    /** Floor division so negative world coordinates map to the correct chunk. */
    inline int32 FloorDiv(const int32 Value, const int32 Divisor)
    {
        int32 Quotient = Value / Divisor;
        if ((Value % Divisor != 0) && ((Value < 0) != (Divisor < 0)))
        {
            --Quotient;
        }
        return Quotient;
    }

    /** Deterministic key matching the server voxel_edit primary key. */
    inline FString MakeVoxelEditKey(
        const FIntVector& ChunkCoordinate,
        const FIntVector& LocalCoordinate
    )
    {
        return FString::Printf(
            TEXT("%d:%d:%d:%d:%d:%d"),
            ChunkCoordinate.X,
            ChunkCoordinate.Y,
            ChunkCoordinate.Z,
            LocalCoordinate.X,
            LocalCoordinate.Y,
            LocalCoordinate.Z
        );
    }

    /** Deterministic key matching the server foliage_edit primary key. */
    inline FString MakeFoliageEditKey(const FIntVector& WorldVoxel)
    {
        return FString::Printf(
            TEXT("%d:%d:%d"),
            WorldVoxel.X,
            WorldVoxel.Y,
            WorldVoxel.Z
        );
    }

    /**
     * Reproducible ID for an object produced by deterministic generation.
     * StableCoordinate is the generator's canonical integer anchor (for
     * foliage this is its world voxel), so IDs do not depend on actor order.
     */
    inline FString MakeGeneratedWorldObjectId(
        const int64 WorldSeed,
        const FName TypeId,
        const FIntVector& StableCoordinate
    )
    {
        return FString::Printf(
            TEXT("generated:%lld:%s:%d:%d:%d"),
            static_cast<long long>(WorldSeed),
            *TypeId.ToString(),
            StableCoordinate.X,
            StableCoordinate.Y,
            StableCoordinate.Z
        );
    }

    /** Globally unique ID for a player-authored object record. */
    inline FString MakePlacedWorldObjectId()
    {
        return FString::Printf(
            TEXT("placed:%s"),
            *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)
        );
    }

    /** Owning chunk coordinate for a world voxel. */
    inline FIntVector WorldVoxelToChunk(const FIntVector& WorldVoxel)
    {
        return FIntVector(
            FloorDiv(WorldVoxel.X, ChunkSize),
            FloorDiv(WorldVoxel.Y, ChunkSize),
            FloorDiv(WorldVoxel.Z, ChunkSize)
        );
    }
}
