#include "CubusCore/Persistence/OrakaiPersistenceSubsystem.h"

#include "CubusCore/Persistence/OrakaiLoggingPersistenceBackend.h"
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

    // Default to the logging backend so the game runs without the SpacetimeDB
    // SDK. SetBackend() swaps in the real transport when it is available.
    Backend = MakeUnique<FOrakaiLoggingPersistenceBackend>();
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
