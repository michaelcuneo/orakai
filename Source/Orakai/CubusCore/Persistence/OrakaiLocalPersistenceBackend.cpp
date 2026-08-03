#include "CubusCore/Persistence/OrakaiLocalPersistenceBackend.h"

#include "CubusCore/Persistence/OrakaiPersistenceLog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace OrakaiLocalPersistence
{
    constexpr uint32 Magic = 0x4F52444C; // ORDL
    constexpr uint32 Version = 2;
    constexpr uint32 FirstSupportedVersion = 1;

    void SerializeIntVector(FArchive& Archive, FIntVector& Value)
    {
        Archive << Value.X;
        Archive << Value.Y;
        Archive << Value.Z;
    }

    void SerializeTransform(FArchive& Archive, FTransform& Value)
    {
        FVector Translation = Value.GetTranslation();
        FQuat Rotation = Value.GetRotation();
        FVector Scale = Value.GetScale3D();

        Archive << Translation;
        Archive << Rotation;
        Archive << Scale;

        if (Archive.IsLoading())
        {
            Rotation.Normalize();
            Value = FTransform(Rotation, Translation, Scale);
        }
    }
}

FString FOrakaiLocalPersistenceBackend::GetBackendName() const
{
    return TEXT("LocalDeltaStore");
}

void FOrakaiLocalPersistenceBackend::Connect()
{
    bConnected = true;
}

void FOrakaiLocalPersistenceBackend::Disconnect()
{
    if (bDirty)
    {
        Save();
    }
    bConnected = false;
}

bool FOrakaiLocalPersistenceBackend::IsConnected() const
{
    return bConnected;
}

void FOrakaiLocalPersistenceBackend::Tick(const float DeltaSeconds)
{
    if (!bDirty)
    {
        return;
    }

    TimeSinceLastMutation += DeltaSeconds;
    if (TimeSinceLastMutation >= 1.0f && Save())
    {
        bDirty = false;
        TimeSinceLastMutation = 0.0f;
    }
}

void FOrakaiLocalPersistenceBackend::MarkDirty()
{
    if (!bDirty)
    {
        TimeSinceLastMutation = 0.0f;
    }
    bDirty = true;
}

void FOrakaiLocalPersistenceBackend::SetWorldConfig(
    const int64 InWorldSeed,
    const uint32 InGenerationVersion
)
{
    if (
        bWorldConfigured &&
        WorldSeed == InWorldSeed &&
        GenerationVersion == InGenerationVersion
    )
    {
        return;
    }

    if (bDirty)
    {
        Save();
    }

    WorldSeed = InWorldSeed;
    GenerationVersion = InGenerationVersion;
    bWorldConfigured = true;
    Load();
}

