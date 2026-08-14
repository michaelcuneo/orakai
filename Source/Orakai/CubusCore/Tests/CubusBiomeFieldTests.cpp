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

    const FCubusBiomeSample First = FCubusBiomeField::Sample(120.0f, -75.0f, 16.0f, 0.25f, Settings);
    const FCubusBiomeSample Repeated = FCubusBiomeField::Sample(120.0f, -75.0f, 16.0f, 0.25f, Settings);
    const FCubusBiomeSample Adjacent = FCubusBiomeField::Sample(121.0f, -75.0f, 16.0f, 0.25f, Settings);

    TestEqual(TEXT("Biome sampling is deterministic"), First.SurfaceMaterialId, Repeated.SurfaceMaterialId);
    TestEqual(TEXT("Biome moisture is deterministic"), First.Moisture, Repeated.Moisture);
    TestTrue(TEXT("Biome weights are normalized"),
        FMath::IsNearlyEqual(First.PlainsWeight + First.ForestWeight + First.RockyWeight + First.WetlandWeight, 1.0f, 0.001f));
    TestTrue(TEXT("Climate varies continuously between adjacent columns"), FMath::Abs(First.Moisture - Adjacent.Moisture) < 0.08f);
    TestTrue(TEXT("Unbound biome settings do not invent a river network"), FMath::IsNearlyEqual(First.RiverDistance, 1.0f));
    TestTrue(TEXT("Biome sample exposes normalized soil"), First.SoilDepth >= 0.0f && First.SoilDepth <= 1.0f);
    TestTrue(TEXT("Biome sample exposes normalized fertility"), First.Fertility >= 0.0f && First.Fertility <= 1.0f);
    TestTrue(TEXT("Biome sample exposes normalized exposure"), First.Exposure >= 0.0f && First.Exposure <= 1.0f);

    const FCubusBiomeSample Cliff = FCubusBiomeField::Sample(
        120.0f, -75.0f, 16.0f, Settings.RockySlopeThreshold * 2.0f, Settings);
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

    const FCubusBiomeSample Custom = FCubusBiomeField::Sample(120.0f, -75.0f, 16.0f, 0.25f, CustomSettings);

    TestEqual(TEXT("Authored biome definition wins by its own envelope"), Custom.BiomeDefinitionIndex, 0);
    TestEqual(TEXT("Authored biome identity is preserved"), Custom.BiomeName, FName(TEXT("DryForest")));
    TestEqual(TEXT("Authored biome material is preserved"), Custom.SurfaceMaterialId, 46);
    TestEqual(TEXT("Authored biome keeps its ecology archetype"), Custom.DominantBiome, ECubusBiomeKind::Forest);
    TestTrue(TEXT("Authored biome exposes a normalized suitability"), Custom.BiomeStrength > 0.0f && Custom.BiomeStrength <= 1.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBiomeEcologicalNicheTest,
    "Orakai.Cubus.Generation.BiomeEcologicalNiches",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusBiomeEcologicalNicheTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusBiomeFieldSettings Settings;
    Settings.bEnabled = true;
    Settings.BiomeOffsetX = 771;
    Settings.BiomeOffsetY = -143;

    FCubusBiomeDefinition ShelteredForest;
    ShelteredForest.Name = TEXT("ShelteredForest");
    ShelteredForest.Archetype = ECubusBiomeKind::Forest;
    ShelteredForest.SurfaceMaterialId = 51;
    ShelteredForest.TargetMoisture = 0.5f;
    ShelteredForest.MoistureTolerance = 1.0f;
    ShelteredForest.TargetTemperature = 0.5f;
    ShelteredForest.TemperatureTolerance = 1.0f;
    ShelteredForest.MinimumWorldZ = -1000.0f;
    ShelteredForest.MaximumWorldZ = 1000.0f;
    ShelteredForest.MaximumSlope = 4.0f;
    ShelteredForest.MinimumSoilDepth = 0.35f;
    ShelteredForest.MaximumSoilDepth = 1.0f;
    ShelteredForest.MinimumRockExposure = 0.0f;
    ShelteredForest.MaximumRockExposure = 0.25f;
    ShelteredForest.MinimumExposure = 0.0f;
    ShelteredForest.MaximumExposure = 0.45f;
    ShelteredForest.MinimumFertility = 0.25f;
    ShelteredForest.Priority = 20.0f;
    Settings.Definitions.Add(ShelteredForest);

    FCubusBiomeDefinition BareRidge = ShelteredForest;
    BareRidge.Name = TEXT("BareRidge");
    BareRidge.Archetype = ECubusBiomeKind::Rocky;
    BareRidge.SurfaceMaterialId = 52;
    BareRidge.MinimumSoilDepth = 0.0f;
    BareRidge.MaximumSoilDepth = 0.28f;
    BareRidge.MinimumRockExposure = 0.45f;
    BareRidge.MaximumRockExposure = 1.0f;
    BareRidge.MinimumExposure = 0.45f;
    BareRidge.MaximumExposure = 1.0f;
    BareRidge.MinimumFertility = 0.0f;
    BareRidge.MaximumFertility = 0.5f;
    Settings.Definitions.Add(BareRidge);

    FCubusBiomeTerrainContext ShelteredContext;
    ShelteredContext.Drainage = 0.55f;
    ShelteredContext.RockExposure = 0.05f;
    ShelteredContext.FoothillWeight = 0.35f;
    ShelteredContext.Gradient = FVector2D(-0.1f, -0.1f);

    FCubusBiomeTerrainContext RidgeContext;
    RidgeContext.Drainage = 0.0f;
    RidgeContext.RockExposure = 0.95f;
    RidgeContext.MountainCore = 1.0f;
    RidgeContext.Ridge = 1.0f;
    RidgeContext.Gradient = FVector2D(1.0f, 0.8f);

    const FCubusBiomeSample Sheltered = FCubusBiomeField::Sample(64.0f, 64.0f, 20.0f, 0.10f, Settings, ShelteredContext);
    const FCubusBiomeSample Ridge = FCubusBiomeField::Sample(64.0f, 64.0f, 20.0f, 1.2f, Settings, RidgeContext);

    TestTrue(TEXT("Sheltered terrain retains more soil than exposed ridge terrain"), Sheltered.SoilDepth > Ridge.SoilDepth);
    TestTrue(TEXT("Exposed ridge reports greater ecological exposure"), Ridge.Exposure > Sheltered.Exposure);
    TestTrue(TEXT("Explicit rock context reaches biome sample"), Ridge.RockExposure > Sheltered.RockExposure);
    TestEqual(TEXT("Sheltered niche selects authored forest"), Sheltered.BiomeName, FName(TEXT("ShelteredForest")));
    TestEqual(TEXT("Exposed niche selects authored ridge biome"), Ridge.BiomeName, FName(TEXT("BareRidge")));
    TestEqual(TEXT("Niche-specific surface material follows authored biome"), Ridge.SurfaceMaterialId, 52);

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
    TestTrue(TEXT("Density biome sample owns the density surface height"), FMath::IsNearlyEqual(First.SurfaceWorldZ, SurfaceHeight, 0.001f));
    TestTrue(TEXT("Density biome sample exposes a finite slope"), FMath::IsFinite(First.Slope) && First.Slope >= 0.0f);
    TestEqual(TEXT("Density authored biome is selected"), First.BiomeName, FName(TEXT("DensityWorldBiome")));
    TestTrue(TEXT("Density biome exposes ecological soil"), First.SoilDepth >= 0.0f && First.SoilDepth <= 1.0f);
    TestTrue(TEXT("Density biome exposes ecological drainage"), First.Drainage >= 0.0f && First.Drainage <= 1.0f);

    return true;
}

#endif
