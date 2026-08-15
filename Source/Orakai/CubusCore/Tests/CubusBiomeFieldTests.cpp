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

    float MinimumLowlandTemperature = 1.0f;
    float MaximumLowlandTemperature = 0.0f;
    for (int32 SampleY = -4096; SampleY <= 4096; SampleY += 512)
    {
        for (int32 SampleX = -4096; SampleX <= 4096; SampleX += 512)
        {
            const FCubusBiomeSample ClimateSample = FCubusBiomeField::Sample(
                static_cast<float>(SampleX),
                static_cast<float>(SampleY),
                Settings.HydrologySettings.SeaLevel,
                0.10f,
                Settings
            );
            MinimumLowlandTemperature = FMath::Min(MinimumLowlandTemperature, ClimateSample.Temperature);
            MaximumLowlandTemperature = FMath::Max(MaximumLowlandTemperature, ClimateSample.Temperature);
        }
    }

    TestTrue(TEXT("Seeded lowlands include genuinely cold climate"), MinimumLowlandTemperature < 0.38f);
    TestTrue(TEXT("Seeded lowlands include genuinely warm climate"), MaximumLowlandTemperature > 0.62f);
    TestTrue(TEXT("Climate varies substantially without relying on elevation"),
        MaximumLowlandTemperature - MinimumLowlandTemperature > 0.30f);

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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBiomeElevationStructureTest,
    "Orakai.Cubus.Generation.BiomeElevationStructure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusBiomeElevationStructureTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusBiomeFieldSettings Settings;
    Settings.bEnabled = true;
    Settings.NivalWorldZ = 72.0f;
    Settings.BiomeOffsetX = 933;
    Settings.BiomeOffsetY = -1701;

    const FCubusBiomeSample Lowland = FCubusBiomeField::Sample(20.0f, 30.0f, 8.0f, 0.08f, Settings);
    const FCubusBiomeSample Montane = FCubusBiomeField::Sample(20.0f, 30.0f, 44.0f, 0.08f, Settings);
    const FCubusBiomeSample Subalpine = FCubusBiomeField::Sample(20.0f, 30.0f, 60.0f, 0.08f, Settings);
    const FCubusBiomeSample Alpine = FCubusBiomeField::Sample(20.0f, 30.0f, 68.0f, 0.08f, Settings);
    const FCubusBiomeSample Nival = FCubusBiomeField::Sample(20.0f, 30.0f, 80.0f, 0.08f, Settings);

    TestEqual(TEXT("Sea-level terrain is lowland"), Lowland.ElevationZone, ECubusElevationZone::Lowland);
    TestEqual(TEXT("Mid mountain terrain is montane"), Montane.ElevationZone, ECubusElevationZone::Montane);
    TestEqual(TEXT("Upper mountain terrain is subalpine"), Subalpine.ElevationZone, ECubusElevationZone::Subalpine);
    TestEqual(TEXT("Terrain below the snow datum can be alpine"), Alpine.ElevationZone, ECubusElevationZone::Alpine);
    TestEqual(TEXT("Terrain above the nival datum is nival"), Nival.ElevationZone, ECubusElevationZone::Nival);
    TestTrue(TEXT("Treeline fades with relative elevation"), Lowland.TreeLineWeight > Subalpine.TreeLineWeight && Subalpine.TreeLineWeight > Alpine.TreeLineWeight);
    TestTrue(TEXT("Nival influence grows only at the top elevation band"), Nival.NivalInfluence > Alpine.NivalInfluence);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBiomeAspectAndWaterTest,
    "Orakai.Cubus.Generation.BiomeAspectAndWater",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusBiomeAspectAndWaterTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusBiomeFieldSettings Settings;
    Settings.bEnabled = true;
    Settings.NivalWorldZ = 72.0f;
    Settings.BiomeOffsetX = 1221;
    Settings.BiomeOffsetY = 381;

    FCubusBiomeTerrainContext SunnyContext;
    SunnyContext.Gradient = FVector2D(0.42f, 0.91f);
    FCubusBiomeTerrainContext ShadyContext = SunnyContext;
    ShadyContext.Gradient *= -1.0f;

    const FCubusBiomeSample Sunny = FCubusBiomeField::Sample(100.0f, -30.0f, 28.0f, 0.80f, Settings, SunnyContext);
    const FCubusBiomeSample Shady = FCubusBiomeField::Sample(100.0f, -30.0f, 28.0f, 0.80f, Settings, ShadyContext);
    TestTrue(TEXT("Opposite slope aspects produce different solar exposure"), Sunny.SolarExposure > Shady.SolarExposure + 0.15f);

    FCubusBiomeTerrainContext WindwardContext;
    WindwardContext.MountainCore = 0.8f;
    WindwardContext.FoothillWeight = 0.8f;
    WindwardContext.Ridge = 0.7f;
    WindwardContext.Gradient = FVector2D(0.82f, 0.57f);
    FCubusBiomeTerrainContext LeewardContext = WindwardContext;
    LeewardContext.Gradient *= -1.0f;

    const FCubusBiomeSample Windward = FCubusBiomeField::Sample(100.0f, -30.0f, 36.0f, 0.70f, Settings, WindwardContext);
    const FCubusBiomeSample Leeward = FCubusBiomeField::Sample(100.0f, -30.0f, 36.0f, 0.70f, Settings, LeewardContext);
    TestTrue(TEXT("Windward slope reports greater wind exposure"), Windward.WindExposure > Leeward.WindExposure);
    TestTrue(TEXT("Leeward slope develops a stronger rain shadow"), Leeward.RainShadow > Windward.RainShadow);

    FCubusBiomeTerrainContext DryContext;
    DryContext.Drainage = 0.0f;
    FCubusBiomeTerrainContext WetContext;
    WetContext.Drainage = 1.0f;
    const FCubusBiomeSample Dry = FCubusBiomeField::Sample(-400.0f, 250.0f, 16.0f, 0.10f, Settings, DryContext);
    const FCubusBiomeSample Wet = FCubusBiomeField::Sample(-400.0f, 250.0f, 16.0f, 0.10f, Settings, WetContext);
    TestTrue(TEXT("Convergent terrain increases surface wetness"), Wet.SurfaceWetness > Dry.SurfaceWetness);
    TestTrue(TEXT("Convergent terrain increases soil saturation"), Wet.SoilSaturation > Dry.SoilSaturation);
    TestTrue(TEXT("Convergent terrain increases groundwater potential"), Wet.GroundwaterPotential > Dry.GroundwaterPotential);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBiomeCommunityBlendTest,
    "Orakai.Cubus.Generation.BiomeCommunityBlend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusBiomeCommunityBlendTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusBiomeFieldSettings Settings;
    Settings.bEnabled = true;
    Settings.NivalWorldZ = 72.0f;
    Settings.BiomeOffsetX = 191;
    Settings.BiomeOffsetY = -817;

    for (int32 Index = 0; Index < 5; ++Index)
    {
        FCubusBiomeDefinition Definition;
        Definition.Name = FName(*FString::Printf(TEXT("Blend_%d"), Index));
        Definition.Archetype = Index % 2 == 0 ? ECubusBiomeKind::Forest : ECubusBiomeKind::Plains;
        Definition.SurfaceMaterialId = 60 + Index;
        Definition.TargetMoisture = 0.5f;
        Definition.MoistureTolerance = 1.0f;
        Definition.TargetTemperature = 0.5f;
        Definition.TemperatureTolerance = 1.0f;
        Definition.MinimumWorldZ = -1000.0f;
        Definition.MaximumWorldZ = 1000.0f;
        Definition.MaximumSlope = 100.0f;
        Definition.PatchStrength = 0.0f;
        Definition.Priority = static_cast<float>(Index + 1);
        Settings.Definitions.Add(Definition);
    }

    const FCubusBiomeSample Sample = FCubusBiomeField::Sample(32.0f, 48.0f, 20.0f, 0.10f, Settings);
    TestEqual(TEXT("Biome sample retains four competing communities"), Sample.CommunityBlendCount, 4);
    TestEqual(TEXT("Highest-priority matching community remains dominant"), Sample.CommunityBlend[0].DefinitionIndex, 4);
    TestEqual(TEXT("Compatibility winner follows top community"), Sample.BiomeDefinitionIndex, 4);

    float BlendTotal = 0.0f;
    for (int32 Index = 0; Index < Sample.CommunityBlendCount; ++Index)
    {
        BlendTotal += Sample.CommunityBlend[Index].Weight;
        if (Index > 0)
        {
  TestTrue(TEXT("Community blend is sorted by descending weight"),
      Sample.CommunityBlend[Index - 1].Weight >= Sample.CommunityBlend[Index].Weight);
        }
    }
    TestTrue(TEXT("Community blend weights are normalized"), FMath::IsNearlyEqual(BlendTotal, 1.0f, 0.001f));
    return true;
}

#endif