void FOrakaiLocalPersistenceBackend::RecordPlayerCoordinate(
    const FOrakaiPlayerCoordinate& Coordinate
)
{
    PlayerCoordinate = Coordinate;
    bHasPlayerCoordinate = true;
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::RecordVoxelEdit(
    const FOrakaiVoxelEdit& Edit
)
{
    VoxelEdits.Add(
        OrakaiPersistence::MakeVoxelEditKey(
            Edit.ChunkCoordinate,
            Edit.LocalCoordinate
        ),
        Edit
    );
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::ClearVoxelEdit(
    const FIntVector& ChunkCoordinate,
    const FIntVector& LocalCoordinate
)
{
    VoxelEdits.Remove(
        OrakaiPersistence::MakeVoxelEditKey(ChunkCoordinate, LocalCoordinate)
    );
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::RecordFoliageEdit(
    const FOrakaiFoliageEdit& Edit
)
{
    FoliageEdits.Add(
        OrakaiPersistence::MakeFoliageEditKey(Edit.WorldVoxel),
        Edit
    );
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::ClearFoliageEdit(
    const FIntVector& WorldVoxel
)
{
    FoliageEdits.Remove(OrakaiPersistence::MakeFoliageEditKey(WorldVoxel));
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::RecordDensityEdit(
    const FOrakaiDensityEdit& Edit
)
{
    if (FMath::IsNearlyZero(Edit.DensityDelta))
    {
        DensityEdits.Remove(Edit.WorldSample);
    }
    else
    {
        DensityEdits.Add(Edit.WorldSample, Edit);
    }
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::ClearDensityEdit(
    const FIntVector& WorldSample
)
{
    DensityEdits.Remove(WorldSample);
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::GetVoxelEditsForChunk(
    const FIntVector& ChunkCoordinate,
    TArray<FOrakaiVoxelEdit>& OutEdits
) const
{
    OutEdits.Reset();
    for (const TPair<FString, FOrakaiVoxelEdit>& Pair : VoxelEdits)
    {
        if (Pair.Value.ChunkCoordinate == ChunkCoordinate)
        {
            OutEdits.Add(Pair.Value);
        }
    }
}

void FOrakaiLocalPersistenceBackend::GetDensityEdits(
    TArray<FOrakaiDensityEdit>& OutEdits
) const
{
    OutEdits.Reset();
    DensityEdits.GenerateValueArray(OutEdits);
}

void FOrakaiLocalPersistenceBackend::GetFoliageEditsForChunk(
    const FIntVector& ChunkCoordinate,
    TArray<FOrakaiFoliageEdit>& OutEdits
) const
{
    OutEdits.Reset();
    for (const TPair<FString, FOrakaiFoliageEdit>& Pair : FoliageEdits)
    {
        if (OrakaiPersistence::WorldVoxelToChunk(Pair.Value.WorldVoxel) == ChunkCoordinate)
        {
            OutEdits.Add(Pair.Value);
        }
    }
}

int32 FOrakaiLocalPersistenceBackend::GetInventoryQuantity(
    const FName ItemId
) const
{
    return FMath::Max(0, Inventory.FindRef(ItemId));
}

void FOrakaiLocalPersistenceBackend::SetInventoryQuantity(
    const FName ItemId,
    const int32 Quantity
)
{
    const int32 SafeQuantity = FMath::Max(0, Quantity);
    if (SafeQuantity == 0)
    {
        Inventory.Remove(ItemId);
    }
    else
    {
        Inventory.Add(ItemId, SafeQuantity);
    }
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::IndexWorldObject(
    const FOrakaiWorldObjectRecord& Record
)
{
    WorldObjectIdsByChunk.AddUnique(
        OrakaiPersistence::PackChunkKey(Record.ChunkCoordinate),
        Record.ObjectId
    );
}

void FOrakaiLocalPersistenceBackend::UnindexWorldObject(
    const FOrakaiWorldObjectRecord& Record
)
{
    WorldObjectIdsByChunk.RemoveSingle(
        OrakaiPersistence::PackChunkKey(Record.ChunkCoordinate),
        Record.ObjectId
    );
}

void FOrakaiLocalPersistenceBackend::RecordWorldObject(
    const FOrakaiWorldObjectRecord& Record
)
{
    if (Record.ObjectId.IsEmpty() || Record.TypeId.IsNone())
    {
        return;
    }

    if (const FOrakaiWorldObjectRecord* Existing =
            WorldObjects.Find(Record.ObjectId))
    {
        UnindexWorldObject(*Existing);
    }

    WorldObjects.Add(Record.ObjectId, Record);
    IndexWorldObject(Record);
    MarkDirty();
}

void FOrakaiLocalPersistenceBackend::ClearWorldObject(
    const FString& ObjectId
)
{
    if (const FOrakaiWorldObjectRecord* Existing = WorldObjects.Find(ObjectId))
    {
        const FOrakaiWorldObjectRecord ExistingCopy = *Existing;
        WorldObjects.Remove(ObjectId);
        UnindexWorldObject(ExistingCopy);
        MarkDirty();
    }
}

bool FOrakaiLocalPersistenceBackend::GetWorldObject(
    const FString& ObjectId,
    FOrakaiWorldObjectRecord& OutRecord
) const
{
    if (const FOrakaiWorldObjectRecord* Record = WorldObjects.Find(ObjectId))
    {
        OutRecord = *Record;
        return true;
    }

    return false;
}

void FOrakaiLocalPersistenceBackend::GetWorldObjectsForChunk(
    const FIntVector& ChunkCoordinate,
    TArray<FOrakaiWorldObjectRecord>& OutRecords
) const
{
    OutRecords.Reset();

    TArray<FString> ObjectIds;
    WorldObjectIdsByChunk.MultiFind(
        OrakaiPersistence::PackChunkKey(ChunkCoordinate),
        ObjectIds
    );

    for (const FString& ObjectId : ObjectIds)
    {
        if (const FOrakaiWorldObjectRecord* Record =
                WorldObjects.Find(ObjectId))
        {
            OutRecords.Add(*Record);
        }
    }

    OutRecords.Sort(
        [](const FOrakaiWorldObjectRecord& Left,
           const FOrakaiWorldObjectRecord& Right)
        {
            return Left.ObjectId < Right.ObjectId;
        }
    );
}

FString FOrakaiLocalPersistenceBackend::GetStorePath() const
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Orakai"),
        TEXT("Worlds"),
        FString::Printf(
            TEXT("world_%lld_v%u.delta"),
            static_cast<long long>(WorldSeed),
            GenerationVersion
        )
    );
}

bool FOrakaiLocalPersistenceBackend::Load()
{
    VoxelEdits.Reset();
    DensityEdits.Reset();
    FoliageEdits.Reset();
    Inventory.Reset();
    WorldObjects.Reset();
    WorldObjectIdsByChunk.Reset();
    bHasPlayerCoordinate = false;
    bDirty = false;
    TimeSinceLastMutation = 0.0f;

    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *GetStorePath()))
    {
        return true;
    }

    FMemoryReader Reader(Bytes, true);
    uint32 StoredMagic = 0;
    uint32 StoredVersion = 0;
    int64 StoredSeed = 0;
    uint32 StoredGenerationVersion = 0;
    Reader << StoredMagic;
    Reader << StoredVersion;
    Reader << StoredSeed;
    Reader << StoredGenerationVersion;

    if (
        StoredMagic != OrakaiLocalPersistence::Magic ||
        StoredVersion < OrakaiLocalPersistence::FirstSupportedVersion ||
        StoredVersion > OrakaiLocalPersistence::Version ||
        StoredSeed != WorldSeed ||
        StoredGenerationVersion != GenerationVersion
    )
    {
        UE_LOG(LogOrakaiPersistence, Warning, TEXT("Refused incompatible local delta store: %s"), *GetStorePath());
        return false;
    }

    Reader << bHasPlayerCoordinate;
    Reader << PlayerCoordinate.Location;
    Reader << PlayerCoordinate.Yaw;
    Reader << PlayerCoordinate.Pitch;

    int32 VoxelCount = 0;
    Reader << VoxelCount;
    for (int32 Index = 0; Index < VoxelCount && !Reader.IsError(); ++Index)
    {
        FOrakaiVoxelEdit Edit;
        OrakaiLocalPersistence::SerializeIntVector(Reader, Edit.ChunkCoordinate);
        OrakaiLocalPersistence::SerializeIntVector(Reader, Edit.LocalCoordinate);
        Reader << Edit.MaterialId;
        Reader << Edit.bIsWater;
        VoxelEdits.Add(OrakaiPersistence::MakeVoxelEditKey(Edit.ChunkCoordinate, Edit.LocalCoordinate), Edit);
    }

    int32 DensityCount = 0;
    Reader << DensityCount;
    for (int32 Index = 0; Index < DensityCount && !Reader.IsError(); ++Index)
    {
        FOrakaiDensityEdit Edit;
        OrakaiLocalPersistence::SerializeIntVector(Reader, Edit.WorldSample);
        Reader << Edit.DensityDelta;
        Reader << Edit.MaterialId;
        DensityEdits.Add(Edit.WorldSample, Edit);
    }

    int32 FoliageCount = 0;
    Reader << FoliageCount;
    for (int32 Index = 0; Index < FoliageCount && !Reader.IsError(); ++Index)
    {
        FOrakaiFoliageEdit Edit;
        OrakaiLocalPersistence::SerializeIntVector(Reader, Edit.WorldVoxel);
        Reader << Edit.bRemoved;
        Reader << Edit.TypeId;
        Reader << Edit.RotationYaw;
        Reader << Edit.Scale;
        FoliageEdits.Add(OrakaiPersistence::MakeFoliageEditKey(Edit.WorldVoxel), Edit);
    }

    int32 InventoryCount = 0;
    Reader << InventoryCount;
    for (int32 Index = 0; Index < InventoryCount && !Reader.IsError(); ++Index)
    {
        FString ItemString;
        int32 Quantity = 0;
        Reader << ItemString;
        Reader << Quantity;
        Inventory.Add(FName(*ItemString), FMath::Max(0, Quantity));
    }

    if (StoredVersion >= 2)
    {
        int32 WorldObjectCount = 0;
        Reader << WorldObjectCount;
        for (
            int32 Index = 0;
            Index < WorldObjectCount && !Reader.IsError();
            ++Index
        )
        {
            FOrakaiWorldObjectRecord Record;
            FString TypeString;
            Reader << Record.ObjectId;
            Reader << TypeString;
            OrakaiLocalPersistence::SerializeIntVector(
                Reader,
                Record.ChunkCoordinate
            );
            OrakaiLocalPersistence::SerializeTransform(
                Reader,
                Record.Transform
            );
            Reader << Record.bGenerated;
            Reader << Record.bDestroyed;
            Reader << Record.Payload;
            Record.TypeId = FName(*TypeString);

            if (!Record.ObjectId.IsEmpty() && !Record.TypeId.IsNone())
            {
                WorldObjects.Add(Record.ObjectId, Record);
                IndexWorldObject(Record);
            }
        }
    }

    return !Reader.IsError();
}

bool FOrakaiLocalPersistenceBackend::Save() const
{
    if (!bConnected || !bWorldConfigured)
    {
        return false;
    }

    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    uint32 StoredMagic = OrakaiLocalPersistence::Magic;
    uint32 StoredVersion = OrakaiLocalPersistence::Version;
    int64 StoredSeed = WorldSeed;
    uint32 StoredGenerationVersion = GenerationVersion;
    Writer << StoredMagic;
    Writer << StoredVersion;
    Writer << StoredSeed;
    Writer << StoredGenerationVersion;

    bool bStoredPlayerCoordinate = bHasPlayerCoordinate;
    FVector StoredLocation = PlayerCoordinate.Location;
    float StoredYaw = PlayerCoordinate.Yaw;
    float StoredPitch = PlayerCoordinate.Pitch;
    Writer << bStoredPlayerCoordinate;
    Writer << StoredLocation;
    Writer << StoredYaw;
    Writer << StoredPitch;

    int32 VoxelCount = VoxelEdits.Num();
    Writer << VoxelCount;
    for (const TPair<FString, FOrakaiVoxelEdit>& Pair : VoxelEdits)
    {
        FOrakaiVoxelEdit Edit = Pair.Value;
        OrakaiLocalPersistence::SerializeIntVector(Writer, Edit.ChunkCoordinate);
        OrakaiLocalPersistence::SerializeIntVector(Writer, Edit.LocalCoordinate);
        Writer << Edit.MaterialId;
        Writer << Edit.bIsWater;
    }

    int32 DensityCount = DensityEdits.Num();
    Writer << DensityCount;
    for (const TPair<FIntVector, FOrakaiDensityEdit>& Pair : DensityEdits)
    {
        FOrakaiDensityEdit Edit = Pair.Value;
        OrakaiLocalPersistence::SerializeIntVector(Writer, Edit.WorldSample);
        Writer << Edit.DensityDelta;
        Writer << Edit.MaterialId;
    }

    int32 FoliageCount = FoliageEdits.Num();
    Writer << FoliageCount;
    for (const TPair<FString, FOrakaiFoliageEdit>& Pair : FoliageEdits)
    {
        FOrakaiFoliageEdit Edit = Pair.Value;
        OrakaiLocalPersistence::SerializeIntVector(Writer, Edit.WorldVoxel);
        Writer << Edit.bRemoved;
        Writer << Edit.TypeId;
        Writer << Edit.RotationYaw;
        Writer << Edit.Scale;
    }

    int32 InventoryCount = Inventory.Num();
    Writer << InventoryCount;
    for (const TPair<FName, int32>& Pair : Inventory)
    {
        FString ItemString = Pair.Key.ToString();
        int32 Quantity = Pair.Value;
        Writer << ItemString;
        Writer << Quantity;
    }

    int32 WorldObjectCount = WorldObjects.Num();
    Writer << WorldObjectCount;
    for (const TPair<FString, FOrakaiWorldObjectRecord>& Pair : WorldObjects)
    {
        FOrakaiWorldObjectRecord Record = Pair.Value;
        FString TypeString = Record.TypeId.ToString();
        Writer << Record.ObjectId;
        Writer << TypeString;
        OrakaiLocalPersistence::SerializeIntVector(
            Writer,
            Record.ChunkCoordinate
        );
        OrakaiLocalPersistence::SerializeTransform(Writer, Record.Transform);
        Writer << Record.bGenerated;
        Writer << Record.bDestroyed;
        Writer << Record.Payload;
    }
    Writer.Close();

    const FString Path = GetStorePath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    const FString TemporaryPath = Path + TEXT(".tmp");

    if (!FFileHelper::SaveArrayToFile(Bytes, *TemporaryPath))
    {
        return false;
    }

    return IFileManager::Get().Move(*Path, *TemporaryPath, true, true);
}
