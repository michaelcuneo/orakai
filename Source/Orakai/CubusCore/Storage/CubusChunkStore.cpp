#include "CubusCore/Storage/CubusChunkStore.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"

#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace CubusChunkStore
{
    constexpr uint32 Magic = 0x43425553u; // "CBUS"
    constexpr int32 BytesPerVoxel = sizeof(uint16) + sizeof(uint8);

    void SerializeHeader(
        FArchive& Archive,
        uint32& InOutMagic,
        uint32& InOutFormatVersion,
        int64& InOutWorldSeed,
        uint32& InOutGenerationVersion,
        FIntVector& InOutChunkCoordinate,
        int32& InOutVoxelCount,
        int32& InOutPayloadSize,
        uint32& InOutPayloadCrc
    )
    {
        Archive << InOutMagic;
        Archive << InOutFormatVersion;
        Archive << InOutWorldSeed;
        Archive << InOutGenerationVersion;
        Archive << InOutChunkCoordinate;
        Archive << InOutVoxelCount;
        Archive << InOutPayloadSize;
        Archive << InOutPayloadCrc;
    }

    bool ValidateVoxelCount(const int32 VoxelCount)
    {
        return VoxelCount == Cubus::ChunkSize * Cubus::ChunkSize * Cubus::ChunkSize;
    }
}

bool FCubusChunkStore::SaveChunk(
    const FCubusBlockChunkData& Chunk,
    const FCubusChunkStoreContext& Context
)
{
    const TConstArrayView<FCubusBlockVoxel> Voxels = Chunk.GetVoxelView();
    const int32 VoxelCount = Voxels.Num();

    if (!CubusChunkStore::ValidateVoxelCount(VoxelCount))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cubus chunk store refused save for (%d, %d, %d): unexpected voxel count %d"),
            Chunk.GetChunkCoordinate().X,
            Chunk.GetChunkCoordinate().Y,
            Chunk.GetChunkCoordinate().Z,
            VoxelCount
        );
        return false;
    }

    TArray<uint8> Payload;
    Payload.Reserve(VoxelCount * CubusChunkStore::BytesPerVoxel);

    FMemoryWriter PayloadWriter(Payload, true);

    for (const FCubusBlockVoxel& Voxel : Voxels)
    {
        uint16 MaterialId = static_cast<uint16>(
            FMath::Clamp(Voxel.MaterialId, 0, 65535)
        );
        uint8 Flags = Voxel.Flags;

        PayloadWriter << MaterialId;
        PayloadWriter << Flags;
    }

    PayloadWriter.Close();

    uint32 HeaderMagic = CubusChunkStore::Magic;
    uint32 FormatVersion = CurrentFormatVersion;
    int64 WorldSeed = Context.WorldSeed;
    uint32 GenerationVersion = Context.GenerationVersion;
    FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    int32 PayloadSize = Payload.Num();
    uint32 PayloadCrc = FCrc::MemCrc32(Payload.GetData(), Payload.Num());

    TArray<uint8> FileData;
    FMemoryWriter FileWriter(FileData, true);

    CubusChunkStore::SerializeHeader(
        FileWriter,
        HeaderMagic,
        FormatVersion,
        WorldSeed,
        GenerationVersion,
        ChunkCoordinate,
        VoxelCount,
        PayloadSize,
        PayloadCrc
    );

    FileWriter.Serialize(Payload.GetData(), Payload.Num());
    FileWriter.Close();

    const FString StoreDirectory = GetWorldStoreDirectory(Context);
    IFileManager::Get().MakeDirectory(*StoreDirectory, true);

    const FString FinalPath = GetChunkPath(ChunkCoordinate, Context);
    const FString TemporaryPath = FinalPath + TEXT(".tmp");

    if (!FFileHelper::SaveArrayToFile(FileData, *TemporaryPath))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cubus chunk store failed temporary write: %s"),
            *TemporaryPath
        );
        return false;
    }

    IFileManager::Get().Delete(*FinalPath, false, true, true);

    if (!IFileManager::Get().Move(*FinalPath, *TemporaryPath, true, true, false, true))
    {
        IFileManager::Get().Delete(*TemporaryPath, false, true, true);
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Cubus chunk store failed atomic move: %s"),
            *FinalPath
        );
        return false;
    }

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT("Cubus chunk store saved (%d, %d, %d): %d bytes"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        FileData.Num()
    );

    return true;
}

