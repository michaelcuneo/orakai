#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusBiomeField.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBiomeClimateFieldTest,
    "Orakai.Cubus.Generation.BiomeClimateField",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusBiomeClimateFieldTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusBiomeFieldSettings Settings;
    Settings.bEnabled = true;
    Settings.BiomeOffsetX = 1387;
    Settings.BiomeOffsetY = -2911;
    Settings.PlainsSurfaceMaterialId = 11;
    Settings.ForestSurfaceMaterialId = 17;
    Settings.RockySurfaceMaterialId = 23;
    Settings.WetlandSurfaceMaterialId = 31;

    const FCubusBiomeSample First = FCubusBiomeField::Sample(
        120.0f, -75.0f, 16.0f, 0.25f, Settings
    );
    const FCubusBiomeSample Repeated = FCubusBiomeField::Sample(
        120.0f, -75.0f, 16.0f, 0.25f, Settings
    );
    const FCubusBiomeSample Adjacent = FCubusBiomeField::Sample(
        121.0f, -75.0f, 16.0f, 0.25f, Settings
    );

    TestEqual(TEXT("Biome sampling is deterministic"), First.SurfaceMaterialId, Repeated.SurfaceMaterialId);
    TestEqual(TEXT("Biome moisture is deterministic"), First.Moisture, Repeated.Moisture);
    TestTrue(
        TEXT("Biome weights are normalized"),
        FMath::IsNearlyEqual(
            First.PlainsWeight + First.ForestWeight + First.RockyWeight + First.WetlandWeight,
            1.0f,
            0.001f
        )
    );
    TestTrue(
        TEXT("Climate varies continuously between adjacent columns"),
        FMath::Abs(First.Moisture - Adjacent.Moisture) < 0.08f
    );
    TestTrue(
        TEXT("Unbound biome settings do not invent a river network"),
        FMath::IsNearlyEqual(First.RiverDistance, 1.0f)
    );

    const FCubusBiomeSample Cliff = FCubusBiomeField::Sample(
        120.0f,
        -75.0f,
        16.0f,
        Settings.RockySlopeThreshold * 2.0f,
        Settings
    );
    TestEqual(TEXT("Steep terrain resolves to the rocky archetype"), Cliff.DominantBiome, ECubusBiomeKind::Rocky);

    FCubusBiomeFieldSettings CustomSettings = Settings;
    CustomSettings.Definitions.Reset();

    FCubusBiomeDefinition DryForest;
    DryForest.Name = TEXT("DryForest");
    DryForest.Archetype = ECubusBiomeKind::Forest;
    DryForest.SurfaceMaterialId = 46;
    DryForest.TargetMoisture = First.Moisture;
    DryForest.MoistureTolerance = 1.0f;
    DryForest.TargetTemperature = First.Temperature;
    DryForest.TemperatureTolerance = 1.0f;
    DryForest.MinimumWorldZ = -1000.0f;
    DryForest.MaximumWorldZ = 1000.0f;
    DryForest.MaximumSlope = 2.0f;
    DryForest.Priority = 100.0f;
    CustomSettings.Definitions.Add(DryForest);

    const FCubusBiomeSample Custom = FCubusBiomeField::Sample(
        120.0f, -75.0f, 16.0f, 0.25f, CustomSettings
    );

    TestEqual(TEXT("Authored biome definition wins by its own envelope"), Custom.BiomeDefinitionIndex, 0);
    TestEqual(TEXT("Authored biome identity is preserved"), Custom.BiomeName, FName(TEXT("DryForest")));
    TestEqual(TEXT("Authored biome material is preserved"), Custom.SurfaceMaterialId, 46);
    TestEqual(TEXT("Authored biome keeps its ecology archetype"), Custom.DominantBiome, ECubusBiomeKind::Forest);
    TestTrue(TEXT("Authored biome exposes a normalized suitability"), Custom.BiomeStrength > 0.0f && Custom.BiomeStrength <= 1.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusDensityBiomeContextTest,
    "Orakai.Cubus.Generation.DensityBiomeContext",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusDensityBiomeContextTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainDensitySettings Settings;
    Settings.BiomeSettings.bEnabled = true;
    Settings.BiomeSettings.BiomeOffsetX = 1731;
    Settings.BiomeSettings.BiomeOffsetY = -2449;
    Settings.BiomeSettings.PlainsSurfaceMaterialId = 11;
    Settings.BiomeSettings.ForestSurfaceMaterialId = 17;
    Settings.BiomeSettings.RockySurfaceMaterialId = 23;
    Settings.BiomeSettings.WetlandSurfaceMaterialId = 31;

    Settings.bGenerateRivers = true;
    Settings.RiverSeed = 91237;
    Settings.TerrainOffsetX = 320;
    Settings.TerrainOffsetY = -224;

    FCubusBiomeDefinition Definition;
    Definition.Name = TEXT("DensityWorldBiome");
    Definition.Archetype = ECubusBiomeKind::Plains;
    Definition.SurfaceMaterialId = 47;
    Definition.TargetMoisture = 0.5f;
    Definition.MoistureTolerance = 1.0f;
    Definition.TargetTemperature = 0.5f;
    Definition.TemperatureTolerance = 1.0f;
    Definition.MinimumWorldZ = -10000.0f;
    Definition.MaximumWorldZ = 10000.0f;
    Definition.MaximumSlope = 100.0f;
    Definition.Priority = 100.0f;
    Settings.BiomeSettings.Definitions.Add(Definition);

    const FCubusTerrainDensityField DensityField(Settings);
    const float WorldX = 96.0f;
    const float WorldY = -48.0f;

    const FCubusBiomeSample First = DensityField.SampleSurfaceBiome(WorldX, WorldY);
    const FCubusBiomeSample Repeated = DensityField.SampleSurfaceBiome(WorldX, WorldY);
    const float SurfaceHeight = DensityField.SampleSurfaceVoxelHeight(WorldX, WorldY);

    TestEqual(TEXT("Density biome sampling is deterministic"), First.BiomeName, Repeated.BiomeName);
    TestTrue(
        TEXT("Density biome sample owns the density surface height"),
        FMath::IsNearlyEqual(First.SurfaceWorldZ, SurfaceHeight, 0.001f)
    );
    TestTrue(TEXT("Density biome sample exposes a finite slope"), FMath::IsFinite(First.Slope) && First.Slope >= 0.0f);
    TestEqual(TEXT("Density authored biome is selected"), First.BiomeName, FName(TEXT("DensityWorldBiome")));

    return true;
}

#endif
