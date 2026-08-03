#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusTerrainForm.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusNaturalTerrainFormTest,
    "Orakai.Cubus.Generation.NaturalTerrainForm",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusNaturalTerrainFormTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainFormSettings Settings;
    float MaximumAdjacentHeightDelta = 0.0f;
    bool bFoundPlain = false;
    bool bFoundMountain = false;
    bool bFoundStrongDrainage = false;

    for (int32 Y = -384; Y <= 384; Y += 12)
    {
        for (int32 X = -384; X <= 384; X += 12)
        {
            const FCubusTerrainFormSample Sample = FCubusTerrainForm::Sample(
                static_cast<float>(X),
                static_cast<float>(Y),
                Settings
            );
            const FCubusTerrainFormSample Repeated = FCubusTerrainForm::Sample(
                static_cast<float>(X),
                static_cast<float>(Y),
                Settings
            );
            const FCubusTerrainFormSample Adjacent = FCubusTerrainForm::Sample(
                static_cast<float>(X + 1),
                static_cast<float>(Y),
                Settings
            );

            TestTrue(TEXT("Terrain height is finite"), FMath::IsFinite(Sample.Height));
            TestEqual(TEXT("Terrain sampling is deterministic"), Sample.Height, Repeated.Height);
            TestTrue(
                TEXT("Terrain region weights are normalized"),
                FMath::IsNearlyEqual(
                    Sample.PlainsWeight + Sample.RollingWeight + Sample.MountainWeight,
                    1.0f,
                    0.001f
                )
            );

            MaximumAdjacentHeightDelta = FMath::Max(
                MaximumAdjacentHeightDelta,
                FMath::Abs(Sample.Height - Adjacent.Height)
            );
            bFoundPlain |= Sample.PlainsWeight > 0.65f;
            bFoundMountain |= Sample.MountainWeight > 0.65f;
            bFoundStrongDrainage |= Sample.Drainage > 0.75f;
        }
    }

    // Mountain scale is a gameplay requirement, not just a visual tuning
    // preference. Sample a 12.3 km square and measure connected range cores
    // on a 128 m grid. A component must continue for kilometres; otherwise a
    // regression to thresholded local mounds can still satisfy the simpler
    // "found a mountain" assertion above.
    constexpr int32 RangeHalfExtent = 6144;
    constexpr int32 RangeSampleStep = 128;
    constexpr int32 RangeGridSize =
        (RangeHalfExtent * 2) / RangeSampleStep + 1;
    TArray<uint8> MountainMask;
    MountainMask.SetNumZeroed(RangeGridSize * RangeGridSize);

    const auto RangeIndex = [](const int32 X, const int32 Y)
    {
        return Y * RangeGridSize + X;
    };

    for (int32 GridY = 0; GridY < RangeGridSize; ++GridY)
    {
        for (int32 GridX = 0; GridX < RangeGridSize; ++GridX)
        {
            const FCubusTerrainFormSample Sample = FCubusTerrainForm::Sample(
                static_cast<float>(GridX * RangeSampleStep - RangeHalfExtent),
                static_cast<float>(GridY * RangeSampleStep - RangeHalfExtent),
                Settings
            );

            MountainMask[RangeIndex(GridX, GridY)] =
                Sample.MountainCore > 0.35f ? 1u : 0u;
            bFoundPlain |= Sample.PlainsWeight > 0.65f;
            bFoundMountain |= Sample.MountainWeight > 0.65f;
            bFoundStrongDrainage |= Sample.Drainage > 0.75f;
        }
    }

    TArray<uint8> Visited;
    Visited.SetNumZeroed(MountainMask.Num());
    int32 LongestConnectedRange = 0;
    constexpr int32 NeighbourOffsets[8][2] =
    {
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    };

    for (int32 StartY = 0; StartY < RangeGridSize; ++StartY)
    {
        for (int32 StartX = 0; StartX < RangeGridSize; ++StartX)
        {
            const int32 StartIndex = RangeIndex(StartX, StartY);
            if (MountainMask[StartIndex] == 0u || Visited[StartIndex] != 0u)
            {
                continue;
            }

            int32 MinimumX = StartX;
            int32 MaximumX = StartX;
            int32 MinimumY = StartY;
            int32 MaximumY = StartY;
            TArray<int32> Pending;
            Pending.Add(StartIndex);
            Visited[StartIndex] = 1u;

            for (int32 PendingIndex = 0; PendingIndex < Pending.Num(); ++PendingIndex)
            {
                const int32 Index = Pending[PendingIndex];
                const int32 X = Index % RangeGridSize;
                const int32 Y = Index / RangeGridSize;
                MinimumX = FMath::Min(MinimumX, X);
                MaximumX = FMath::Max(MaximumX, X);
                MinimumY = FMath::Min(MinimumY, Y);
                MaximumY = FMath::Max(MaximumY, Y);

                for (const int32* Offset : NeighbourOffsets)
                {
                    const int32 NextX = X + Offset[0];
                    const int32 NextY = Y + Offset[1];
                    if (
                        NextX < 0 || NextX >= RangeGridSize ||
                        NextY < 0 || NextY >= RangeGridSize
                    )
                    {
                        continue;
                    }

                    const int32 NextIndex = RangeIndex(NextX, NextY);
                    if (
                        MountainMask[NextIndex] != 0u &&
                        Visited[NextIndex] == 0u
                    )
                    {
                        Visited[NextIndex] = 1u;
                        Pending.Add(NextIndex);
                    }
                }
            }

            const int32 ComponentSpan = FMath::Max(
                MaximumX - MinimumX,
                MaximumY - MinimumY
            ) * RangeSampleStep;
            LongestConnectedRange = FMath::Max(
                LongestConnectedRange,
                ComponentSpan
            );
        }
    }

    TestTrue(TEXT("The sampled world contains coherent plains"), bFoundPlain);
    TestTrue(TEXT("The sampled world contains mountain regions"), bFoundMountain);
    TestTrue(TEXT("The sampled world contains drainage corridors"), bFoundStrongDrainage);
    TestTrue(
        TEXT("A connected mountain range spans at least 2.4 kilometres"),
        LongestConnectedRange >= 2400
    );
    TestTrue(
        TEXT("Neighbouring terrain columns remain continuous"),
        MaximumAdjacentHeightDelta < 8.0f
    );
    return true;
}

#endif
