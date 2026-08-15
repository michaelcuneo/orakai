from pathlib import Path

# One-shot v30 finalizer. Removed after the guarded migration succeeds.
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

anchor = '''    const float AlpineBand = Result.AlpineInfluence * (1.0f - Result.NivalInfluence);\n\n    if (Settings.Definitions.Num() == 0)\n    {\n'''
replacement = '''    const float AlpineBand = Result.AlpineInfluence * (1.0f - Result.NivalInfluence);\n\n    const auto IsLegacyArchetypeDefinition = [](const FCubusBiomeDefinition& Definition)\n    {\n        return Definition.Name == TEXT("Plains") ||\n            Definition.Name == TEXT("Forest") ||\n            Definition.Name == TEXT("Rocky") ||\n            Definition.Name == TEXT("Wetland");\n    };\n    bool bOnlyLegacyArchetypeDefinitions = !Settings.Definitions.IsEmpty();\n    for (const FCubusBiomeDefinition& Definition : Settings.Definitions)\n    {\n        bOnlyLegacyArchetypeDefinitions &= IsLegacyArchetypeDefinition(Definition);\n    }\n    const bool bUseBuiltInRealCommunities =\n        Settings.Definitions.IsEmpty() || bOnlyLegacyArchetypeDefinitions;\n\n    /* Plains / Forest / Rocky / Wetland are compatibility projections. A Data\n     * Asset containing only those generic entries must not suppress the real\n     * named community classifier. */\n    if (bUseBuiltInRealCommunities)\n    {\n'''
replace_once(biome, anchor, replacement)

# Convert geology fabric frequencies to physical sizes while retaining v29's
# hard displacement cap. These are coherent structures, not extra amplitude.
replace_once(density,
    '''\tconst float RockMass = SampleNoise3D(\n\t\tWorldX - 6211.0f,\n\t\tWorldY + 4177.0f,\n\t\tWorldZ - 1987.0f,\n\t\tFMath::Clamp(Settings.GeologyMassFrequency, 0.006f, 0.18f)\n\t);\n\tconst float RockWarp = SampleNoise3D(\n\t\tWorldX + 1709.0f,\n\t\tWorldY - 3187.0f,\n\t\tWorldZ + 733.0f,\n\t\tFMath::Clamp(Settings.GeologyRockWarpFrequency, 0.008f, 0.22f)\n\t);\n\tconst float FractureVolume = SampleRidgedNoise3D(\n\t\tWorldX + 12011.0f,\n\t\tWorldY - 4919.0f,\n\t\tWorldZ + 2791.0f,\n\t\tFMath::Clamp(Settings.GeologyFractureFrequency, 0.006f, 0.18f)\n\t);\n''',
    '''\tconst float RockMassFrequency = TerrainFormSettings.bUsePhysicalWorldScale\n\t\t? CubusWorldScale::FrequencyForWavelengthMeters(220.0f, Settings.VoxelSizeCm)\n\t\t: FMath::Clamp(Settings.GeologyMassFrequency, 0.006f, 0.18f);\n\tconst float RockWarpFrequency = TerrainFormSettings.bUsePhysicalWorldScale\n\t\t? CubusWorldScale::FrequencyForWavelengthMeters(70.0f, Settings.VoxelSizeCm)\n\t\t: FMath::Clamp(Settings.GeologyRockWarpFrequency, 0.008f, 0.22f);\n\tconst float FractureFrequency = TerrainFormSettings.bUsePhysicalWorldScale\n\t\t? CubusWorldScale::FrequencyForWavelengthMeters(90.0f, Settings.VoxelSizeCm)\n\t\t: FMath::Clamp(Settings.GeologyFractureFrequency, 0.006f, 0.18f);\n\n\tconst float RockMass = SampleNoise3D(\n\t\tWorldX - 6211.0f,\n\t\tWorldY + 4177.0f,\n\t\tWorldZ - 1987.0f,\n\t\tRockMassFrequency\n\t);\n\tconst float RockWarp = SampleNoise3D(\n\t\tWorldX + 1709.0f,\n\t\tWorldY - 3187.0f,\n\t\tWorldZ + 733.0f,\n\t\tRockWarpFrequency\n\t);\n\tconst float FractureVolume = SampleRidgedNoise3D(\n\t\tWorldX + 12011.0f,\n\t\tWorldY - 4919.0f,\n\t\tWorldZ + 2791.0f,\n\t\tFractureFrequency\n\t);\n''')
