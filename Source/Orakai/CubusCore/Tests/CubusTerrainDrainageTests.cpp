#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusTerrainDrainage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDrainageTest,
    "Orakai.Cubus.Terrain.Drainage.Network",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDrainageTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainDrainageSettings Settings;
    Settings.AnalysisCellSizeMeters = 16.0f;
    Settings.RegionCellCount = 64;
    Settings.HaloCellCount = 24;
    Settings.StreamSourceAreaSquareKm = 0.003f;
    Settings.MajorRiverAreaSquareKm = 0.10f;
    Settings.Raster.Structure.Seed = 9137;

    TArray<FCubusTerrainDrainageSegment> Segments;
    FCubusTerrainDrainage::CollectSegments(
        FBox2D(FVector2D(-512.0, -512.0), FVector2D(512.0, 512.0)),
        Settings,
        Segments
    );

    TestTrue(TEXT("Drainage produces stream segments"), !Segments.IsEmpty());

    float MaximumArea = 0.0f;
    int32 HigherOrderSegments = 0;
    for (const FCubusTerrainDrainageSegment& Segment : Segments)
    {
        TestTrue(
            TEXT("Filled drainage segment never routes uphill"),
            Segment.EndElevationMeters <= Segment.StartElevationMeters + Settings.FillEpsilonMeters * 2.0f
        );
        TestTrue(
            TEXT("Drainage segment has positive contributing area"),
            Segment.ContributingAreaSquareKm > 0.0f
        );
        TestTrue(
            TEXT("Drainage receiver differs from source"),
            !Segment.StartMeters.Equals(Segment.EndMeters, 0.001)
        );
        MaximumArea = FMath::Max(MaximumArea, Segment.ContributingAreaSquareKm);
        HigherOrderSegments += Segment.StrahlerOrder > 1 ? 1 : 0;
    }

    TestTrue(
        TEXT("Network contains downstream accumulation beyond source threshold"),
        MaximumArea > Settings.StreamSourceAreaSquareKm * 2.0f
    );
    TestTrue(
        TEXT("Confluences produce higher Strahler order"),
        HigherOrderSegments > 0
    );

    const FCubusTerrainDrainageSample SampleA = FCubusTerrainDrainage::Sample(137.0, -219.0, Settings);
    const FCubusTerrainDrainageSample SampleARepeat = FCubusTerrainDrainage::Sample(137.0, -219.0, Settings);
    TestTrue(
        TEXT("Drainage query is deterministic"),
        FMath::IsNearlyEqual(SampleA.FilledHeightMeters, SampleARepeat.FilledHeightMeters, 0.0001f) &&
        FMath::IsNearlyEqual(SampleA.ContributingAreaSquareKm, SampleARepeat.ContributingAreaSquareKm, 0.000001f) &&
        SampleA.StrahlerOrder == SampleARepeat.StrahlerOrder &&
        SampleA.FlowDirection.Equals(SampleARepeat.FlowDirection, 0.0001)
    );

    FCubusTerrainDrainageSettings OtherSeed = Settings;
    OtherSeed.Raster.Structure.Seed += 1;
    const FCubusTerrainDrainageSample SampleB = FCubusTerrainDrainage::Sample(137.0, -219.0, OtherSeed);
    TestTrue(
        TEXT("Structural seed changes drainage solution"),
        !FMath::IsNearlyEqual(SampleA.RawHeightMeters, SampleB.RawHeightMeters, 0.001f) ||
        !FMath::IsNearlyEqual(SampleA.ContributingAreaSquareKm, SampleB.ContributingAreaSquareKm, 0.000001f) ||
        !SampleA.FlowDirection.Equals(SampleB.FlowDirection, 0.0001)
    );

    return true;
}

#endif
