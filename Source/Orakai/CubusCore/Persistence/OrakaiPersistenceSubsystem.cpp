#include "CubusCore/Persistence/OrakaiPersistenceSubsystem.h"

#include "CubusCore/Persistence/OrakaiLocalPersistenceBackend.h"
#include "CubusCore/Persistence/OrakaiPersistenceBackend.h"
#include "CubusCore/Persistence/OrakaiPersistenceLog.h"
#include "CubusCore/Persistence/OrakaiSpacetimeBackend.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogOrakaiPersistence);

UOrakaiPersistenceSubsystem::UOrakaiPersistenceSubsystem() = default;
UOrakaiPersistenceSubsystem::~UOrakaiPersistenceSubsystem() = default;

UOrakaiPersistenceSubsystem* UOrakaiPersistenceSubsystem::Get(
    const UObject* WorldContextObject
)
{
    if (WorldContextObject == nullptr)
    {
        return nullptr;
    }

    const UWorld* World = WorldContextObject->GetWorld();
    if (World == nullptr)
    {
        return nullptr;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (GameInstance == nullptr)
    {
        return nullptr;
    }

    return GameInstance->GetSubsystem<UOrakaiPersistenceSubsystem>();
}

void UOrakaiPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Local play must survive process restarts. The backend stores only deltas;
    // generated chunk payloads remain a separate disposable cache.
    Backend = MakeUnique<FOrakaiLocalPersistenceBackend>();
    Backend->Connect();

    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UOrakaiPersistenceSubsystem::HandleTick)
    );

    UE_LOG(
        LogOrakaiPersistence,
        Log,
        TEXT("Persistence subsystem initialized with '%s' backend."),
        *Backend->GetBackendName()
    );
}

void UOrakaiPersistenceSubsystem::Deinitialize()
{
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    if (Backend.IsValid())
    {
        Backend->Disconnect();
        Backend.Reset();
    }

    Super::Deinitialize();
}

void UOrakaiPersistenceSubsystem::SetBackend(
    TUniquePtr<IOrakaiPersistenceBackend> InBackend
)
{
    if (!InBackend.IsValid())
    {
        return;
    }

    if (Backend.IsValid())
    {
        Backend->Disconnect();
    }

    Backend = MoveTemp(InBackend);
    Backend->Connect();

    // Force config + coordinate to be (re)sent to the new backend once it
    // reports a connection.
    bHasSentCoordinate = false;
    bBackendWasConnected = false;

    UE_LOG(
        LogOrakaiPersistence,
        Log,
        TEXT("Persistence backend switched to '%s'."),
        *Backend->GetBackendName()
    );
}

bool UOrakaiPersistenceSubsystem::IsConnected() const
{
    return Backend.IsValid() && Backend->IsConnected();
}

void UOrakaiPersistenceSubsystem::ConnectToSpacetimeDB(
    const FString& Uri,
    const FString& DatabaseName,
    const FString& TokenFilePath
)
{
    SetBackend(MakeUnique<FOrakaiSpacetimeBackend>(Uri, DatabaseName, TokenFilePath));
}

void UOrakaiPersistenceSubsystem::SetWorldConfig(
    const int64 WorldSeed,
    const uint32 GenerationVersion
)
{
    WorldSeedValue = WorldSeed;
    GenerationVersionValue = GenerationVersion;
    bHasWorldConfig = true;

    if (Backend.IsValid())
    {
        Backend->SetWorldConfig(WorldSeed, GenerationVersion);
    }
}

void UOrakaiPersistenceSubsystem::RecordPlayerCoordinate(
    const FVector& Location,
    const float Yaw,
    const float Pitch
)
{
    PendingCoordinate.Location = Location;
    PendingCoordinate.Yaw = Yaw;
    PendingCoordinate.Pitch = Pitch;
    bHasPendingCoordinate = true;
}

void UOrakaiPersistenceSubsystem::RecordPlayerLocation(
    const FVector Location,
    const float Yaw,
    const float Pitch
)
{
    RecordPlayerCoordinate(Location, Yaw, Pitch);
}

void UOrakaiPersistenceSubsystem::RecordVoxelEdit(
    const FIntVector& ChunkCoordinate,
    const FIntVector& LocalCoordinate,
    const int32 MaterialId,
    const bool bIsWater
)
{
    if (!Backend.IsValid())
    {
        return;
    }

    FOrakaiVoxelEdit Edit;
    Edit.ChunkCoordinate = ChunkCoordinate;
    Edit.LocalCoordinate = LocalCoordinate;
    Edit.MaterialId = MaterialId;
    Edit.bIsWater = bIsWater;
    Backend->RecordVoxelEdit(Edit);
}

