#include "CubusCore/Generation/CubusTerrainDrainage.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace CubusTerrainDrainage
{
    struct FRegion
    {
        FIntPoint Coordinate = FIntPoint::ZeroValue;
        int32 InteriorCellCount = 0;
        int32 HaloCellCount = 0;
        int32 GridSize = 0;
        float CellSizeMeters = 8.0f;
        FVector2D WorldOriginMeters = FVector2D::ZeroVector;

        TArray<float> RawHeight;
        TArray<float> FilledHeight;
        TArray<float> AccumulationCells;
        TArray<int32> Receiver;
        TArray<int32> StrahlerOrder;

        int32 Index(const int32 X, const int32 Y) const
        {
            return Y * GridSize + X;
        }

        FVector2D CellCentre(const int32 X, const int32 Y) const
        {
            return WorldOriginMeters + FVector2D(
                (static_cast<double>(X) + 0.5) * CellSizeMeters,
                (static_cast<double>(Y) + 0.5) * CellSizeMeters
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

    uint32 HashStructureSettings(const FCubusTerrainStructureSettings& Settings)
    {
        uint32 Hash = GetTypeHash(Settings.Seed);
        auto AddFloat = [&Hash](const float Value)
        {
            Hash = HashCombine(Hash, GetTypeHash(Value));
        };

        AddFloat(Settings.BaseElevationMeters);
        AddFloat(Settings.RegionalReliefMeters);
        AddFloat(Settings.RegionalScaleKm);
        AddFloat(Settings.MountainSystemSpacingKm);
        AddFloat(Settings.RangeNodeSpacingKm);
        AddFloat(Settings.RangeCoreHalfWidthKm);
        AddFloat(Settings.RangeShoulderHalfWidthKm);
        AddFloat(Settings.RangeReliefMeters);
        AddFloat(Settings.AlongSpineJitter);
        AddFloat(Settings.AcrossSpineJitter);
        AddFloat(Settings.BranchChance);
        AddFloat(Settings.BranchReliefScale);
        AddFloat(Settings.BranchWidthScale);
        AddFloat(Settings.MassifRadiusKm);
        AddFloat(Settings.MassifReliefMeters);
        AddFloat(Settings.PassChance);
        AddFloat(Settings.PassHalfWidthKm);
        AddFloat(Settings.PassDepthMeters);
        AddFloat(Settings.BasinCellSizeKm);
        AddFloat(Settings.BasinRadiusKm);
        AddFloat(Settings.BasinDepthMeters);
        AddFloat(Settings.HillScaleKm);
        AddFloat(Settings.HillReliefMeters);
        AddFloat(Settings.SpurScaleKm);
        AddFloat(Settings.SpurReliefMeters);
        AddFloat(Settings.SwaleScaleMeters);
        AddFloat(Settings.SwaleDepthMeters);
        AddFloat(Settings.FineScaleMeters);
        AddFloat(Settings.FineReliefMeters);
        AddFloat(Settings.MorphologyWarpMeters);
        return Hash;
    }

    uint32 HashSettings(const FCubusTerrainDrainageSettings& Settings)
    {
        // Only hash inputs that affect the solved region itself. Stream/lake
        // classification thresholds are applied when querying the cached field,
        // so changing those should not force an expensive drainage rebuild.
        uint32 Hash = HashStructureSettings(Settings.Raster.Structure);
        Hash = HashCombine(Hash, GetTypeHash(Settings.Raster.DomainOffsetMeters));
        Hash = HashCombine(Hash, GetTypeHash(Settings.AnalysisCellSizeMeters));
        Hash = HashCombine(Hash, GetTypeHash(Settings.RegionCellCount));
        Hash = HashCombine(Hash, GetTypeHash(Settings.HaloCellCount));
        Hash = HashCombine(Hash, GetTypeHash(Settings.FillEpsilonMeters));
        Hash = HashCombine(Hash, GetTypeHash(Settings.RegionOriginMeters));
        return Hash;
    }

    struct FCacheKey
    {
        FIntPoint Region = FIntPoint::ZeroValue;
        uint32 SettingsHash = 0;

        bool operator==(const FCacheKey& Other) const
        {
            return Region == Other.Region && SettingsHash == Other.SettingsHash;
        }
    };

    uint32 GetTypeHash(const FCacheKey& Key)
    {
        uint32 Hash = ::GetTypeHash(Key.Region.X);
        Hash = HashCombine(Hash, ::GetTypeHash(Key.Region.Y));
        return HashCombine(Hash, ::GetTypeHash(Key.SettingsHash));
    }

    FCriticalSection CacheMutex;
    TMap<FCacheKey, TSharedPtr<const FRegion, ESPMode::ThreadSafe>> Cache;

    const FIntPoint Neighbours[8] =
    {
        FIntPoint(-1, -1), FIntPoint(0, -1), FIntPoint(1, -1),
        FIntPoint(-1, 0),                         FIntPoint(1, 0),
        FIntPoint(-1, 1),  FIntPoint(0, 1),  FIntPoint(1, 1)
    };

    TSharedPtr<const FRegion, ESPMode::ThreadSafe> BuildRegion(
        const FIntPoint& RegionCoordinate,
        const FCubusTerrainDrainageSettings& InSettings
    )
    {
        FCubusTerrainDrainageSettings Settings = InSettings;
        Settings.AnalysisCellSizeMeters = FMath::Max(2.0f, Settings.AnalysisCellSizeMeters);
        Settings.RegionCellCount = FMath::Clamp(Settings.RegionCellCount, 32, 512);
        Settings.HaloCellCount = FMath::Clamp(Settings.HaloCellCount, 8, 192);
        Settings.FillEpsilonMeters = FMath::Max(0.00001f, Settings.FillEpsilonMeters);

        TSharedPtr<FRegion, ESPMode::ThreadSafe> Region = MakeShared<FRegion, ESPMode::ThreadSafe>();
        Region->Coordinate = RegionCoordinate;
        Region->InteriorCellCount = Settings.RegionCellCount;
        Region->HaloCellCount = Settings.HaloCellCount;
        Region->GridSize = Settings.RegionCellCount + Settings.HaloCellCount * 2;
        Region->CellSizeMeters = Settings.AnalysisCellSizeMeters;

        const double RegionWorldSize = static_cast<double>(Settings.RegionCellCount) * Settings.AnalysisCellSizeMeters;
        Region->WorldOriginMeters = Settings.RegionOriginMeters + FVector2D(
            static_cast<double>(RegionCoordinate.X) * RegionWorldSize - static_cast<double>(Settings.HaloCellCount) * Settings.AnalysisCellSizeMeters,
            static_cast<double>(RegionCoordinate.Y) * RegionWorldSize - static_cast<double>(Settings.HaloCellCount) * Settings.AnalysisCellSizeMeters
        );

        const int32 SampleCount = Region->GridSize * Region->GridSize;
        Region->RawHeight.SetNumUninitialized(SampleCount);
        Region->FilledHeight.SetNumUninitialized(SampleCount);
        Region->AccumulationCells.Init(1.0f, SampleCount);
        Region->Receiver.Init(INDEX_NONE, SampleCount);
        Region->StrahlerOrder.Init(1, SampleCount);

        for (int32 Y = 0; Y < Region->GridSize; ++Y)
        {
            for (int32 X = 0; X < Region->GridSize; ++X)
            {
                const int32 Index = Region->Index(X, Y);
                const FVector2D Centre = Region->CellCentre(X, Y);
                const float Height = FCubusTerrainRasterBuilder::SampleStructuralHeightMeters(
                    Centre.X,
                    Centre.Y,
                    Settings.Raster
                );
                Region->RawHeight[Index] = Height;
                Region->FilledHeight[Index] = Height;
            }
        }

        TArray<uint8> Visited;
        Visited.Init(0, SampleCount);
        TArray<FHeapNode> Heap;
        Heap.Reserve(SampleCount / 4);

        // Priority flood already removes cells from the min-heap in nondecreasing
        // filled-height order. Retaining that order lets accumulation later walk
        // it backwards instead of sorting every cell by height a second time.
        TArray<int32> LowToHigh;
        LowToHigh.Reserve(SampleCount);

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

        while (!Heap.IsEmpty())
        {
            const FHeapNode Current = HeapPop(Heap);
            LowToHigh.Add(Current.Index);

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
                Region->FilledHeight[Index] = FMath::Max(
                    Region->RawHeight[Index],
                    Current.Height + Settings.FillEpsilonMeters
                );
                HeapPush(Heap, {Region->FilledHeight[Index], Index});
            }
        }

        for (int32 Y = 1; Y < Region->GridSize - 1; ++Y)
        {
            for (int32 X = 1; X < Region->GridSize - 1; ++X)
            {
                const int32 Index = Region->Index(X, Y);
                const float CurrentHeight = Region->FilledHeight[Index];
                float BestDropPerMeter = 0.0f;
                int32 BestIndex = INDEX_NONE;

                for (const FIntPoint& Offset : Neighbours)
                {
                    const int32 NeighbourX = X + Offset.X;
                    const int32 NeighbourY = Y + Offset.Y;
                    const int32 NeighbourIndex = Region->Index(NeighbourX, NeighbourY);
                    const float Drop = CurrentHeight - Region->FilledHeight[NeighbourIndex];
                    if (Drop <= 0.0f)
                    {
                        continue;
                    }
                    const float Distance = Offset.X != 0 && Offset.Y != 0
                        ? Settings.AnalysisCellSizeMeters * UE_SQRT_2
                        : Settings.AnalysisCellSizeMeters;
                    const float DropPerMeter = Drop / Distance;
                    if (DropPerMeter > BestDropPerMeter)
                    {
                        BestDropPerMeter = DropPerMeter;
                        BestIndex = NeighbourIndex;
                    }
                }
                Region->Receiver[Index] = BestIndex;
            }
        }

        // Receivers are always strictly lower on the epsilon-resolved filled
        // surface, so reverse priority-flood order is already a valid upstream-
        // to-downstream accumulation order. No O(N log N) height sort is needed.
        for (int32 OrderIndex = LowToHigh.Num() - 1; OrderIndex >= 0; --OrderIndex)
        {
            const int32 Index = LowToHigh[OrderIndex];
            const int32 Receiver = Region->Receiver[Index];
            if (Receiver != INDEX_NONE)
            {
                Region->AccumulationCells[Receiver] += Region->AccumulationCells[Index];
            }
        }

        TArray<int32> UpstreamCount;
        UpstreamCount.Init(0, SampleCount);
        TArray<int32> MaximumIncomingOrder;
        MaximumIncomingOrder.Init(0, SampleCount);
        TArray<int32> MaximumIncomingOrderCount;
        MaximumIncomingOrderCount.Init(0, SampleCount);

        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            const int32 Receiver = Region->Receiver[Index];
            if (Receiver != INDEX_NONE)
            {
                ++UpstreamCount[Receiver];
            }
        }

        TArray<int32> Queue;
        Queue.Reserve(SampleCount);
        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            if (UpstreamCount[Index] == 0)
            {
                Queue.Add(Index);
            }
        }

        int32 QueueIndex = 0;
        while (QueueIndex < Queue.Num())
        {
            const int32 Index = Queue[QueueIndex++];
            const int32 Receiver = Region->Receiver[Index];
            if (Receiver == INDEX_NONE)
            {
                continue;
            }

            const int32 IncomingOrder = Region->StrahlerOrder[Index];
            if (IncomingOrder > MaximumIncomingOrder[Receiver])
            {
                MaximumIncomingOrder[Receiver] = IncomingOrder;
                MaximumIncomingOrderCount[Receiver] = 1;
            }
            else if (IncomingOrder == MaximumIncomingOrder[Receiver])
            {
                ++MaximumIncomingOrderCount[Receiver];
            }

            --UpstreamCount[Receiver];
            if (UpstreamCount[Receiver] == 0)
            {
                const int32 MaxIncoming = MaximumIncomingOrder[Receiver];
                Region->StrahlerOrder[Receiver] = MaxIncoming > 0
                    ? MaxIncoming + (MaximumIncomingOrderCount[Receiver] >= 2 ? 1 : 0)
                    : 1;
                Queue.Add(Receiver);
            }
        }

        return Region;
    }

    TSharedPtr<const FRegion, ESPMode::ThreadSafe> GetRegion(
        const FIntPoint& Coordinate,
        const FCubusTerrainDrainageSettings& Settings
    )
    {
        const FCacheKey Key{Coordinate, HashSettings(Settings)};
        {
            FScopeLock Lock(&CacheMutex);
            if (const TSharedPtr<const FRegion, ESPMode::ThreadSafe>* Existing = Cache.Find(Key))
            {
                return *Existing;
            }
        }

        const TSharedPtr<const FRegion, ESPMode::ThreadSafe> Built = BuildRegion(Coordinate, Settings);
        FScopeLock Lock(&CacheMutex);
        if (const TSharedPtr<const FRegion, ESPMode::ThreadSafe>* Existing = Cache.Find(Key))
        {
            return *Existing;
        }
        Cache.Add(Key, Built);
        return Built;
    }

    FIntPoint WorldToRegion(
        const double WorldXmeters,
        const double WorldYmeters,
        const FCubusTerrainDrainageSettings& Settings
    )
    {
        const double CellSize = FMath::Max(2.0, static_cast<double>(Settings.AnalysisCellSizeMeters));
        const double RegionSize = CellSize * static_cast<double>(FMath::Clamp(Settings.RegionCellCount, 32, 512));
        return FIntPoint(
            FMath::FloorToInt((WorldXmeters - Settings.RegionOriginMeters.X) / RegionSize),
            FMath::FloorToInt((WorldYmeters - Settings.RegionOriginMeters.Y) / RegionSize)
        );
    }

    float CellAreaSquareKm(const FRegion& Region)
    {
        return Region.CellSizeMeters * Region.CellSizeMeters / 1000000.0f;
    }

    float DepressionDepthMeters(const FRegion& Region, const int32 Index)
    {
        return FMath::Max(0.0f, Region.FilledHeight[Index] - Region.RawHeight[Index]);
    }

    int32 ClosestCellIndex(const FRegion& Region, const double WorldXmeters, const double WorldYmeters)
    {
        const int32 X = FMath::Clamp(
            FMath::FloorToInt((WorldXmeters - Region.WorldOriginMeters.X) / Region.CellSizeMeters),
            0,
            Region.GridSize - 1
        );
        const int32 Y = FMath::Clamp(
            FMath::FloorToInt((WorldYmeters - Region.WorldOriginMeters.Y) / Region.CellSizeMeters),
            0,
            Region.GridSize - 1
        );
        return Region.Index(X, Y);
    }
}

