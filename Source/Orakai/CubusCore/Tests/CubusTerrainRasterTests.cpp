#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusTerrainRaster.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainRasterContractTest,
    "Orakai.Cubus.Terrain.Raster.MetricContract",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainRasterContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainRasterSettings Settings;
    Settings.SampleSpacingMeters = 1.0f;
    Settings.TileSizeMeters = 32.0f;
    Settings.HaloSamples = 2;
    Settings.DomainOffsetMeters = FVector2D(137.0, -89.0);
    Settings.Structure.Seed = 73491;

    const FCubusTerrainRasterTile Left = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(0, 0), Settings);
    const FCubusTerrainRasterTile Right = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(1, 0), Settings);

    TestTrue(TEXT("Left high-resolution terrain tile is valid"), Left.IsValid());
    TestTrue(TEXT("Right high-resolution terrain tile is valid"), Right.IsValid());
    TestEqual(TEXT("Default metric raster uses one metre samples"), Left.GetSampleSpacingMeters(), 1.0f);
    TestEqual(TEXT("32 metre test tile contains 32 interior cells"), Left.GetInteriorCellCount(), 32);

    const int32 EdgeX = Left.GetInteriorCellCount();
    for (int32 Y = 0; Y <= Left.GetInteriorCellCount(); ++Y)
    {
        const float LeftHeight = Left.GetHeightSampleMeters(EdgeX, Y);
        const float RightHeight = Right.GetHeightSampleMeters(0, Y);
        TestTrue(
            *FString::Printf(TEXT("Adjacent raster border sample %d is identical"), Y),
            FMath::IsNearlyEqual(LeftHeight, RightHeight, 0.0001f)
        );
    }

    const double BoundaryX = Left.GetWorldMaximumMeters().X;
    const double ProbeY = 13.375;
    const float LeftBoundary = Left.SampleHeightMeters(BoundaryX, ProbeY);
    const float RightBoundary = Right.SampleHeightMeters(BoundaryX, ProbeY);
    TestTrue(
        TEXT("Bicubic reconstruction is continuous at tile boundary"),
        FMath::IsNearlyEqual(LeftBoundary, RightBoundary, 0.0001f)
    );

    TestEqual(
        TEXT("Negative world coordinates map to the negative tile"),
        FCubusTerrainRasterBuilder::WorldToTileCoordinate(-0.01, -0.01, Settings),
        FIntPoint(-1, -1)
    );
    TestEqual(
        TEXT("Positive boundary maps to the next tile"),
        FCubusTerrainRasterBuilder::WorldToTileCoordinate(32.0, 0.0, Settings),
        FIntPoint(1, 0)
    );

    const FCubusTerrainRasterTile Repeat = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(0, 0), Settings);
    TestTrue(
        TEXT("Structural raster generation is deterministic"),
        FMath::IsNearlyEqual(
            Left.SampleHeightMeters(11.25, 17.75),
            Repeat.SampleHeightMeters(11.25, 17.75),
            0.0001f
        )
    );

    FCubusTerrainRasterSettings Alternate = Settings;
    Alternate.Structure.Seed += 991;
    const FCubusTerrainRasterTile AlternateTile = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(0, 0), Alternate);
    TestFalse(
        TEXT("Changing the terrain seed changes structural elevation"),
        FMath::IsNearlyEqual(
            Left.SampleHeightMeters(11.25, 17.75),
            AlternateTile.SampleHeightMeters(11.25, 17.75),
            0.001f
        )
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainStructureCoherenceTest,
    "Orakai.Cubus.Terrain.Structure.CoherentRanges",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainStructureCoherenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainStructureSettings Settings;
    Settings.Seed = 98173;

    int32 RangeSamples = 0;
    int32 CoreSamples = 0;
    int32 BasinSamples = 0;
    float MaximumHeight = -MAX_flt;
    float MinimumHeight = MAX_flt;

    // A broad survey verifies that the explicit structures actually occupy
    // continuous landscape-scale areas rather than producing sparse point peaks.
    for (int32 Y = -16; Y <= 16; ++Y)
    {
        for (int32 X = -16; X <= 16; ++X)
        {
            const FCubusTerrainStructureSample Sample = FCubusTerrainStructure::Sample(
                static_cast<double>(X) * 2000.0,
                static_cast<double>(Y) * 2000.0,
                Settings
            );

            RangeSamples += Sample.RangeWeight > 0.35f ? 1 : 0;
            CoreSamples += Sample.RangeCoreWeight > 0.35f ? 1 : 0;
            BasinSamples += Sample.BasinWeight > 0.35f ? 1 : 0;
            MaximumHeight = FMath::Max(MaximumHeight, Sample.HeightMeters);
            MinimumHeight = FMath::Min(MinimumHeight, Sample.HeightMeters);
        }
    }

    TestTrue(TEXT("Survey contains broad mountain-range belts"), RangeSamples > 20);
    TestTrue(TEXT("Survey contains connected high range cores"), CoreSamples > 4);
    TestTrue(TEXT("Survey contains broad basin regions"), BasinSamples > 20);
    TestTrue(TEXT("Structural relief spans meaningful vertical range"), MaximumHeight - MinimumHeight > 500.0f);

    return true;
}

#endif
