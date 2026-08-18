#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusTerrainCarving.h"
#include "CubusCore/Generation/CubusTerrainErosion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainErosionSeamTest,
    "Orakai.Cubus.Terrain.Erosion.SeamContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainErosionSeamTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainRasterSettings Raster;
    Raster.SampleSpacingMeters = 2.0f;
    Raster.TileSizeMeters = 64.0f;
    Raster.HaloSamples = 8;
    Raster.Structure.Seed = 918273;

    FCubusTerrainDrainageSettings Drainage;
    Drainage.Raster = Raster;
    Drainage.AnalysisCellSizeMeters = 8.0f;
    Drainage.RegionCellCount = 64;
    Drainage.HaloCellCount = 24;
    Drainage.RegionOriginMeters = FVector2D(-256.0, -256.0);
    Drainage.StreamSourceAreaSquareKm = 0.001f;

    FCubusTerrainCarvingSettings Carving;
    Carving.Drainage = Drainage;

    FCubusTerrainErosionSettings Erosion;
    Erosion.Iterations = 5;

    const FCubusTerrainRasterTile LeftStructural = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(0, 0), Raster);
    const FCubusTerrainRasterTile RightStructural = FCubusTerrainRasterBuilder::BuildTile(FIntPoint(1, 0), Raster);
    TestTrue(TEXT("Left structural tile is valid"), LeftStructural.IsValid());
    TestTrue(TEXT("Right structural tile is valid"), RightStructural.IsValid());

    const FCubusTerrainRasterTile LeftCarved = FCubusTerrainCarving::CarveTile(LeftStructural, Carving);
    const FCubusTerrainRasterTile RightCarved = FCubusTerrainCarving::CarveTile(RightStructural, Carving);
    const FCubusTerrainRasterTile LeftEroded = FCubusTerrainErosion::ErodeTile(LeftCarved, Erosion);
    const FCubusTerrainRasterTile RightEroded = FCubusTerrainErosion::ErodeTile(RightCarved, Erosion);

    TestTrue(TEXT("Left eroded tile is valid"), LeftEroded.IsValid());
    TestTrue(TEXT("Right eroded tile is valid"), RightEroded.IsValid());

    const int32 Cells = LeftEroded.GetInteriorCellCount();
    bool bSeamMatches = true;
    for (int32 Y = 0; Y <= Cells; ++Y)
    {
        const float LeftHeight = LeftEroded.GetHeightSampleMeters(Cells, Y);
        const float RightHeight = RightEroded.GetHeightSampleMeters(0, Y);
        if (!FMath::IsNearlyEqual(LeftHeight, RightHeight, 0.001f))
        {
            bSeamMatches = false;
            AddError(FString::Printf(
                TEXT("Erosion seam mismatch at Y=%d: left=%f right=%f"),
                Y,
                LeftHeight,
                RightHeight
            ));
            break;
        }
    }

    TestTrue(TEXT("Repeated erosion preserves the authored tile seam"), bSeamMatches);
    return true;
}