FCubusTerrainDrainageSample FCubusTerrainDrainage::Sample(
    const double WorldXmeters,
    const double WorldYmeters,
    const FCubusTerrainDrainageSettings& Settings
)
{
    using namespace CubusTerrainDrainage;

    const FIntPoint RegionCoordinate = WorldToRegion(WorldXmeters, WorldYmeters, Settings);
    const TSharedPtr<const FRegion, ESPMode::ThreadSafe> Region = GetRegion(RegionCoordinate, Settings);
    const int32 Index = ClosestCellIndex(*Region, WorldXmeters, WorldYmeters);

    FCubusTerrainDrainageSample Result;
    Result.RawHeightMeters = Region->RawHeight[Index];
    Result.FilledHeightMeters = Region->FilledHeight[Index];
    Result.DepressionDepthMeters = DepressionDepthMeters(*Region, Index);
    Result.ContributingAreaSquareKm = Region->AccumulationCells[Index] * CellAreaSquareKm(*Region);
    Result.StrahlerOrder = Region->StrahlerOrder[Index];
    Result.bStream = Result.ContributingAreaSquareKm >= FMath::Max(0.0f, Settings.StreamSourceAreaSquareKm);
    Result.bLakeCandidate = Result.DepressionDepthMeters > FMath::Max(0.0f, Settings.LakeMinimumDepthMeters);

    const int32 Receiver = Region->Receiver[Index];
    if (Receiver != INDEX_NONE)
    {
        const int32 X = Index % Region->GridSize;
        const int32 Y = Index / Region->GridSize;
        const int32 ReceiverX = Receiver % Region->GridSize;
        const int32 ReceiverY = Receiver / Region->GridSize;
        Result.FlowDirection = (Region->CellCentre(ReceiverX, ReceiverY) - Region->CellCentre(X, Y)).GetSafeNormal();
    }

    return Result;
}

