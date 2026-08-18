#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusTerrainCarving.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainCarvingStageTest,
    "Orakai.Cubus.Terrain.Carving.DrainageDriven",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainCarvingStageTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainCarvingSettings Settings;
    Settings.Drainage.Raster.SampleSpacingMeters = 1.0f;
    Settings.Drainage.Raster.TileSizeMeters = 64.0f;
    Settings.Drainage.Raster.Structure.Seed = 1947;
    Settings.Drainage.AnalysisCellSizeMeters = 8.0f;
    Settings.Drainage.RegionCellCount = 96;
    Settings.Drainage.HaloCellCount = 48;
    Settings.Drainage.StreamSourceAreaSquareKm = 0.01f;
    Settings.Drainage.MajorRiverAreaSquareKm = 1.0f;
    Settings.HeadwaterValleyHalfWidthMeters = 10.0f;
    Settings.MajorValleyHalfWidthMeters = 80.0f;

    TArray<FCubusTerrainDrainageSegment> Segments;
    FCubusTerrainDrainage::CollectSegments(
        FBox2D(FVector2D(-1024.0, -1024.0), FVector2D(1024.0, 1024.0)),
        Settings.Drainage,
        Segments
    );
    TestTrue(TEXT("Drainage exposes at least one segment for carving"), !Segments.IsEmpty());

    if (!Segments.IsEmpty())
    {
        const FCubusTerrainDrainageSegment& Segment = Segments[Segments.Num() / 2];
        const FVector2D Midpoint = (Segment.StartMeters + Segment.EndMeters) * 0.5;
        const FCubusTerrainCarvingSample Centre = FCubusTerrainCarving::Sample(Midpoint.X, Midpoint.Y, Settings);

        TestTrue(TEXT("Stream centre is incised"), Centre.TotalIncisionMeters > 0.01f);
        TestTrue(TEXT("Stream centre receives channel influence"), Centre.ChannelWeight > 0.1f);
        TestTrue(TEXT("Carving never raises the structural terrain"), Centre.CarvedHeightMeters <= Centre.OriginalHeightMeters + KINDA_SMALL_NUMBER);
        TestTrue(TEXT("Carving retains drainage hierarchy"), Centre.ContributingAreaSquareKm > 0.0f && Centre.StrahlerOrder >= 1);
    }

    const FCubusTerrainRasterTile LeftStructural = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(0, 0), Settings.Drainage.Raster);
    const FCubusTerrainRasterTile RightStructural = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(1, 0), Settings.Drainage.Raster);
    const FCubusTerrainRasterTile LeftCarved = FCubusTerrainCarving::CarveTile(LeftStructural, Settings);
    const FCubusTerrainRasterTile RightCarved = FCubusTerrainCarving::CarveTile(RightStructural, Settings);

    TestTrue(TEXT("Left carved DEM tile remains valid"), LeftCarved.IsValid());
    TestTrue(TEXT("Right carved DEM tile remains valid"), RightCarved.IsValid());

    const int32 Edge = LeftCarved.GetInteriorCellCount();
    for (int32 Y = 0; Y <= Edge; ++Y)
    {
        TestTrue(
            *FString::Printf(TEXT("Carved DEM tile boundary sample %d remains seamless"), Y),
            FMath::IsNearlyEqual(
                LeftCarved.GetHeightSampleMeters(Edge, Y),
                RightCarved.GetHeightSampleMeters(0, Y),
                0.001f
            )
        );
    }

    return true;
}

#endif
