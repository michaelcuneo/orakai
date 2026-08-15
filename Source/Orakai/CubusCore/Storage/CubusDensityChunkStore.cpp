#include "CubusCore/Storage/CubusDensityChunkStore.h"

#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Data/CubusDensitySample.h"
#include "CubusCore/Storage/CubusChunkStore.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformTLS.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace CubusDensityChunkStore
{
    constexpr uint32 Magic = 0x43444E53u; // "CDNS"
    constexpr int32 BytesPerSample = sizeof(float) + sizeof(int32);

    void SerializeHeader(
        FArchive& Archive,
        uint32& InOutMagic,
        uint32& InOutFormatVersion,
        int64& InOutWorldSeed,
        uint32& InOutGenerationVersion,
        FIntVector& InOutChunkCoordinate,
        float& InOutVoxelSize,
        int32& InOutSubdivisionsPerVoxel,
        int32& InOutSampleDimension,
        int32& InOutSampleCount,
        FVector& InOutSampleOffsetInVoxels,
        int32& InOutPayloadSize,
        uint32& InOutPayloadCrc
    )
    {
        Archive << InOutMagic;
        Archive << InOutFormatVersion;
        Archive << InOutWorldSeed;
        Archive << InOutGenerationVersion;
        Archive << InOutChunkCoordinate;
        Archive << InOutVoxelSize;
        Archive << InOutSubdivisionsPerVoxel;
        Archive << InOutSampleDimension;
        Archive << InOutSampleCount;
        Archive << InOutSampleOffsetInVoxels;
        Archive << InOutPayloadSize;
        Archive << InOutPayloadCrc;
    }

    bool IsSupportedContext(const FCubusDensityChunkStoreContext& Context)
    {
        return
  Context.SubdivisionsPerVoxel == 1 &&
  FMath::IsFinite(Context.VoxelSize) &&
  Context.VoxelSize >= 1.0f;
    }
}

bool FCubusDensityChunkStore::SaveBuffer(
    const FCubusDensitySamplingBuffer& Buffer,
    const FCubusDensityChunkStoreContext& Context
)
{
    if (!Buffer.IsBuilt() || !CubusDensityChunkStore::IsSupportedContext(Context))
    {
        return false;
    }

    const TConstArrayView<FCubusDensitySample> Samples = Buffer.GetSamples();
    if (Samples.Num() != FCubusDensitySamplingBuffer::SampleCount)
    {
        return false;
    }

    TArray<uint8> Payload;
    Payload.Reserve(Samples.Num() * CubusDensityChunkStore::BytesPerSample);
    FMemoryWriter PayloadWriter(Payload, true);

    for (const FCubusDensitySample& Sample : Samples)
    {
        if (!FMath::IsFinite(Sample.Density) || Sample.MaterialId < 0)
        {
  return false;
        }

        float Density = Sample.Density;
        int32 MaterialId = Sample.MaterialId;
        PayloadWriter << Density;
        PayloadWriter << MaterialId;
    }
    PayloadWriter.Close();

    uint32 HeaderMagic = CubusDensityChunkStore::Magic;
    uint32 FormatVersion = CurrentFormatVersion;
    int64 WorldSeed = Context.WorldSeed;
    uint32 GenerationVersion = Context.GenerationVersion;
    FIntVector ChunkCoordinate = Buffer.GetChunkCoordinate();
    float VoxelSize = Context.VoxelSize;
    int32 SubdivisionsPerVoxel = Context.SubdivisionsPerVoxel;
    int32 SampleDimension = FCubusDensitySamplingBuffer::SampleDimension;
    int32 SampleCount = Samples.Num();
    FVector SampleOffsetInVoxels = Buffer.GetSampleOffsetInVoxels();
    int32 PayloadSize = Payload.Num();
    uint32 PayloadCrc = FCrc::MemCrc32(Payload.GetData(), Payload.Num());

    TArray<uint8> FileData;
    FMemoryWriter Writer(FileData, true);
    CubusDensityChunkStore::SerializeHeader(
        Writer,
        HeaderMagic,
        FormatVersion,
        WorldSeed,
        GenerationVersion,
        ChunkCoordinate,
        VoxelSize,
        SubdivisionsPerVoxel,
        SampleDimension,
        SampleCount,
        SampleOffsetInVoxels,
        PayloadSize,
        PayloadCrc
    );
    Writer.Serialize(Payload.GetData(), Payload.Num());
    Writer.Close();

    const FString StoreDirectory = GetDensityStoreDirectory(Context);
    IFileManager::Get().MakeDirectory(*StoreDirectory, true);

    const FString FinalPath = GetBufferPath(ChunkCoordinate, Context);
    const FString TemporaryPath = FinalPath + FString::Printf(
        TEXT(".%u.tmp"),
        FPlatformTLS::GetCurrentThreadId()
    );

    if (!FFileHelper::SaveArrayToFile(FileData, *TemporaryPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("Cubus density cache failed temporary write: %s"), *TemporaryPath);
        return false;
    }

    IFileManager::Get().Delete(*FinalPath, false, true, true);
    if (!IFileManager::Get().Move(*FinalPath, *TemporaryPath, true, true, false, true))
    {
        IFileManager::Get().Delete(*TemporaryPath, false, true, true);
        UE_LOG(LogTemp, Warning, TEXT("Cubus density cache failed atomic move: %s"), *FinalPath);
        return false;
    }

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT("Cubus density cache saved (%d, %d, %d): %d samples, %d bytes"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        SampleCount,
        FileData.Num()
    );
    return true;
}