void FCubusTerrainDrainage::CollectSegments(
    const FBox2D& WorldBoundsMeters,
    const FCubusTerrainDrainageSettings& Settings,
    TArray<FCubusTerrainDrainageSegment>& OutSegments
)
{
    using namespace CubusTerrainDrainage;

    if (!WorldBoundsMeters.bIsValid)
    {
        return;
    }

    const FIntPoint MinRegion = WorldToRegion(WorldBoundsMeters.Min.X, WorldBoundsMeters.Min.Y, Settings);
    const FIntPoint MaxRegion = WorldToRegion(WorldBoundsMeters.Max.X, WorldBoundsMeters.Max.Y, Settings);
    const float LakeThresholdMeters = FMath::Max(0.0f, Settings.LakeMinimumDepthMeters);

    for (int32 RegionY = MinRegion.Y; RegionY <= MaxRegion.Y; ++RegionY)
    {
        for (int32 RegionX = MinRegion.X; RegionX <= MaxRegion.X; ++RegionX)
        {
            const TSharedPtr<const FRegion, ESPMode::ThreadSafe> Region = GetRegion(FIntPoint(RegionX, RegionY), Settings);
            const float CellAreaKm2 = CellAreaSquareKm(*Region);
            const int32 InteriorMin = Region->HaloCellCount;
            const int32 InteriorMax = Region->HaloCellCount + Region->InteriorCellCount;

            for (int32 Y = InteriorMin; Y < InteriorMax; ++Y)
            {
                for (int32 X = InteriorMin; X < InteriorMax; ++X)
                {
                    const int32 Index = Region->Index(X, Y);
                    const int32 Receiver = Region->Receiver[Index];
                    if (Receiver == INDEX_NONE)
                    {
                        continue;
                    }

                    const float AreaKm2 = Region->AccumulationCells[Index] * CellAreaKm2;
                    if (AreaKm2 < FMath::Max(0.0f, Settings.StreamSourceAreaSquareKm))
                    {
                        continue;
                    }

                    const FVector2D Start = Region->CellCentre(X, Y);
                    if (!WorldBoundsMeters.IsInside(Start))
                    {
                        continue;
                    }

                    const int32 ReceiverX = Receiver % Region->GridSize;
                    const int32 ReceiverY = Receiver / Region->GridSize;

                    FCubusTerrainDrainageSegment Segment;
                    Segment.StartMeters = Start;
                    Segment.EndMeters = Region->CellCentre(ReceiverX, ReceiverY);
                    Segment.StartElevationMeters = Region->FilledHeight[Index];
                    Segment.EndElevationMeters = Region->FilledHeight[Receiver];
                    Segment.ContributingAreaSquareKm = AreaKm2;
                    Segment.StrahlerOrder = Region->StrahlerOrder[Index];
                    Segment.bLakeTraversal =
                        DepressionDepthMeters(*Region, Index) > LakeThresholdMeters ||
                        DepressionDepthMeters(*Region, Receiver) > LakeThresholdMeters;
                    OutSegments.Add(Segment);
                }
            }
        }
    }
}
