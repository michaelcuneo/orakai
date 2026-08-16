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

    FCubusTerrainRasterSettings Alternate = Settings;
    Alternate.StructuralSource.VoxelSizeCm = 37.0f;
    Alternate.StructuralSource.bUsePhysicalWorldScale = false;
    const FCubusTerrainRasterTile AlternateTile = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(0, 0), Alternate);

    const float ReferenceHeight = Left.SampleHeightMeters(11.25, 17.75);
    const float AlternateHeight = AlternateTile.SampleHeightMeters(11.25, 17.75);
    TestTrue(
        TEXT("Pre-voxel terrain geometry is independent of later voxel size settings"),
        FMath::IsNearlyEqual(ReferenceHeight, AlternateHeight, 0.0001f)
    );

    return true;
}

#endif
