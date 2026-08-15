from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected one anchor, found {count}: {old[:120]!r}')
    p.write_text(text.replace(old, new, 1), encoding='utf-8')

biome = 'Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp'
tests = 'Source/Orakai/CubusCore/Tests/CubusTerrainFormTests.cpp'
density = 'Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp'

# Treat the original four generic Data Asset definitions as compatibility
# archetypes. They should never suppress the richer named community classifier.
anchor = '''    const float AlpineBand = Result.AlpineInfluence * (1.0f - Result.NivalInfluence);\n\n    if (Settings.Definitions.Num() == 0)\n    {\n'''
replacement = '''    const float AlpineBand = Result.AlpineInfluence * (1.0f - Result.NivalInfluence);\n\n    const auto IsLegacyArchetypeDefinition = [](const FCubusBiomeDefinition& Definition)\n    {\n        return Definition.Name == TEXT("Plains") ||\n            Definition.Name == TEXT("Forest") ||\n            Definition.Name == TEXT("Rocky") ||\n            Definition.Name == TEXT("Wetland");\n    };\n    bool bOnlyLegacyArchetypeDefinitions = !Settings.Definitions.IsEmpty();\n    for (const FCubusBiomeDefinition& Definition : Settings.Definitions)\n    {\n        bOnlyLegacyArchetypeDefinitions &= IsLegacyArchetypeDefinition(Definition);\n    }\n    const bool bUseBuiltInRealCommunities =\n        Settings.Definitions.IsEmpty() || bOnlyLegacyArchetypeDefinitions;\n\n    /*\n     * Plains / Forest / Rocky / Wetland are compatibility projections used by\n     * old material and vegetation masks. A Data Asset containing only those\n     * generic entries must not downgrade the ecology engine to four biomes.\n     * The built-in named communities remain active until the asset contains at\n     * least one genuinely authored community definition.\n     */\n    if (bUseBuiltInRealCommunities)\n    {\n'''
replace_once(biome, anchor, replacement)

# Physical geology frequencies: broad rock masses/outcrops need to exist at
# resolvable real-world scales. This does not increase the already-bounded
# displacement amplitude; it only stops the rock fabric repeating every ~40 m.
replace_once(density,
    '''\tconst float StructuralFrequency = FMath::Max(Settings.GeologyMassFrequency, 0.000001f);\n''',
    '''\tconst float StructuralFrequency = TerrainFormSettings.bUsePhysicalWorldScale\n\t\t? CubusWorldScale::FrequencyForWavelengthMeters(220.0f, Settings.VoxelSizeCm)\n\t\t: FMath::Max(Settings.GeologyMassFrequency, 0.000001f);\n''')
# Ensure include exists for the conversion helper.
p = Path(density)
text = p.read_text(encoding='utf-8')
if '#include "CubusCore/Generation/CubusWorldScale.h"' not in text:
    text = text.replace(
        '#include "CubusCore/Generation/CubusTerrainDensityField.h"\n',
        '#include "CubusCore/Generation/CubusTerrainDensityField.h"\n#include "CubusCore/Generation/CubusWorldScale.h"\n',
        1,
    )
p.write_text(text, encoding='utf-8')

# Existing detailed-shape test keeps exercising the legacy parameter path so
# its tight one-voxel curvature thresholds remain meaningful. Physical-scale
# geography is validated independently below in metres/km.
replace_once(tests,
    '''    FCubusTerrainFormSettings Settings;\n    FCubusTerrainFormSettings NoDetailSettings = Settings;\n''',
    '''    FCubusTerrainFormSettings Settings;\n    Settings.bUsePhysicalWorldScale = false;\n    FCubusTerrainFormSettings NoDetailSettings = Settings;\n''')

