#include "CubusCore/Generation/CubusHydrologyField.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace CubusHydrologyField
{
    constexpr float FillEpsilon = 0.0005f;

    struct FHydrologyRegion
    {
        FIntPoint RegionCoordinate = FIntPoint::ZeroValue;
        int32 InteriorCellCount = 0;
        int32 HaloCellCount = 0;
        int32 GridSize = 0;
        float CellSize = 16.0f;
        FVector2D WorldOrigin = FVector2D::ZeroVector;

        TArray<float> RawHeight;
        TArray<float> FilledHeight;
        TArray<float> Accumulation;
        TArray<int32> Receiver;

        int32 Index(const int32 X, const int32 Y) const
        {
            return Y * GridSize + X;
        }

        FVector2D CellCentre(const int32 X, const int32 Y) const
        {
            return WorldOrigin + FVector2D(
                (static_cast<float>(X) + 0.5f) * CellSize,
                (static_cast<float>(Y) + 0.5f) * CellSize
            );
        }
    };

    struct FHeapNode
    {
        float Height = 0.0f;
        int32 Index = INDEX_NONE;
    };

    void HeapPush(TArray<FHeapNode>& Heap, const FHeapNode& Node)
    {
        int32 Child = Heap.Add(Node);
        while (Child > 0)
        {
            const int32 Parent = (Child - 1) / 2;
            if (Heap[Parent].Height <= Heap[Child].Height)
            {
                break;
            }
            Swap(Heap[Parent], Heap[Child]);
            Child = Parent;
        }
    }

    FHeapNode HeapPop(TArray<FHeapNode>& Heap)
    {
        check(!Heap.IsEmpty());

        const FHeapNode Result = Heap[0];
        const FHeapNode Last = Heap.Pop(EAllowShrinking::No);
        if (Heap.IsEmpty())
        {
            return Result;
        }

        Heap[0] = Last;
        int32 Parent = 0;
        for (;;)
        {
            const int32 Left = Parent * 2 + 1;
            if (Left >= Heap.Num())
            {
                break;
            }

            const int32 Right = Left + 1;
            int32 Smallest = Left;
            if (Right < Heap.Num() && Heap[Right].Height < Heap[Left].Height)
            {
                Smallest = Right;
            }

            if (Heap[Parent].Height <= Heap[Smallest].Height)
            {
                break;
            }

            Swap(Heap[Parent], Heap[Smallest]);
            Parent = Smallest;
        }

        return Result;
    }

    int32 FloorDiv(const float Value, const float Divisor)
    {
        return FMath::FloorToInt(Value / FMath::Max(Divisor, UE_SMALL_NUMBER));
    }

    float HashJitter(const int32 X, const int32 Y, const int32 Seed)
    {
        uint32 Value = static_cast<uint32>(X) * 0x9E3779B9u;
        Value ^= static_cast<uint32>(Y) * 0x85EBCA6Bu;
        Value ^= static_cast<uint32>(Seed) * 0xC2B2AE35u;
        Value ^= Value >> 16;
        Value *= 0x7FEB352Du;
        Value ^= Value >> 15;
        return static_cast<float>(Value & 0xffffu) / 65535.0f;
    }

    TSharedPtr<const FHydrologyRegion, ESPMode::ThreadSafe> BuildRegion(
        const FIntPoint& RegionCoordinate,
        const FCubusHydrologySettings& InSettings
    )
    {
        FCubusHydrologySettings Settings = InSettings;
        Settings.CellSize = FMath::Max(1.0f, Settings.CellSize);
        Settings.RegionCellCount = FMath::Clamp(Settings.RegionCellCount, 16, 256);
        Settings.HaloCellCount = FMath::Clamp(Settings.HaloCellCount, 8, 128);

        TSharedPtr<FHydrologyRegion, ESPMode::ThreadSafe> Region =
            MakeShared<FHydrologyRegion, ESPMode::ThreadSafe>();
        Region->RegionCoordinate = RegionCoordinate;
        Region->InteriorCellCount = Settings.RegionCellCount;
        Region->HaloCellCount = Settings.HaloCellCount;
        Region->GridSize = Settings.RegionCellCount + Settings.HaloCellCount * 2;
        Region->CellSize = Settings.CellSize;

        const float RegionWorldSize =
            static_cast<float>(Settings.RegionCellCount) * Settings.CellSize;
        Region->WorldOrigin = FVector2D(
            static_cast<float>(RegionCoordinate.X) * RegionWorldSize -
                static_cast<float>(Settings.HaloCellCount) * Settings.CellSize,
            static_cast<float>(RegionCoordinate.Y) * RegionWorldSize -
                static_cast<float>(Settings.HaloCellCount) * Settings.CellSize
        );

        const int32 SampleCount = Region->GridSize * Region->GridSize;
        Region->RawHeight.SetNumUninitialized(SampleCount);
        Region->FilledHeight.SetNumUninitialized(SampleCount);
        Region->Accumulation.Init(1.0f, SampleCount);
        Region->Receiver.Init(INDEX_NONE, SampleCount);

        TArray<uint8> Visited;
        Visited.Init(0, SampleCount);

        TArray<FHeapNode> Heap;
        Heap.Reserve(SampleCount / 4);

        for (int32 Y = 0; Y < Region->GridSize; ++Y)
        {
            for (int32 X = 0; X < Region->GridSize; ++X)
            {
                const int32 Index = Region->Index(X, Y);
                const FVector2D Centre = Region->CellCentre(X, Y);
                const float TerrainX = Centre.X + static_cast<float>(Settings.TerrainOffsetX);
                const float TerrainY = Centre.Y + static_cast<float>(Settings.TerrainOffsetY);
                const float RawHeight = FCubusTerrainForm::Sample(
                    TerrainX,
                    TerrainY,
                    Settings.TerrainFormSettings
                ).Height;

                Region->RawHeight[Index] = RawHeight;
                Region->FilledHeight[Index] = RawHeight;
            }
        }

        auto SeedBoundary = [&](const int32 X, const int32 Y)
        {
            const int32 Index = Region->Index(X, Y);
            if (Visited[Index] != 0)
            {
                return;
            }

            Visited[Index] = 1;
            HeapPush(Heap, {Region->FilledHeight[Index], Index});
        };

        for (int32 X = 0; X < Region->GridSize; ++X)
        {
            SeedBoundary(X, 0);
            SeedBoundary(X, Region->GridSize - 1);
        }
        for (int32 Y = 1; Y < Region->GridSize - 1; ++Y)
        {
            SeedBoundary(0, Y);
            SeedBoundary(Region->GridSize - 1, Y);
        }

        static const FIntPoint Neighbours[8] =
        {
            FIntPoint(-1, -1), FIntPoint(0, -1), FIntPoint(1, -1),
            FIntPoint(-1, 0),                         FIntPoint(1, 0),
            FIntPoint(-1, 1),  FIntPoint(0, 1),  FIntPoint(1, 1)
        };

        while (!Heap.IsEmpty())
        {
            const FHeapNode Current = HeapPop(Heap);
            const int32 CurrentX = Current.Index % Region->GridSize;
            const int32 CurrentY = Current.Index / Region->GridSize;

            for (const FIntPoint& Offset : Neighbours)
            {
                const int32 X = CurrentX + Offset.X;
                const int32 Y = CurrentY + Offset.Y;
                if (X < 0 || Y < 0 || X >= Region->GridSize || Y >= Region->GridSize)
                {
                    continue;
                }

                const int32 Index = Region->Index(X, Y);
                if (Visited[Index] != 0)
                {
                    continue;
                }

                Visited[Index] = 1;
                const float Filled = FMath::Max(
                    Region->RawHeight[Index],
                    Current.Height + FillEpsilon
                );
                Region->FilledHeight[Index] = Filled;
                HeapPush(Heap, {Filled, Index});
            }
        }

        for (int32 Y = 1; Y < Region->GridSize - 1; ++Y)
        {
            for (int32 X = 1; X < Region->GridSize - 1; ++X)
            {
                const int32 Index = Region->Index(X, Y);
                const float CurrentHeight = Region->FilledHeight[Index];
                float BestScore = CurrentHeight;
                int32 BestIndex = INDEX_NONE;

                for (const FIntPoint& Offset : Neighbours)
                {
                    const int32 NeighbourX = X + Offset.X;
                    const int32 NeighbourY = Y + Offset.Y;
                    const int32 NeighbourIndex = Region->Index(NeighbourX, NeighbourY);
                    const float Jitter = HashJitter(
                        NeighbourX + RegionCoordinate.X * Settings.RegionCellCount,
                        NeighbourY + RegionCoordinate.Y * Settings.RegionCellCount,
                        Settings.RiverSeed
                    ) * FillEpsilon * 0.25f;
                    const float Score = Region->FilledHeight[NeighbourIndex] + Jitter;

                    if (Score < BestScore - FillEpsilon * 0.1f)
                    {
                        BestScore = Score;
                        BestIndex = NeighbourIndex;
                    }
                }

                Region->Receiver[Index] = BestIndex;
            }
        }

        TArray<int32> Order;
        Order.Reserve(SampleCount);
        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            Order.Add(Index);
        }
        Order.Sort([&](const int32 A, const int32 B)
        {
            if (!FMath::IsNearlyEqual(Region->FilledHeight[A], Region->FilledHeight[B], FillEpsilon * 0.1f))
            {
                return Region->FilledHeight[A] > Region->FilledHeight[B];
            }
            return A > B;
        });

        for (const int32 Index : Order)
        {
            const int32 Receiver = Region->Receiver[Index];
            if (Receiver != INDEX_NONE)
            {
                Region->Accumulation[Receiver] += Region->Accumulation[Index];
            }
        }

        return Region;
    }

    uint32 SettingsHash(const FCubusHydrologySettings& Settings)
    {
        uint32 Hash = GetTypeHash(Settings.RiverSeed);
        Hash = HashCombine(Hash, GetTypeHash(Settings.TerrainOffsetX));
        Hash = HashCombine(Hash, GetTypeHash(Settings.TerrainOffsetY));
        Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Settings.CellSize * 100.0f)));
        Hash = HashCombine(Hash, GetTypeHash(Settings.RegionCellCount));
        Hash = HashCombine(Hash, GetTypeHash(Settings.HaloCellCount));
        Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Settings.TerrainFormSettings.BaseHeight * 100.0f)));
        Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Settings.TerrainFormSettings.ContinentAmplitude * 100.0f)));
        Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Settings.TerrainFormSettings.RidgeAmplitude * 100.0f)));
        Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Settings.TerrainFormSettings.ValleyDepth * 100.0f)));
        return Hash;
    }

    struct FRegionCacheKey
    {
        FIntPoint Region = FIntPoint::ZeroValue;
        uint32 Settings = 0;

        bool operator==(const FRegionCacheKey& Other) const
        {
            return Region == Other.Region && Settings == Other.Settings;
        }
    };

    uint32 GetTypeHash(const FRegionCacheKey& Key)
    {
        uint32 Hash = ::GetTypeHash(Key.Region.X);
        Hash = HashCombine(Hash, ::GetTypeHash(Key.Region.Y));
        return HashCombine(Hash, ::GetTypeHash(Key.Settings));
    }

    FCriticalSection RegionCacheMutex;
    TMap<FRegionCacheKey, TSharedPtr<const FHydrologyRegion, ESPMode::ThreadSafe>> RegionCache;

    TSharedPtr<const FHydrologyRegion, ESPMode::ThreadSafe> GetRegion(
        const FIntPoint& RegionCoordinate,
        const FCubusHydrologySettings& Settings
    )
    {
        const FRegionCacheKey Key{RegionCoordinate, SettingsHash(Settings)};
        {
            FScopeLock Lock(&RegionCacheMutex);
            if (const TSharedPtr<const FHydrologyRegion, ESPMode::ThreadSafe>* Existing = RegionCache.Find(Key))
            {
                return *Existing;
            }
        }

        const TSharedPtr<const FHydrologyRegion, ESPMode::ThreadSafe> Built =
            BuildRegion(RegionCoordinate, Settings);

        FScopeLock Lock(&RegionCacheMutex);
        if (const TSharedPtr<const FHydrologyRegion, ESPMode::ThreadSafe>* Existing = RegionCache.Find(Key))
        {
            return *Existing;
        }
        RegionCache.Add(Key, Built);
        return Built;
    }

    float DistanceToSegment(
        const FVector2D& Point,
        const FVector2D& A,
        const FVector2D& B,
        float& OutAlpha
    )
    {
        const FVector2D Segment = B - A;
        const float LengthSquared = Segment.SizeSquared();
        OutAlpha = LengthSquared > UE_SMALL_NUMBER
            ? FMath::Clamp(FVector2D::DotProduct(Point - A, Segment) / LengthSquared, 0.0f, 1.0f)
            : 0.0f;
        return FVector2D::Distance(Point, A + Segment * OutAlpha);
    }
}