bool FCubusChunkStore::LoadChunk(
    FCubusBlockChunkData& Chunk,
    const FCubusChunkStoreContext& Context
)
{
    const FIntVector ExpectedCoordinate = Chunk.GetChunkCoordinate();
    const FString ChunkPath = GetChunkPath(ExpectedCoordinate, Context);

    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *ChunkPath))
    {
        return false;
    }

    FMemoryReader Reader(FileData, true);

    uint32 HeaderMagic = 0;
    uint32 FormatVersion = 0;
    int64 WorldSeed = 0;
    uint32 GenerationVersion = 0;
    FIntVector StoredCoordinate = FIntVector::ZeroValue;
    int32 VoxelCount = 0;
    int32 PayloadSize = 0;
    uint32 StoredPayloadCrc = 0;

    CubusChunkStore::SerializeHeader(
        Reader,
        HeaderMagic,
        FormatVersion,
        WorldSeed,
        GenerationVersion,
        StoredCoordinate,
        VoxelCount,
        PayloadSize,
        StoredPayloadCrc
    );

    const int64 PayloadOffset = Reader.Tell();
    const int64 RemainingBytes = Reader.TotalSize() - PayloadOffset;

    if (
        Reader.IsError() ||
        HeaderMagic != CubusChunkStore::Magic ||
        FormatVersion != CurrentFormatVersion ||
        WorldSeed != Context.WorldSeed ||
        GenerationVersion != Context.GenerationVersion ||
        StoredCoordinate != ExpectedCoordinate ||
        !CubusChunkStore::ValidateVoxelCount(VoxelCount) ||
        PayloadSize != VoxelCount * CubusChunkStore::BytesPerVoxel ||
        RemainingBytes != PayloadSize
    )
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cubus chunk store rejected stale or invalid cache: %s"),
            *ChunkPath
        );
        return false;
    }

    const uint8* PayloadData = FileData.GetData() + PayloadOffset;
    const uint32 ActualPayloadCrc = FCrc::MemCrc32(PayloadData, PayloadSize);

    if (ActualPayloadCrc != StoredPayloadCrc)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cubus chunk store rejected corrupt cache: %s"),
            *ChunkPath
        );
        return false;
    }

    Chunk.Clear();

    for (int32 Index = 0; Index < VoxelCount; ++Index)
    {
        uint16 MaterialId = 0;
        uint8 Flags = 0;

        Reader << MaterialId;
        Reader << Flags;

        const int32 LocalX = Index % Cubus::ChunkSize;
        const int32 LocalY = (Index / Cubus::ChunkSize) % Cubus::ChunkSize;
        const int32 LocalZ = Index / (Cubus::ChunkSize * Cubus::ChunkSize);

        FCubusBlockVoxel Voxel;
        Voxel.MaterialId = static_cast<int32>(MaterialId);
        Voxel.Flags = Flags;
        Chunk.SetVoxel(LocalX, LocalY, LocalZ, Voxel);
    }

    Reader.Close();
    Chunk.ClearVegetationInstances();

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT("Cubus chunk store loaded (%d, %d, %d)"),
        ExpectedCoordinate.X,
        ExpectedCoordinate.Y,
        ExpectedCoordinate.Z
    );

    return true;
}

bool FCubusChunkStore::HasChunk(
    const FIntVector& ChunkCoordinate,
    const FCubusChunkStoreContext& Context
)
{
    return IFileManager::Get().FileExists(
        *GetChunkPath(ChunkCoordinate, Context)
    );
}

bool FCubusChunkStore::DeleteChunk(
    const FIntVector& ChunkCoordinate,
    const FCubusChunkStoreContext& Context
)
{
    return IFileManager::Get().Delete(
        *GetChunkPath(ChunkCoordinate, Context),
        false,
        true,
        true
    );
}

FString FCubusChunkStore::GetChunkPath(
    const FIntVector& ChunkCoordinate,
    const FCubusChunkStoreContext& Context
)
{
    return FPaths::Combine(
        GetWorldStoreDirectory(Context),
        FString::Printf(
            TEXT("chunk_%d_%d_%d.cubus"),
            ChunkCoordinate.X,
            ChunkCoordinate.Y,
            ChunkCoordinate.Z
        )
    );
}

FString FCubusChunkStore::GetWorldStoreDirectory(
    const FCubusChunkStoreContext& Context
)
{
    return FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("Cubus"),
        FString::Printf(
            TEXT("world_%lld_v%u"),
            static_cast<long long>(Context.WorldSeed),
            Context.GenerationVersion
        )
    );
}
