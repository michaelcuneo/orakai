#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Meshing/CubusDensityMesher.h"
#include "CubusCore/Meshing/CubusMeshData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensityFractionalPlaneTest,
    "Orakai.Cubus.Density.NativeTerrain.FractionalPlane",
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensitySeedDomainTest,
    "Orakai.Cubus.Density.NativeTerrain.SeededDomain",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDensitySeedDomainTest::RunTest(
    const FString& Parameters
)
{
    FCubusTerrainDensitySettings UnshiftedSettings;
    UnshiftedSettings.bGenerateRivers = false;
    UnshiftedSettings.bGenerateCaves = false;

    FCubusTerrainDensitySettings ShiftedSettings =
        UnshiftedSettings;

    ShiftedSettings.TerrainOffsetX = 8192;
    ShiftedSettings.TerrainOffsetY = -4096;

    const FCubusTerrainDensityField UnshiftedField(
        UnshiftedSettings
    );

    const FCubusTerrainDensityField ShiftedFieldA(
        ShiftedSettings
    );

    const FCubusTerrainDensityField ShiftedFieldB(
        ShiftedSettings
    );

    const float UnshiftedHeight =
        UnshiftedField.SampleSurfaceVoxelHeight(
            12.5f,
            -3.25f
        );

    const float ShiftedHeightA =
        ShiftedFieldA.SampleSurfaceVoxelHeight(
            12.5f,
            -3.25f
        );

    const float ShiftedHeightB =
        ShiftedFieldB.SampleSurfaceVoxelHeight(
            12.5f,
            -3.25f
        );

    TestTrue(
        TEXT("The terrain seed domain changes the generated surface"),
        !FMath::IsNearlyEqual(
            UnshiftedHeight,
            ShiftedHeightA,
            0.001f
        )
    );

    TestTrue(
        TEXT("The same terrain seed domain is deterministic"),
        FMath::IsNearlyEqual(
            ShiftedHeightA,
            ShiftedHeightB,
            KINDA_SMALL_NUMBER
        )
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensityRiverTest,
    "Orakai.Cubus.Density.NativeTerrain.RiverLowering",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDensityRiverTest::RunTest(
    const FString& Parameters
)
{
    FCubusTerrainDensitySettings Settings;
    Settings.BaseHeight = 20.0f;
    Settings.ContinentAmplitude = 0.0f;
    Settings.HillAmplitude = 0.0f;
    Settings.DetailAmplitude = 0.0f;
    Settings.RidgeAmplitude = 0.0f;
    Settings.ValleyDepth = 0.0f;

    Settings.bGenerateRivers = true;
    Settings.RiverChannelWidth = 1.0f;
    Settings.RiverValleyWidth = 1.0f;
    Settings.RiverValleyDepth = 3.0f;
    Settings.RiverChannelDepth = 2.0f;
    Settings.RiverWarpAmplitude = 0.0f;
    Settings.RiverOffsetX = 0;
    Settings.RiverOffsetY = 0;

    const FCubusTerrainDensityField DensityField(Settings);

    const float SurfaceHeight =
        DensityField.SampleSurfaceVoxelHeight(
            0.0f,
            0.0f
        );

    TestTrue(
        TEXT("The native field lowers terrain continuously inside a river channel"),
        FMath::IsNearlyEqual(
            SurfaceHeight,
            15.0f,
            0.0001f
        )
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensityCaveTest,
    "Orakai.Cubus.Density.NativeTerrain.Caves",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDensityCaveTest::RunTest(
    const FString& Parameters
)
{
    FCubusTerrainDensitySettings Settings;
    Settings.bUseHeightTerrain = false;
    Settings.FlatSurfaceWorldZ = 32.0f;
    Settings.bGenerateCaves = true;
    Settings.CaveMinimumWorldZ = -32;
    Settings.CaveMaximumWorldZ = 24;
    Settings.CaveSurfaceClearance = 2;
    Settings.CaveThreshold = 1.0f;
    Settings.CavePrimaryFrequency = 0.035f;
    Settings.CaveSecondaryFrequency = 0.07f;
    Settings.CaveSurfaceSharpness = 8.0f;

    const FCubusTerrainDensityField DensityField(Settings);

    bool bFoundCarvedSample = false;

    for (int32 Z = -8; Z <= 8 && !bFoundCarvedSample; ++Z)
    {
        for (int32 Y = 0; Y < 16 && !bFoundCarvedSample; ++Y)
        {
            for (int32 X = 0; X < 16; ++X)
            {
                if (
                    !DensityField.Sample(
                        FIntVector(X, Y, Z)
                    ).IsSolid()
                )
                {
                    bFoundCarvedSample = true;
                    break;
                }
            }
        }
    }

    TestTrue(
        TEXT("The native density field carves empty samples below the terrain surface"),
        bFoundCarvedSample
    );

    TestTrue(
        TEXT("Cave clearance preserves terrain near the surface"),
        DensityField.Sample(
            FIntVector(0, 0, 31)
        ).IsSolid()
    );

    return true;
}

#endif