FCubusHydrologySample FCubusHydrologyField::Sample(
    const float WorldX,
    const float WorldY,
    const FCubusHydrologySettings& InSettings
)
{
    FCubusHydrologySample Result;
    if (!InSettings.bEnabled)
    {
        return Result;
    }

    FCubusHydrologySettings Settings = InSettings;
    Settings.CellSize = FMath::Max(1.0f, Settings.CellSize);
    Settings.RegionCellCount = FMath::Clamp(Settings.RegionCellCount, 16, 256);
    Settings.HaloCellCount = FMath::Clamp(Settings.HaloCellCount, 8, 128);
    Settings.SourceAccumulation = FMath::Max(1.0f, Settings.SourceAccumulation);
    Settings.MajorRiverAccumulation = FMath::Max(Settings.SourceAccumulation + 1.0f, Settings.MajorRiverAccumulation);
    Settings.ChannelHalfWidth = FMath::Max(Settings.CellSize * 0.08f, Settings.ChannelHalfWidth);
    Settings.ValleyHalfWidth = FMath::Max(Settings.ChannelHalfWidth + 1.0f, Settings.ValleyHalfWidth);

    const float RegionWorldSize = static_cast<float>(Settings.RegionCellCount) * Settings.CellSize;
    const FIntPoint RegionCoordinate(
        CubusHydrologyField::FloorDiv(WorldX, RegionWorldSize),
        CubusHydrologyField::FloorDiv(WorldY, RegionWorldSize)
    );

    const TSharedPtr<const CubusHydrologyField::FHydrologyRegion, ESPMode::ThreadSafe> Region =
        CubusHydrologyField::GetRegion(RegionCoordinate, Settings);
    if (!Region.IsValid())
    {
        return Result;
    }

    const FVector2D Query(WorldX, WorldY);
    const FVector2D Relative = Query - Region->WorldOrigin;
    const int32 CentreX = FMath::Clamp(FMath::FloorToInt(Relative.X / Region->CellSize), 0, Region->GridSize - 1);
    const int32 CentreY = FMath::Clamp(FMath::FloorToInt(Relative.Y / Region->CellSize), 0, Region->GridSize - 1);
    const int32 SearchRadius = FMath::CeilToInt(Settings.ValleyHalfWidth / Region->CellSize) + 2;

    float BestDistance = MAX_flt;
    for (int32 Y = FMath::Max(1, CentreY - SearchRadius); Y <= FMath::Min(Region->GridSize - 2, CentreY + SearchRadius); ++Y)
    {
        for (int32 X = FMath::Max(1, CentreX - SearchRadius); X <= FMath::Min(Region->GridSize - 2, CentreX + SearchRadius); ++X)
        {
            const int32 Index = Region->Index(X, Y);
            const float Accumulation = Region->Accumulation[Index];
            const int32 ReceiverIndex = Region->Receiver[Index];
            if (Accumulation < Settings.SourceAccumulation || ReceiverIndex == INDEX_NONE)
            {
                continue;
            }

            const int32 ReceiverX = ReceiverIndex % Region->GridSize;
            const int32 ReceiverY = ReceiverIndex / Region->GridSize;
            const FVector2D A = Region->CellCentre(X, Y);
            const FVector2D B = Region->CellCentre(ReceiverX, ReceiverY);
            float SegmentAlpha = 0.0f;
            const float Distance = CubusHydrologyField::DistanceToSegment(Query, A, B, SegmentAlpha);
            if (Distance >= BestDistance)
            {
                continue;
            }

            BestDistance = Distance;
            const float ReceiverAccumulation = Region->Accumulation[ReceiverIndex];
            const float InterpolatedAccumulation = FMath::Lerp(Accumulation, ReceiverAccumulation, SegmentAlpha);
            const float LogSource = FMath::Loge(Settings.SourceAccumulation);
            const float LogMajor = FMath::Loge(Settings.MajorRiverAccumulation);
            const float LogAccumulation = FMath::Loge(FMath::Max(Settings.SourceAccumulation, InterpolatedAccumulation));

            Result.DistanceToChannel = Distance;
            Result.FlowAccumulation = InterpolatedAccumulation;
            Result.ChannelStrength = FMath::Clamp(
                (LogAccumulation - LogSource) / FMath::Max(0.001f, LogMajor - LogSource),
                0.0f,
                1.0f
            );
            Result.HydraulicHeight = FMath::Lerp(
                Region->FilledHeight[Index],
                Region->FilledHeight[ReceiverIndex],
                SegmentAlpha
            );
            Result.FlowDirection = (B - A).GetSafeNormal();
        }
    }

    return Result;
}

float FCubusHydrologyField::NormalizeRiverDistance(
    const FCubusHydrologySample& Sample,
    const FCubusHydrologySettings& Settings
)
{
    if (!Sample.IsChannel())
    {
        return 1.0f;
    }

    const float ValleyWidth = FMath::Max(
        Settings.ChannelHalfWidth + 1.0f,
        FMath::Lerp(Settings.ChannelHalfWidth * 2.0f, Settings.ValleyHalfWidth, Sample.ChannelStrength)
    );
    return FMath::Clamp(Sample.DistanceToChannel / ValleyWidth, 0.0f, 1.0f);
}
