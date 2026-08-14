#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Vegetation/CubusVegetationCatalog.h"
#include "CubusCore/Vegetation/CubusVegetationTypes.h"
#include "UObject/SoftObjectPath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusVegetationHabitatQuantizationTest,
    "Orakai.Cubus.Vegetation.HabitatQuantization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusVegetationHabitatQuantizationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusVegetationHabitatSample Sample;
    Sample.Moisture = FCubusVegetationHabitatSample::QuantizeUnit(0.73f);
    Sample.Temperature = FCubusVegetationHabitatSample::QuantizeUnit(0.28f);
    Sample.SoilDepth = FCubusVegetationHabitatSample::QuantizeUnit(0.61f);
    Sample.SlopeDegrees = FCubusVegetationHabitatSample::QuantizeSlopeDegrees(37.0f);

    TestTrue(TEXT("Moisture survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetMoisture(), 0.73f, 0.005f));
    TestTrue(TEXT("Temperature survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetTemperature(), 0.28f, 0.005f));
    TestTrue(TEXT("Soil depth survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetSoilDepth(), 0.61f, 0.005f));
    TestTrue(TEXT("Slope survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetSlopeDegrees(), 37.0f, 0.5f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusVegetationSpeciesHabitatSelectionTest,
    "Orakai.Cubus.Vegetation.SpeciesHabitatSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusVegetationSpeciesHabitatSelectionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    TArray<FCubusVegetationSpeciesCatalogEntry> Species;

    FCubusVegetationSpeciesCatalogEntry WetSpecies;
    WetSpecies.SpeciesId = TEXT("WetBroadleaf");
    WetSpecies.TypeId = CubusVegetationType::BroadleafTree;
    WetSpecies.BiomeMask = static_cast<int32>(ECubusVegetationBiome::Forest) |
        static_cast<int32>(ECubusVegetationBiome::Wetland);
    WetSpecies.Habitat.MinimumMoisture = 0.68f;
    WetSpecies.Habitat.MinimumSoilDepth = 0.45f;
    WetSpecies.Habitat.MaximumRockExposure = 0.45f;
    WetSpecies.Habitat.TransitionSoftness = 0.04f;
    WetSpecies.GrowthStageMeshes.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/Wet.Wet"))));
    Species.Add(WetSpecies);

    FCubusVegetationSpeciesCatalogEntry DrySpecies;
    DrySpecies.SpeciesId = TEXT("DryBroadleaf");
    DrySpecies.TypeId = CubusVegetationType::BroadleafTree;
    DrySpecies.BiomeMask = static_cast<int32>(ECubusVegetationBiome::Forest) |
        static_cast<int32>(ECubusVegetationBiome::Plains);
    DrySpecies.Habitat.MaximumMoisture = 0.32f;
    DrySpecies.Habitat.MaximumSoilDepth = 0.62f;
    DrySpecies.Habitat.MaximumRiverInfluence = 0.30f;
    DrySpecies.Habitat.TransitionSoftness = 0.04f;
    DrySpecies.GrowthStageMeshes.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Game/Test/Dry.Dry"))));
    Species.Add(DrySpecies);

    FCubusVegetationCatalog Catalog;
    Catalog.Rebuild(Species);

    FCubusVegetationInstance WetInstance;
    WetInstance.WorldVoxel = FIntVector(12, 18, 4);
    WetInstance.TypeId = CubusVegetationType::BroadleafTree;
    WetInstance.BiomeMask = CubusVegetationBiome::Forest | CubusVegetationBiome::Wetland;
    WetInstance.Habitat.Moisture = FCubusVegetationHabitatSample::QuantizeUnit(0.86f);
    WetInstance.Habitat.SoilDepth = FCubusVegetationHabitatSample::QuantizeUnit(0.78f);
    WetInstance.Habitat.RockExposure = FCubusVegetationHabitatSample::QuantizeUnit(0.12f);
    WetInstance.Habitat.RiverInfluence = FCubusVegetationHabitatSample::QuantizeUnit(0.72f);

    const int32 WetSelection = Catalog.SelectSpeciesIndex(WetInstance, Species, false, 12, 1337);
    const int32 WetRepeated = Catalog.SelectSpeciesIndex(WetInstance, Species, false, 12, 1337);
    TestEqual(TEXT("Wet habitat selects wet-adapted species"), WetSelection, 0);
    TestEqual(TEXT("Habitat-driven selection is deterministic"), WetRepeated, WetSelection);

    FCubusVegetationInstance DryInstance = WetInstance;
    DryInstance.WorldVoxel = FIntVector(-31, 7, 5);
    DryInstance.BiomeMask = CubusVegetationBiome::Forest | CubusVegetationBiome::Plains;
    DryInstance.Habitat.Moisture = FCubusVegetationHabitatSample::QuantizeUnit(0.16f);
    DryInstance.Habitat.SoilDepth = FCubusVegetationHabitatSample::QuantizeUnit(0.34f);
    DryInstance.Habitat.RiverInfluence = FCubusVegetationHabitatSample::QuantizeUnit(0.05f);
    const int32 DrySelection = Catalog.SelectSpeciesIndex(DryInstance, Species, false, 12, 1337);
    TestEqual(TEXT("Dry habitat selects dry-adapted species"), DrySelection, 1);

    FCubusVegetationInstance UnsuitableInstance = WetInstance;
    UnsuitableInstance.WorldVoxel = FIntVector(100, 100, 10);
    UnsuitableInstance.BiomeMask = CubusVegetationBiome::Forest;
    UnsuitableInstance.Habitat.Moisture = FCubusVegetationHabitatSample::QuantizeUnit(0.50f);
    UnsuitableInstance.Habitat.SoilDepth = FCubusVegetationHabitatSample::QuantizeUnit(0.80f);
    UnsuitableInstance.Habitat.RiverInfluence = FCubusVegetationHabitatSample::QuantizeUnit(0.65f);
    const int32 UnsuitableSelection = Catalog.SelectSpeciesIndex(UnsuitableInstance, Species, false, 12, 1337);
    TestEqual(TEXT("Ecologically impossible species are rejected"), UnsuitableSelection, INDEX_NONE);

    return true;
}

#endif