void UOrakaiPersistenceSubsystem::ClearVoxelEdit(
    const FIntVector& ChunkCoordinate,
    const FIntVector& LocalCoordinate
)
{
    if (Backend.IsValid())
    {
        Backend->ClearVoxelEdit(ChunkCoordinate, LocalCoordinate);
    }
}

void UOrakaiPersistenceSubsystem::RecordDensityEdit(
    const FIntVector& WorldSample,
    const float DensityDelta,
    const int32 MaterialId
)
{
    if (!Backend.IsValid())
    {
        return;
    }

    FOrakaiDensityEdit Edit;
    Edit.WorldSample = WorldSample;
    Edit.DensityDelta = DensityDelta;
    Edit.MaterialId = MaterialId;
    Backend->RecordDensityEdit(Edit);
}

void UOrakaiPersistenceSubsystem::ClearDensityEdit(
    const FIntVector& WorldSample
)
{
    if (Backend.IsValid())
    {
        Backend->ClearDensityEdit(WorldSample);
    }
}

void UOrakaiPersistenceSubsystem::GetVoxelEditsForChunk(
    const FIntVector& ChunkCoordinate,
    TArray<FOrakaiVoxelEdit>& OutEdits
) const
{
    if (Backend.IsValid())
    {
        Backend->GetVoxelEditsForChunk(ChunkCoordinate, OutEdits);
    }
    else
    {
        OutEdits.Reset();
    }
}

void UOrakaiPersistenceSubsystem::GetDensityEdits(
    TArray<FOrakaiDensityEdit>& OutEdits
) const
{
    if (Backend.IsValid())
    {
        Backend->GetDensityEdits(OutEdits);
    }
    else
    {
        OutEdits.Reset();
    }
}

void UOrakaiPersistenceSubsystem::GetFoliageEditsForChunk(
    const FIntVector& ChunkCoordinate,
    TArray<FOrakaiFoliageEdit>& OutEdits
) const
{
    if (Backend.IsValid())
    {
        Backend->GetFoliageEditsForChunk(ChunkCoordinate, OutEdits);
    }
    else
    {
        OutEdits.Reset();
    }
}

void UOrakaiPersistenceSubsystem::RecordFoliageEdit(
    const FIntVector& WorldVoxel,
    const bool bRemoved,
    const int32 TypeId,
    const float RotationYaw,
    const float Scale
)
{
    if (!Backend.IsValid())
    {
        return;
    }

    FOrakaiFoliageEdit Edit;
    Edit.WorldVoxel = WorldVoxel;
    Edit.bRemoved = bRemoved;
    Edit.TypeId = TypeId;
    Edit.RotationYaw = RotationYaw;
    Edit.Scale = Scale;
    Backend->RecordFoliageEdit(Edit);
}

void UOrakaiPersistenceSubsystem::ClearFoliageEdit(const FIntVector& WorldVoxel)
{
    if (Backend.IsValid())
    {
        Backend->ClearFoliageEdit(WorldVoxel);
    }
}

int32 UOrakaiPersistenceSubsystem::GetInventoryQuantity(
    const FName ItemId
) const
{
    return Backend.IsValid()
        ? Backend->GetInventoryQuantity(ItemId)
        : 0;
}

void UOrakaiPersistenceSubsystem::SetInventoryQuantity(
    const FName ItemId,
    const int32 Quantity
)
{
    if (Backend.IsValid())
    {
        Backend->SetInventoryQuantity(ItemId, Quantity);
    }
}

FString UOrakaiPersistenceSubsystem::MakeGeneratedWorldObjectId(
    const int64 WorldSeed,
    const FName TypeId,
    const FIntVector StableCoordinate
) const
{
    return OrakaiPersistence::MakeGeneratedWorldObjectId(
        WorldSeed,
        TypeId,
        StableCoordinate
    );
}

void UOrakaiPersistenceSubsystem::RecordWorldObject(
    const FOrakaiWorldObjectRecord& Record
)
{
    if (Backend.IsValid())
    {
        Backend->RecordWorldObject(Record);
    }
}

