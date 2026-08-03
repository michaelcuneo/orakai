#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Generation/CubusBiomeField.h"

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
    Settings.bGenerateRivers = true;
    Settings.BiomeOffsetX = 1387;
    Settings.BiomeOffsetY = -2911;
    Settings.RiverOffsetX = -617;
    Settings.RiverOffsetY = 2089;
    Settings.PlainsSurfaceMaterialId = 11;
    Settings.ForestSurfaceMaterialId = 17;
    Settings.RockySurfaceMaterialId = 23;
    Settings.WetlandSurfaceMaterialId = 31;

    const FCubusBiomeSample First = FCubusBiomeField::Sample(
        120.0f,
        -75.0f,
        16.0f,
        0.25f,
        Settings
    );
    const FCubusBiomeSample Repeated = FCubusBiomeField::Sample(
        120.0f,
        -75.0f,
        16.0f,
        0.25f,
        Settings
    );
    const FCubusBiomeSample Adjacent = FCubusBiomeField::Sample(
        121.0f,
        -75.0f,
        16.0f,
        0.25f,
        Settings
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
        TEXT("Client-authored material IDs above five are preserved"),
        First.SurfaceMaterialId == 11 ||
        First.SurfaceMaterialId == 17 ||
        First.SurfaceMaterialId == 23 ||
        First.SurfaceMaterialId == 31
    );

    const FCubusBiomeSample Cliff = FCubusBiomeField::Sample(
        120.0f,
        -75.0f,
        16.0f,
        Settings.RockySlopeThreshold * 2.0f,
        Settings
    );
    TestEqual(TEXT("Steep terrain resolves to the rocky biome"), Cliff.SurfaceMaterialId, 23);

    FCubusBiomeFieldSettings CustomSettings = Settings;
    CustomSettings.Definitions.Reset();
    for (int32 Index = 0; Index < 7; ++Index)
    {
        FCubusBiomeDefinition Definition;
        Definition.Name = FName(*FString::Printf(TEXT("Custom_%d"), Index));
        Definition.Archetype = ECubusBiomeKind::Plains;
        Definition.SurfaceMaterialId = 40 + Index;
        Definition.TargetMoisture = First.Moisture;
        Definition.MoistureTolerance = 1.0f;
        Definition.TargetTemperature = First.Temperature;
        Definition.TemperatureTolerance = 1.0f;
        Definition.Priority = static_cast<float>(Index + 1);
        CustomSettings.Definitions.Add(Definition);
    }
    const FCubusBiomeSample Custom = FCubusBiomeField::Sample(
        120.0f,
        -75.0f,
        16.0f,
        0.25f,
        CustomSettings
    );
    TestEqual(
        TEXT("Client-defined biome counts are not capped at five"),
        Custom.BiomeDefinitionIndex,
        6
    );
    TestEqual(
        TEXT("The selected custom biome preserves its material"),
        Custom.SurfaceMaterialId,
        46
    );

    bool bFoundForest = false;
    bool bFoundWetland = false;
    bool bFoundPlains = false;
    for (int32 Y = -768; Y <= 768; Y += 16)
    {
        for (int32 X = -768; X <= 768; X += 16)
        {
            const FCubusBiomeSample Sample = FCubusBiomeField::Sample(
                static_cast<float>(X),
                static_cast<float>(Y),
                12.0f,
                0.1f,
                Settings
            );
            bFoundForest |= Sample.DominantBiome == ECubusBiomeKind::Forest;
            bFoundWetland |= Sample.DominantBiome == ECubusBiomeKind::Wetland;
            bFoundPlains |= Sample.DominantBiome == ECubusBiomeKind::Plains;
        }
    }

    TestTrue(TEXT("Climate field creates forest regions"), bFoundForest);
    TestTrue(TEXT("Drainage and moisture create wetlands"), bFoundWetland);
    TestTrue(TEXT("Climate field retains open plains"), bFoundPlains);
    return true;
}

#endif
