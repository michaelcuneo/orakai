#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Meshing/CubusDensityLod.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityLodScaleTest,
    "Orakai.Cubus.Density.LOD.PhysicalScale",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusDensityLodScaleTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    TestEqual(
        TEXT("100 cm density uses one sample per canonical voxel"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 100.0f),
        1
    );
    TestEqual(
        TEXT("50 cm density uses two samples per canonical voxel"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 50.0f),
        2
    );
    TestEqual(
        TEXT("25 cm density uses four samples per canonical voxel"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 25.0f),
        4
    );
    TestEqual(
        TEXT("Runtime refinement is capped at four subdivisions"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 10.0f),
        4
    );

    TestEqual(
        TEXT("80 cm canonical terrain resolves 20 cm near spacing"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(80.0f, 20.0f),
        4
    );
    TestEqual(
        TEXT("80 cm canonical terrain resolves 40 cm middle spacing"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(80.0f, 40.0f),
        2
    );
    TestEqual(
        TEXT("80 cm canonical terrain keeps 80 cm far spacing"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(80.0f, 80.0f),
        1
    );

    TestEqual(
        TEXT("Fine density changes mesh spacing, not canonical voxel identity"),
        FCubusDensityLod::GetSampleSpacing(80.0f, 4),
        20.0f
    );

    TestEqual(
        TEXT("LOD distance is a three-dimensional chunk ring"),
        FCubusDensityLod::ChunkDistance(
            FIntVector(5, -2, 8),
            FIntVector(2, 2, 6)
        ),
        4
    );

    return true;
}

#endif