FString UOrakaiPersistenceSubsystem::RecordPlacedWorldObject(
    const FName TypeId,
    const FIntVector ChunkCoordinate,
    const FTransform Transform,
    const FString& Payload
)
{
    if (!Backend.IsValid() || TypeId.IsNone())
    {
        return FString();
    }

    FOrakaiWorldObjectRecord Record;
    Record.ObjectId = OrakaiPersistence::MakePlacedWorldObjectId();
    Record.TypeId = TypeId;
    Record.ChunkCoordinate = ChunkCoordinate;
    Record.Transform = Transform;
    Record.bGenerated = false;
    Record.bDestroyed = false;
    Record.Payload = Payload;
    Backend->RecordWorldObject(Record);
    return Record.ObjectId;
}

FString UOrakaiPersistenceSubsystem::TombstoneGeneratedWorldObject(
    const int64 WorldSeed,
    const FName TypeId,
    const FIntVector StableCoordinate,
    const FIntVector ChunkCoordinate,
    const FTransform GeneratedTransform
)
{
    if (!Backend.IsValid() || TypeId.IsNone())
    {
        return FString();
    }

    FOrakaiWorldObjectRecord Record;
    Record.ObjectId = OrakaiPersistence::MakeGeneratedWorldObjectId(
        WorldSeed,
        TypeId,
        StableCoordinate
    );
    Record.TypeId = TypeId;
    Record.ChunkCoordinate = ChunkCoordinate;
    Record.Transform = GeneratedTransform;
    Record.bGenerated = true;
    Record.bDestroyed = true;
    Backend->RecordWorldObject(Record);
    return Record.ObjectId;
}

bool UOrakaiPersistenceSubsystem::DestroyWorldObject(
    const FString& ObjectId
)
{
    if (!Backend.IsValid() || ObjectId.IsEmpty())
    {
        return false;
    }

    FOrakaiWorldObjectRecord Record;
    if (!Backend->GetWorldObject(ObjectId, Record))
    {
        return false;
    }

    if (Record.bGenerated)
    {
        Record.bDestroyed = true;
        Backend->RecordWorldObject(Record);
    }
    else
    {
        Backend->ClearWorldObject(ObjectId);
    }

    return true;
}

void UOrakaiPersistenceSubsystem::ClearWorldObjectDelta(
    const FString& ObjectId
)
{
    if (Backend.IsValid())
    {
        Backend->ClearWorldObject(ObjectId);
    }
}

bool UOrakaiPersistenceSubsystem::GetWorldObject(
    const FString& ObjectId,
    FOrakaiWorldObjectRecord& OutRecord
) const
{
    return Backend.IsValid() && Backend->GetWorldObject(ObjectId, OutRecord);
}

TArray<FOrakaiWorldObjectRecord>
UOrakaiPersistenceSubsystem::GetWorldObjectsForChunk(
    const FIntVector ChunkCoordinate
) const
{
    TArray<FOrakaiWorldObjectRecord> Records;
    if (Backend.IsValid())
    {
        Backend->GetWorldObjectsForChunk(ChunkCoordinate, Records);
    }
    return Records;
}

bool UOrakaiPersistenceSubsystem::HandleTick(const float DeltaSeconds)
{
    if (Backend.IsValid())
    {
        Backend->Tick(DeltaSeconds);

        // On a fresh connection, resend the cached world config and force the
        // player coordinate to be pushed again.
        const bool bNowConnected = Backend->IsConnected();
        if (bNowConnected && !bBackendWasConnected)
        {
            if (bHasWorldConfig)
            {
                Backend->SetWorldConfig(WorldSeedValue, GenerationVersionValue);
            }
            bHasSentCoordinate = false;
        }
        bBackendWasConnected = bNowConnected;
    }

    TimeSinceLastPositionSend += DeltaSeconds;
    FlushPendingCoordinate();

    return true;
}

void UOrakaiPersistenceSubsystem::FlushPendingCoordinate()
{
    if (!bHasPendingCoordinate || !Backend.IsValid())
    {
        return;
    }

    if (TimeSinceLastPositionSend < PositionSendInterval)
    {
        return;
    }

    const bool bMovedEnough =
        !bHasSentCoordinate ||
        FVector::Dist(PendingCoordinate.Location, LastSentLocation) >=
            PositionSendDistanceThreshold;

    if (!bMovedEnough)
    {
        return;
    }

    Backend->RecordPlayerCoordinate(PendingCoordinate);

    LastSentLocation = PendingCoordinate.Location;
    bHasSentCoordinate = true;
    bHasPendingCoordinate = false;
    TimeSinceLastPositionSend = 0.0f;
}