p = Path(density)
text = p.read_text(encoding='utf-8')
if '#include "CubusCore/Generation/CubusWorldScale.h"' not in text:
    text = text.replace('#include "CubusCore/Generation/CubusTerrainDensityField.h"\n', '#include "CubusCore/Generation/CubusTerrainDensityField.h"\n#include "CubusCore/Generation/CubusWorldScale.h"\n', 1)
p.write_text(text, encoding='utf-8')

# Preserve the old tight shape test on the legacy parameter path, and add a
# separate test that measures the physical world in metres/kilometres.
replace_once(tests,
    '''    FCubusTerrainFormSettings Settings;\n    FCubusTerrainFormSettings NoDetailSettings = Settings;\n''',
    '''    FCubusTerrainFormSettings Settings;\n    Settings.bUsePhysicalWorldScale = false;\n    FCubusTerrainFormSettings NoDetailSettings = Settings;\n''')

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

    TestTrue(TEXT("80 cm voxels convert one kilometre to 1250 canonical voxels"), FMath::IsNearlyEqual(MetersToVoxels(1000.0f), 1250.0f, 0.01f));
    TestTrue(TEXT("Mountain relief is kilometre scale"), Settings.MountainReliefMeters >= 1000.0f);
    TestTrue(TEXT("Major valleys are hundreds of metres deep"), Settings.ValleyReliefMeters >= 250.0f);
    TestTrue(TEXT("Mountain systems have regional spacing"), Settings.MountainSystemSpacingKm >= 10.0f);

    const int32 HalfExtent = FMath::RoundToInt(MetersToVoxels(20000.0f));
    const int32 Step = FMath::Max(1, FMath::RoundToInt(MetersToVoxels(320.0f)));
    float MinimumHeight = MAX_flt;
    float MaximumHeight = -MAX_flt;
    float MaximumDrainage = 0.0f;
    int32 MountainSamples = 0;
    int32 LowlandSamples = 0;

    for (int32 Y = -HalfExtent; Y <= HalfExtent; Y += Step)
    {
        for (int32 X = -HalfExtent; X <= HalfExtent; X += Step)
        {
            const FCubusTerrainFormSample Sample = FCubusTerrainForm::Sample(static_cast<float>(X), static_cast<float>(Y), Settings);
            MinimumHeight = FMath::Min(MinimumHeight, Sample.Height);
            MaximumHeight = FMath::Max(MaximumHeight, Sample.Height);
            MaximumDrainage = FMath::Max(MaximumDrainage, Sample.Drainage);
            MountainSamples += Sample.MountainWeight > 0.60f ? 1 : 0;
            LowlandSamples += Sample.MountainWeight < 0.25f ? 1 : 0;
        }
    }

    const float ReliefMeters = (MaximumHeight - MinimumHeight) * MetersPerVoxel;
    TestTrue(TEXT("Physical world contains mountain country"), MountainSamples > 0);
    TestTrue(TEXT("Physical world contains broad non-mountain country"), LowlandSamples > 0);
    TestTrue(TEXT("Physical world contains major drainage"), MaximumDrainage > 0.55f);
    TestTrue(TEXT("Regional relief reaches mountain scale"), ReliefMeters >= 700.0f);
    TestTrue(TEXT("Regional relief remains physically bounded"), ReliefMeters <= 4200.0f);
    return true;
}
'''
p = Path(tests)
text = p.read_text(encoding='utf-8')
if '\n#endif' not in text:
    raise RuntimeError('test endif anchor missing')
text = text.replace('\n#endif', insert + '\n#endif', 1)
p.write_text(text, encoding='utf-8')

b = Path(biome).read_text(encoding='utf-8')
d = Path(density).read_text(encoding='utf-8')
t = Path(tests).read_text(encoding='utf-8')
if 'bUseBuiltInRealCommunities' not in b:
    raise RuntimeError('legacy biome compatibility fix missing')
for token in ['FrequencyForWavelengthMeters(220.0f', 'FrequencyForWavelengthMeters(70.0f', 'FrequencyForWavelengthMeters(90.0f']:
    if token not in d:
        raise RuntimeError(f'physical geology token missing: {token}')
if 'FCubusPhysicalTerrainScaleTest' not in t:
    raise RuntimeError('physical terrain scale test missing')