bool FCubusDensityChunkStore::LoadBuffer(
    const FIntVector& ChunkCoordinate,
    const FCubusDensityChunkStoreContext& Context,
    FCubusDensitySamplingBuffer& OutBuffer
)
{
    if (!CubusDensityChunkStore::IsSupportedContext(Context))
    {
        return false;
    }

    const FString Path = GetBufferPath(ChunkCoordinate, Context);
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *Path))
    {
        return false;
    }

    FMemoryReader Reader(FileData, true);

    uint32 HeaderMagic = 0;
    uint32 FormatVersion = 0;
    int64 WorldSeed = 0;
    uint32 GenerationVersion = 0;
    FIntVector StoredCoordinate = FIntVector::ZeroValue;
    float StoredVoxelSize = 0.0f;
    int32 StoredSubdivisions = 0;
    int32 StoredSampleDimension = 0;
    int32 StoredSampleCount = 0;
    FVector StoredSampleOffset = FVector::ZeroVector;
    int32 PayloadSize = 0;
    uint32 StoredPayloadCrc = 0;

    CubusDensityChunkStore::SerializeHeader(
        Reader,
        HeaderMagic,
        FormatVersion,
        WorldSeed,
        GenerationVersion,
        StoredCoordinate,
        StoredVoxelSize,
        StoredSubdivisions,
        StoredSampleDimension,
        StoredSampleCount,
        StoredSampleOffset,
        PayloadSize,
        StoredPayloadCrc
    );

    const int64 PayloadOffset = Reader.Tell();
    const int64 RemainingBytes = Reader.TotalSize() - PayloadOffset;
    const bool bHeaderValid =
        !Reader.IsError() &&
        HeaderMagic == CubusDensityChunkStore::Magic &&
        FormatVersion == CurrentFormatVersion &&
        WorldSeed == Context.WorldSeed &&
        GenerationVersion == Context.GenerationVersion &&
        StoredCoordinate == ChunkCoordinate &&
        FMath::IsNearlyEqual(StoredVoxelSize, Context.VoxelSize, 0.001f) &&
        StoredSubdivisions == Context.SubdivisionsPerVoxel &&
        StoredSampleDimension == FCubusDensitySamplingBuffer::SampleDimension &&
        StoredSampleCount == FCubusDensitySamplingBuffer::SampleCount &&
        !StoredSampleOffset.ContainsNaN() &&
        PayloadSize == StoredSampleCount * CubusDensityChunkStore::BytesPerSample &&
        RemainingBytes == PayloadSize;

    if (!bHeaderValid)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cubus density cache rejected stale or invalid baseline: %s"), *Path);
        return false;
    }

    const uint8* PayloadData = FileData.GetData() + PayloadOffset;
    if (FCrc::MemCrc32(PayloadData, PayloadSize) != StoredPayloadCrc)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cubus density cache rejected corrupt baseline: %s"), *Path);
        return false;
    }

    TArray<FCubusDensitySample> Samples;
    Samples.SetNumUninitialized(StoredSampleCount);

    for (FCubusDensitySample& Sample : Samples)
    {
        Reader << Sample.Density;
        Reader << Sample.MaterialId;
        if (Reader.IsError() || !FMath::IsFinite(Sample.Density) || Sample.MaterialId < 0)
        {
  return false;
        }
    }
    Reader.Close();

    if (!OutBuffer.RestoreGeneratedBaseline(StoredCoordinate, StoredSampleOffset, MoveTemp(Samples)))
    {
        return false;
    }

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT("Cubus density cache hit (%d, %d, %d), seed %lld, generation %u"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        static_cast<long long>(Context.WorldSeed),
        Context.GenerationVersion
    );
    return true;
}

bool FCubusDensityChunkStore::HasBuffer(
    const FIntVector& ChunkCoordinate,
    const FCubusDensityChunkStoreContext& Context
)
{
    return IFileManager::Get().FileExists(*GetBufferPath(ChunkCoordinate, Context));
}

bool FCubusDensityChunkStore::DeleteBuffer(
    const FIntVector& ChunkCoordinate,
    const FCubusDensityChunkStoreContext& Context
)
{
    return IFileManager::Get().Delete(*GetBufferPath(ChunkCoordinate, Context), false, true, true);
}

FString FCubusDensityChunkStore::GetBufferPath(
    const FIntVector& ChunkCoordinate,
    const FCubusDensityChunkStoreContext& Context
)
{
    return FPaths::Combine(
        GetDensityStoreDirectory(Context),
        FString::Printf(
  TEXT("chunk_%d_%d_%d.cubusd"),
  ChunkCoordinate.X,
  ChunkCoordinate.Y,
  ChunkCoordinate.Z
        )
    );
}

FString FCubusDensityChunkStore::GetDensityStoreDirectory(
    const FCubusDensityChunkStoreContext& Context
)
{
    FCubusChunkStoreContext BaseContext;
    BaseContext.WorldSeed = Context.WorldSeed;
    BaseContext.GenerationVersion = Context.GenerationVersion;

    const int32 VoxelSizeMilliCentimetres = FMath::RoundToInt(Context.VoxelSize * 1000.0f);
    return FPaths::Combine(
        FCubusChunkStore::GetWorldStoreDirectory(BaseContext),
        TEXT("Density"),
        FString::Printf(
  TEXT("s%d_v%d"),
  Context.SubdivisionsPerVoxel,
  VoxelSizeMilliCentimetres
        )
    );
}
