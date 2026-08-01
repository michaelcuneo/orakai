#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensityFractionalPlaneTest,
    "Orakai.Cubus.Density.NativeTerrainFractionalPlane",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDensityFractionalPlaneTest::RunTest(
    const FString& Parameters
)
{
    FCubusTerrainDensitySettings Settings;
    Settings.bUseHeightTerrain = false;
    Settings.FlatSurfaceWorldZ = 8.25f;
    Settings.SurfaceMaterialId = 11;
    Settings.SubsurfaceMaterialId = 12;
    Settings.SurfaceMaterialDepth = 2.0f;

    const FCubusTerrainDensityField DensityField(Settings);

    const FCubusDensitySample NearSurface =
        DensityField.Sample(FIntVector(0, 0, 9));

    const FCubusDensitySample AboveSurface =
        DensityField.Sample(FIntVector(0, 0, 10));

    const FCubusDensitySample DeepSample =
        DensityField.Sample(FIntVector(0, 0, 7));

    TestTrue(
        TEXT("A sample below the fractional surface is solid"),
        NearSurface.IsSolid()
    );

    TestTrue(
        TEXT("A sample above the fractional surface is empty"),
        !AboveSurface.IsSolid()
    );

    TestEqual(
        TEXT("Near-surface density preserves the fractional height"),
        NearSurface.Density,
        0.25f
    );

    TestEqual(
        TEXT("Above-surface density preserves the fractional height"),
        AboveSurface.Density,
        -0.75f
    );

    TestEqual(
        TEXT("The near-surface sample uses the surface material"),
        NearSurface.MaterialId,
        11
    );

    TestEqual(
        TEXT("A deeper solid sample uses the subsurface material"),
        DeepSample.MaterialId,
        12
    );

    FCubusDensitySamplingBuffer DensityBuffer;
    DensityBuffer.Build(
        FIntVector::ZeroValue,
        DensityField
    );

    TMap<int32, FCubusMeshData> MaterialMeshes;
    int32 TriangleCount = 0;

    FCubusDensityMesher::BuildChunk(
        DensityBuffer,
        1.0f,
        0.0f,
        MaterialMeshes,
        TriangleCount
    );

    TestEqual(
        TEXT("A full 32 by 32 fractional plane emits two triangles per cell"),
        TriangleCount,
        32 * 32 * 2
    );

    const double ExpectedLocalZ = -6.75;
    int32 VertexCount = 0;

    for (const TPair<int32, FCubusMeshData>& Pair : MaterialMeshes)
    {
        for (const FVector& Vertex : Pair.Value.Vertices)
        {
            ++VertexCount;

            TestTrue(
                TEXT("Marching Cubes places the surface at the fractional Z"),
                FMath::IsNearlyEqual(
                    Vertex.Z,
                    ExpectedLocalZ,
                    0.0001
                )
            );
        }
    }

    TestTrue(
        TEXT("The fractional plane emitted vertices"),
        VertexCount > 0
    );

    return true;
}

#endif