# Append a real-world scale automation test before #endif.
insert = r'''

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusPhysicalTerrainScaleTest,
    "Orakai.Cubus.Generation.PhysicalTerrainScale",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter
)

bool FCubusPhysicalTerrainScaleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusTerrainFormSettings Settings;
    Settings.VoxelSizeCm = 80.0f;
    Settings.bUsePhysicalWorldScale = true;

    const float MetersPerVoxel = Settings.VoxelSizeCm * 0.01f;
    const auto MetersToVoxels = [MetersPerVoxel](const float Meters)
    {
        return Meters / MetersPerVoxel;
    };

    TestTrue(
        TEXT("80 cm voxels convert one kilometre to 1250 canonical voxels"),
        FMath::IsNearlyEqual(MetersToVoxels(1000.0f), 1250.0f, 0.01f)
    );
    TestTrue(
        TEXT("Configured mountain relief represents kilometre-scale vertical terrain"),
        Settings.MountainReliefMeters >= 1000.0f
    );
    TestTrue(
        TEXT("Configured major valleys are hundreds of metres deep"),
        Settings.ValleyReliefMeters >= 250.0f
    );
    TestTrue(
        TEXT("Mountain systems are separated by regional real-world distances"),
        Settings.MountainSystemSpacingKm >= 10.0f
    );

    // Inspect a 40 km square at a 320 m lattice. This spans multiple intended
    // mountain-system and major-valley wavelengths at the 80 cm canonical scale.
    const int32 HalfExtent = FMath::RoundToInt(MetersToVoxels(20000.0f));
    const int32 Step = FMath::Max(1, FMath::RoundToInt(MetersToVoxels(320.0f)));

    float MinimumHeight = MAX_flt;
    float MaximumHeight = -MAX_flt;
    float MaximumMountainWeight = 0.0f;
    float MaximumDrainage = 0.0f;
    int32 MountainSamples = 0;
    int32 LowlandSamples = 0;

    for (int32 Y = -HalfExtent; Y <= HalfExtent; Y += Step)
    {
        for (int32 X = -HalfExtent; X <= HalfExtent; X += Step)
        {
            const FCubusTerrainFormSample Sample = FCubusTerrainForm::Sample(
                static_cast<float>(X),
                static_cast<float>(Y),
                Settings
            );
            MinimumHeight = FMath::Min(MinimumHeight, Sample.Height);
            MaximumHeight = FMath::Max(MaximumHeight, Sample.Height);
            MaximumMountainWeight = FMath::Max(MaximumMountainWeight, Sample.MountainWeight);
            MaximumDrainage = FMath::Max(MaximumDrainage, Sample.Drainage);
            MountainSamples += Sample.MountainWeight > 0.60f ? 1 : 0;
            LowlandSamples += Sample.MountainWeight < 0.25f ? 1 : 0;
        }
    }

    const float ReliefMeters = (MaximumHeight - MinimumHeight) * MetersPerVoxel;
    TestTrue(TEXT("Physical world contains mountain country"), MountainSamples > 0);
    TestTrue(TEXT("Physical world also contains broad non-mountain country"), LowlandSamples > 0);
    TestTrue(TEXT("Physical world contains major drainage structure"), MaximumDrainage > 0.55f);
    TestTrue(
        TEXT("Regional terrain relief reaches real mountain scale"),
        ReliefMeters >= 700.0f
    );
    TestTrue(
        TEXT("Regional terrain relief remains bounded below absurd vertical scale"),
        ReliefMeters <= 4200.0f
    );
    return true;
}
'''
p = Path(tests)
text = p.read_text(encoding='utf-8')
if '\n#endif' not in text:
    raise RuntimeError('test endif anchor missing')
text = text.replace('\n#endif', insert + '\n#endif', 1)
p.write_text(text, encoding='utf-8')

# Guards.
b = Path(biome).read_text(encoding='utf-8')
d = Path(density).read_text(encoding='utf-8')
t = Path(tests).read_text(encoding='utf-8')
for token in ['bOnlyLegacyArchetypeDefinitions', 'bUseBuiltInRealCommunities']:
    if token not in b:
        raise RuntimeError(f'missing biome compatibility token {token}')
if 'FrequencyForWavelengthMeters(220.0f' not in d:
    raise RuntimeError('physical outcrop scale missing')
if 'FCubusPhysicalTerrainScaleTest' not in t:
    raise RuntimeError('physical terrain scale test missing')
