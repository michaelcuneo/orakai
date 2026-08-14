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
    (void)Parameters;

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
    (void)Parameters;

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
    (void)Parameters;

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
    (void)Parameters;

    FCubusTerrainDensitySettings Settings;
    Settings.bUseHeightTerrain = false;
    Settings.FlatSurfaceWorldZ = 32.0f;
    Settings.bGenerateCaves = true;
    Settings.CaveMinimumWorldZ = -32;
    Settings.CaveMaximumWorldZ = 24;
    Settings.CaveSurfaceClearance = 2;
    Settings.CaveThreshold = 1.0f;
    Settings.CaveNetworkCellSize = 12.0f;
    Settings.CaveTunnelRadius = 3.5f;
    Settings.CaveChamberChance = 1.0f;
    Settings.CaveChamberRadius = 5.5f;
    Settings.CaveWallWarpStrength = 0.25f;
    Settings.CaveSurfaceSharpness = 8.0f;

    const FCubusTerrainDensityField DensityField(Settings);

    bool bFoundCarvedSample = false;
    int32 CarvedSampleCount = 0;

    for (int32 Z = -20; Z <= 20; ++Z)
    {
        for (int32 Y = -16; Y <= 16; ++Y)
        {
            for (int32 X = -16; X <= 16; ++X)
            {
                if (!DensityField.Sample(FIntVector(X, Y, Z)).IsSolid())
                {
                    bFoundCarvedSample = true;
                    ++CarvedSampleCount;
                }
            }
        }
    }

    TestTrue(
        TEXT("The native density field carves deterministic tunnel and chamber samples below the terrain surface"),
        bFoundCarvedSample
    );

    TestTrue(
        TEXT("A cave network occupies a meaningful connected volume rather than isolated noise specks"),
        CarvedSampleCount > 128
    );

    TestTrue(
        TEXT("Cave clearance preserves terrain near the surface"),
        DensityField.Sample(
            FIntVector(0, 0, 31)
        ).IsSolid()
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensityCaveDeterminismTest,
    "Orakai.Cubus.Density.NativeTerrain.CaveNetworkDeterminism",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDensityCaveDeterminismTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    FCubusTerrainDensitySettings Settings;
    Settings.bUseHeightTerrain = false;
    Settings.FlatSurfaceWorldZ = 48.0f;
    Settings.bGenerateCaves = true;
    Settings.CaveMinimumWorldZ = -48;
    Settings.CaveMaximumWorldZ = 32;
    Settings.CaveSurfaceClearance = 4;
    Settings.CaveThreshold = 0.55f;
    Settings.CaveNetworkCellSize = 18.0f;
    Settings.CaveTunnelRadius = 3.25f;
    Settings.CaveOffsetX = 731;
    Settings.CaveOffsetY = -199;
    Settings.CaveOffsetZ = 43;

    const FCubusTerrainDensityField FieldA(Settings);
    const FCubusTerrainDensityField FieldB(Settings);

    for (int32 Z = -16; Z <= 16; Z += 4)
    {
        for (int32 Y = -24; Y <= 24; Y += 4)
        {
            for (int32 X = -24; X <= 24; X += 4)
            {
                const FVector Position(
                    static_cast<double>(X) + 0.35,
                    static_cast<double>(Y) + 0.65,
                    static_cast<double>(Z) + 0.15
                );
                const FCubusDensitySample A = FieldA.SampleContinuous(Position);
                const FCubusDensitySample B = FieldB.SampleContinuous(Position);

                TestTrue(
                    TEXT("Cave density is deterministic at fractional coordinates"),
                    FMath::IsNearlyEqual(A.Density, B.Density, KINDA_SMALL_NUMBER)
                );
                TestEqual(
                    TEXT("Cave material classification is deterministic"),
                    A.MaterialId,
                    B.MaterialId
                );
            }
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensityVolumetricGeologyTest,
    "Orakai.Cubus.Density.NativeTerrain.VolumetricGeology",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDensityVolumetricGeologyTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    FCubusTerrainDensitySettings GeologySettings;
    GeologySettings.bGenerateRivers = false;
    GeologySettings.bGenerateCaves = false;
    GeologySettings.bGenerateVolumetricGeology = true;
    GeologySettings.MountainThreshold = -0.90f;
    GeologySettings.MountainBlend = 0.05f;
    GeologySettings.RidgeAmplitude = 28.0f;
    GeologySettings.HillAmplitude = 14.0f;
    GeologySettings.GeologySurfaceBand = 36.0f;
    GeologySettings.GeologyCliffSlopeStart = 0.05f;
    GeologySettings.GeologyCliffSlopeFull = 0.25f;
    GeologySettings.GeologyShelfStrength = 7.0f;
    GeologySettings.GeologyOverhangStrength = 11.0f;
    GeologySettings.GeologyMassStrength = 5.0f;

    FCubusTerrainDensitySettings HeightOnlySettings = GeologySettings;
    HeightOnlySettings.bGenerateVolumetricGeology = false;

    const FCubusTerrainDensityField GeologyField(GeologySettings);
    const FCubusTerrainDensityField HeightOnlyField(HeightOnlySettings);

    bool bFoundRockBeyondHeightEnvelope = false;
    bool bFoundFractionalDensityVariation = false;

    for (int32 Y = -128; Y <= 128 && !bFoundRockBeyondHeightEnvelope; Y += 4)
    {
        for (int32 X = -128; X <= 128; X += 4)
        {
            const float SurfaceHeight = GeologyField.SampleSurfaceVoxelHeight(
                static_cast<float>(X),
                static_cast<float>(Y)
            );
            const FVector AboveSurface(
                static_cast<double>(X) + 0.5,
                static_cast<double>(Y) + 0.5,
                static_cast<double>(SurfaceHeight) + 1.25
            );

            const FCubusDensitySample GeologicalSample = GeologyField.SampleContinuous(AboveSurface);
            const FCubusDensitySample HeightOnlySample = HeightOnlyField.SampleContinuous(AboveSurface);

            if (GeologicalSample.IsSolid() && !HeightOnlySample.IsSolid())
            {
                bFoundRockBeyondHeightEnvelope = true;

                const FCubusDensitySample FractionalNeighbour = GeologyField.SampleContinuous(
                    AboveSurface + FVector(0.10, 0.0, 0.0)
                );
                bFoundFractionalDensityVariation = !FMath::IsNearlyEqual(
                    GeologicalSample.Density,
                    FractionalNeighbour.Density,
                    0.0001f
                );
                break;
            }
        }
    }

    TestTrue(
        TEXT("Volumetric geology can create solid rock outside the base height envelope"),
        bFoundRockBeyondHeightEnvelope
    );

    TestTrue(
        TEXT("The geological field contains meaningful sub-voxel density variation at 10 cm spacing"),
        bFoundFractionalDensityVariation
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusTerrainDensityLayeringTest,
    "Orakai.Cubus.Density.NativeTerrain.MaterialLayers",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusTerrainDensityLayeringTest::RunTest(
    const FString& Parameters
)
{
    (void)Parameters;

    FCubusTerrainDensitySettings Settings;
    Settings.bUseHeightTerrain = false;
    Settings.FlatSurfaceWorldZ = 10.0f;
    Settings.SurfaceMaterialId = 11;
    Settings.SubsurfaceMaterialId = 17;
    Settings.RockMaterialId = 23;
    Settings.SurfaceMaterialDepth = 2.0f;
    Settings.RockMaterialDepth = 5.0f;

    const FCubusTerrainDensityField DensityField(Settings);

    TestEqual(
        TEXT("The exposed density surface uses the surface material"),
        DensityField.Sample(FIntVector(0, 0, 10)).MaterialId,
        11
    );
    TestEqual(
        TEXT("Shallow density terrain uses the soil/subsurface material"),
        DensityField.Sample(FIntVector(0, 0, 8)).MaterialId,
        17
    );
    TestEqual(
        TEXT("Deep density terrain resolves to rock"),
        DensityField.Sample(FIntVector(0, 0, 5)).MaterialId,
        23
    );

    return true;
}

#endif