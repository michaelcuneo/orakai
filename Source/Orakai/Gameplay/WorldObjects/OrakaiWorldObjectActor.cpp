#include "Gameplay/WorldObjects/OrakaiWorldObjectActor.h"

#include "CubusCore/Persistence/OrakaiPersistenceSubsystem.h"
#include "Net/UnrealNetwork.h"

AOrakaiWorldObjectActor::AOrakaiWorldObjectActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);
}

void AOrakaiWorldObjectActor::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AOrakaiWorldObjectActor, PersistentObjectId);
    DOREPLIFETIME(AOrakaiWorldObjectActor, WorldObjectTypeId);
    DOREPLIFETIME(AOrakaiWorldObjectActor, OwningChunkCoordinate);
    DOREPLIFETIME(AOrakaiWorldObjectActor, bGeneratedObject);
    DOREPLIFETIME(AOrakaiWorldObjectActor, ObjectPayload);
}

void AOrakaiWorldObjectActor::InitializeGeneratedObject(
    const int64 WorldSeed,
    const FName TypeId,
    const FIntVector StableCoordinate,
    const FIntVector ChunkCoordinate,
    const FString& InPayload
)
{
    if (!HasAuthority() || TypeId.IsNone())
    {
        return;
    }

    PersistentObjectId = OrakaiPersistence::MakeGeneratedWorldObjectId(
        WorldSeed,
        TypeId,
        StableCoordinate
    );
    WorldObjectTypeId = TypeId;
    OwningChunkCoordinate = ChunkCoordinate;
    bGeneratedObject = true;
    ObjectPayload = InPayload;
    ForceNetUpdate();
}

bool AOrakaiWorldObjectActor::InitializePlacedObject(
    const FName TypeId,
    const FIntVector ChunkCoordinate,
    const FString& InPayload
)
{
    if (!HasAuthority() || TypeId.IsNone())
    {
        return false;
    }

    PersistentObjectId = OrakaiPersistence::MakePlacedWorldObjectId();
    WorldObjectTypeId = TypeId;
    OwningChunkCoordinate = ChunkCoordinate;
    bGeneratedObject = false;
    ObjectPayload = InPayload;
    ForceNetUpdate();
    return PersistWorldObject();
}

void AOrakaiWorldObjectActor::InitializeFromRecord(
    const FOrakaiWorldObjectRecord& Record
)
{
    if (!HasAuthority() || Record.ObjectId.IsEmpty())
    {
        return;
    }

    PersistentObjectId = Record.ObjectId;
    WorldObjectTypeId = Record.TypeId;
    OwningChunkCoordinate = Record.ChunkCoordinate;
    bGeneratedObject = Record.bGenerated;
    ObjectPayload = Record.Payload;
    SetActorTransform(Record.Transform);
    SetActorHiddenInGame(Record.bDestroyed);
    SetActorEnableCollision(!Record.bDestroyed);
    ForceNetUpdate();
}

FOrakaiWorldObjectRecord
AOrakaiWorldObjectActor::BuildPersistenceRecord() const
{
    FOrakaiWorldObjectRecord Record;
    Record.ObjectId = PersistentObjectId;
    Record.TypeId = WorldObjectTypeId;
    Record.ChunkCoordinate = OwningChunkCoordinate;
    Record.Transform = GetActorTransform();
    Record.bGenerated = bGeneratedObject;
    Record.bDestroyed = false;
    Record.Payload = ObjectPayload;
    return Record;
}

bool AOrakaiWorldObjectActor::PersistWorldObject()
{
    if (
        !HasAuthority() ||
        PersistentObjectId.IsEmpty() ||
        WorldObjectTypeId.IsNone()
    )
    {
        return false;
    }

    UOrakaiPersistenceSubsystem* Persistence =
        UOrakaiPersistenceSubsystem::Get(this);
    if (Persistence == nullptr)
    {
        return false;
    }

    Persistence->RecordWorldObject(BuildPersistenceRecord());
    return true;
}

bool AOrakaiWorldObjectActor::DestroyPersistentWorldObject()
{
    if (!HasAuthority() || PersistentObjectId.IsEmpty())
    {
        return false;
    }

    UOrakaiPersistenceSubsystem* Persistence =
        UOrakaiPersistenceSubsystem::Get(this);
    if (Persistence == nullptr)
    {
        return false;
    }

    if (bGeneratedObject)
    {
        FOrakaiWorldObjectRecord Tombstone = BuildPersistenceRecord();
        Tombstone.bDestroyed = true;
        Persistence->RecordWorldObject(Tombstone);
    }
    else
    {
        Persistence->ClearWorldObjectDelta(PersistentObjectId);
    }

    Destroy();
    return true;
}

void AOrakaiWorldObjectActor::SetOwningChunkCoordinate(
    const FIntVector ChunkCoordinate
)
{
    if (!HasAuthority())
    {
        return;
    }

    OwningChunkCoordinate = ChunkCoordinate;
    ForceNetUpdate();
}

void AOrakaiWorldObjectActor::SetObjectPayload(
    const FString& InPayload
)
{
    if (!HasAuthority())
    {
        return;
    }

    ObjectPayload = InPayload;
    ForceNetUpdate();
}
