from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected one anchor, found {count}: {old[:100]!r}')
    p.write_text(text.replace(old, new, 1), encoding='utf-8')

terrain_cpp = 'Source/Orakai/CubusCore/Generation/CubusTerrainForm.cpp'
density_h = 'Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h'
density_cpp = 'Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp'
voxel_cpp = 'Source/Orakai/CubusCore/Actors/CubusVoxelVolumeActor.cpp'
biome_cpp = 'Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp'
veg_h = 'Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.h'
veg_cpp = 'Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp'
seeds = 'Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h'

replace_once(terrain_cpp,
    '#include "CubusCore/Generation/CubusTerrainForm.h"\n',
    '#include "CubusCore/Generation/CubusTerrainForm.h"\n#include "CubusCore/Generation/CubusWorldScale.h"\n')

replace_once(terrain_cpp,
    '    const float RegionFrequency = Settings.RegionFrequency;\n',
    '''    const float VoxelSizeCm = FMath::Max(1.0f, Settings.VoxelSizeCm);\n    const bool bPhysicalScale = Settings.bUsePhysicalWorldScale;\n\n    // The physical model is authored in metres. Frequencies below are cycles\n    // per canonical voxel, derived from the actual voxel size exactly once.\n    const float RegionFrequency = bPhysicalScale\n        ? CubusWorldScale::FrequencyForWavelengthKilometres(\n            Settings.MountainSystemSpacingKm * 2.0f, VoxelSizeCm)\n        : Settings.RegionFrequency;\n    const float MountainSystemFrequency = bPhysicalScale\n        ? CubusWorldScale::FrequencyForWavelengthKilometres(Settings.MountainSystemSpacingKm, VoxelSizeCm)\n        : FMath::Max(0.00035f, RegionFrequency * 0.30f);\n    const float MassifFrequency = bPhysicalScale\n        ? CubusWorldScale::FrequencyForWavelengthKilometres(Settings.MassifScaleKm, VoxelSizeCm)\n        : RegionFrequency * 0.62f;\n    const float MajorValleyFrequency = bPhysicalScale\n        ? CubusWorldScale::FrequencyForWavelengthKilometres(Settings.MajorValleySpacingKm, VoxelSizeCm)\n        : Settings.ValleyFrequency * 0.30f;\n    const float TributaryFrequency = bPhysicalScale\n        ? CubusWorldScale::FrequencyForWavelengthKilometres(Settings.TributaryValleyScaleKm, VoxelSizeCm)\n        : Settings.ValleyFrequency * 0.72f;\n    const float HillFrequency = bPhysicalScale\n        ? CubusWorldScale::FrequencyForWavelengthMeters(Settings.RollingHillScaleMeters, VoxelSizeCm)\n        : Settings.HillFrequency * 0.34f;\n    const float BroadReliefFrequency = bPhysicalScale\n        ? CubusWorldScale::FrequencyForWavelengthMeters(Settings.BroadReliefScaleMeters, VoxelSizeCm)\n        : Settings.DetailFrequency * 0.26f;\n    const float LocalReliefFrequency = bPhysicalScale\n        ? CubusWorldScale::FrequencyForWavelengthMeters(Settings.LocalReliefScaleMeters, VoxelSizeCm)\n        : Settings.DetailFrequency * 0.82f;\n\n    const float RegionalReliefVoxels = bPhysicalScale\n        ? CubusWorldScale::MetersToVoxels(Settings.RegionalReliefMeters, VoxelSizeCm)\n        : Settings.ContinentAmplitude;\n    const float MountainReliefVoxels = bPhysicalScale\n        ? CubusWorldScale::MetersToVoxels(Settings.MountainReliefMeters, VoxelSizeCm)\n        : Settings.RidgeAmplitude * Settings.MountainElevationScale;\n    const float ValleyReliefVoxels = bPhysicalScale\n        ? CubusWorldScale::MetersToVoxels(Settings.ValleyReliefMeters, VoxelSizeCm)\n        : Settings.ValleyDepth;\n    const float HillReliefVoxels = bPhysicalScale\n        ? CubusWorldScale::MetersToVoxels(Settings.RollingHillReliefMeters, VoxelSizeCm)\n        : Settings.HillAmplitude;\n    const float BroadReliefVoxels = bPhysicalScale\n        ? CubusWorldScale::MetersToVoxels(Settings.BroadReliefMeters, VoxelSizeCm)\n        : Settings.DetailAmplitude;\n    const float LocalReliefVoxels = bPhysicalScale\n        ? CubusWorldScale::MetersToVoxels(Settings.LocalReliefMeters, VoxelSizeCm)\n        : Settings.DetailAmplitude;\n''')

replace_once(terrain_cpp,
    '''    const float RangeFrequency = FMath::Max(\n        0.00035f,\n        RegionFrequency * 0.30f\n    );\n''',
    '    const float RangeFrequency = MountainSystemFrequency;\n')

replace_once(terrain_cpp,
    '            RegionFrequency * 0.62f,\n',
    '            MassifFrequency,\n')
replace_once(terrain_cpp,
    '        Settings.ValleyFrequency * 0.30f,\n',
    '        MajorValleyFrequency,\n')
replace_once(terrain_cpp,
    '        Settings.ValleyFrequency * 0.72f,\n',
    '        TributaryFrequency,\n')
replace_once(terrain_cpp,
    '        Settings.HillFrequency * 0.34f,\n',
    '        HillFrequency,\n')
replace_once(terrain_cpp,
    '        Settings.DetailFrequency * 0.26f,\n',
    '        BroadReliefFrequency,\n')
replace_once(terrain_cpp,
    '        Settings.DetailFrequency * 0.82f\n',
    '        LocalReliefFrequency\n')

replace_once(terrain_cpp,
    '''    const float ProvinceUplift =\n        (ProvinceElevation - 0.5f) * 2.0f *\n        Settings.ContinentAmplitude * 0.54f;\n''',
    '''    const float ProvinceUplift =\n        (ProvinceElevation - 0.5f) * 2.0f *\n        RegionalReliefVoxels * 0.54f;\n''')
replace_once(terrain_cpp,
    '        MacroRelief * Settings.ContinentAmplitude * ContinentStrength +\n',
    '        MacroRelief * RegionalReliefVoxels * 0.34f * ContinentStrength +\n')
replace_once(terrain_cpp,
    '        RollingRelief * Settings.HillAmplitude * HillStrength * FloodplainCalm +\n',
    '        RollingRelief * HillReliefVoxels * HillStrength * FloodplainCalm +\n')

replace_once(terrain_cpp,
    '''    const float RangeUplift =\n        Settings.RidgeAmplitude *\n        Settings.MountainElevationScale *\n        (\n            RangeShoulder * 0.34f +\n            RangeCrest * 1.16f +\n            Result.MassifWeight * 0.42f\n        ) *\n        RangeRhythm;\n''',
    '''    const float RangeUplift =\n        MountainReliefVoxels *\n        FMath::Clamp(\n            RangeShoulder * 0.24f +\n            RangeCrest * 0.68f +\n            Result.MassifWeight * 0.22f,\n            0.0f,\n            1.0f\n        ) *\n        RangeRhythm;\n''')

replace_once(terrain_cpp,
    '        Settings.ValleyDepth * ValleyStrength *\n',
    '        ValleyReliefVoxels * ValleyStrength *\n')
replace_once(terrain_cpp,
    '        Settings.ValleyDepth * TributaryValley *\n',
    '        ValleyReliefVoxels * TributaryValley *\n')
replace_once(terrain_cpp,
    '        Settings.ValleyDepth * 0.16f * LowlandProvince;\n',
    '        ValleyReliefVoxels * 0.16f * LowlandProvince;\n')
replace_once(terrain_cpp,
    '        Settings.ValleyDepth * 0.10f * Result.Cirque;\n',
    '        ValleyReliefVoxels * 0.10f * Result.Cirque;\n')

replace_once(terrain_cpp,
    '''    const float LocalDetail =\n        Settings.DetailAmplitude *\n        Result.SurfaceRoughness *\n        (\n            BroadDetail * 0.34f +\n            FineDetail * 0.14f\n        );\n    const float RillCut =\n        Result.ErosionRills *\n        Settings.DetailAmplitude * 0.12f;\n''',
    '''    const float LocalDetail =\n        Result.SurfaceRoughness *\n        (\n            BroadDetail * BroadReliefVoxels * 0.34f +\n            FineDetail * LocalReliefVoxels * 0.14f\n        );\n    const float RillCut =\n        Result.ErosionRills * LocalReliefVoxels * 0.12f;\n''')

# Runtime density carries the actual canonical voxel size into terrain-form conversion.
replace_once(density_h,
    '    float FlatSurfaceWorldZ = 8.0f;\n    float BaseHeight = 8.0f;\n',
    '    float FlatSurfaceWorldZ = 8.0f;\n    float BaseHeight = 8.0f;\n    float VoxelSizeCm = 80.0f;\n')
replace_once(density_cpp,
    '    TerrainFormSettings.BaseHeight = Settings.BaseHeight;\n',
    '    TerrainFormSettings.BaseHeight = Settings.BaseHeight;\n\tTerrainFormSettings.VoxelSizeCm = Settings.VoxelSizeCm;\n')
replace_once(voxel_cpp,
    '    DensitySettings.BaseHeight = static_cast<float>(TerrainBaseHeight);\n',
    '    DensitySettings.BaseHeight = static_cast<float>(TerrainBaseHeight);\n\n\tDensitySettings.VoxelSizeCm = VoxelSize;\n')

# Climate geography gets a physical 80 km province wavelength. The existing
# SampleClimate hierarchy derives regional/local climates from this root.
replace_once(biome_cpp,
    '#include "CubusCore/Generation/CubusGenerationSeeds.h"\n',
    '#include "CubusCore/Generation/CubusGenerationSeeds.h"\n#include "CubusCore/Generation/CubusWorldScale.h"\n')
replace_once(biome_cpp,
    '''    const FCubusTerrainFormSettings& Terrain = HydrologySettings.TerrainFormSettings;\n    const float StructuralNivalWorldZ = Terrain.BaseHeight + FMath::Max(64.0f, Terrain.RidgeAmplitude * 4.0f);\n''',
    '''    const FCubusTerrainFormSettings& Terrain = HydrologySettings.TerrainFormSettings;\n    if (Terrain.bUsePhysicalWorldScale)\n    {\n        // SampleClimate multiplies Frequency by 0.18 for its province field.\n        // Resolve the root so that field has the requested real-world wavelength.\n        Result.Frequency = CubusWorldScale::FrequencyForWavelengthKilometres(\n            Terrain.ClimateProvinceScaleKm,\n            Terrain.VoxelSizeCm\n        ) / 0.18f;\n    }\n    const float StructuralNivalWorldZ = Terrain.BaseHeight + (Terrain.bUsePhysicalWorldScale\n        ? CubusWorldScale::MetersToVoxels(Terrain.MountainReliefMeters * 0.78f, Terrain.VoxelSizeCm)\n        : FMath::Max(64.0f, Terrain.RidgeAmplitude * 4.0f));\n''')

# RockyTreeDensity was exposed in the Data Asset but never propagated.
replace_once(veg_h,
    '    float RockyAlpineDensity = 0.0f;\n',
    '    float RockyTreeDensity = 0.0f;\n    float RockyAlpineDensity = 0.0f;\n')
replace_once(veg_cpp,
    '        const float RockyConiferBase = Settings.ForestTreeDensity * BiomeSample.RockyWeight * 0.22f;\n',
    '        const float RockyConiferBase = Settings.RockyTreeDensity * BiomeSample.RockyWeight;\n')
replace_once(veg_cpp,
    '    Settings.RockyAlpineDensity = GeologyProfile->RockyAlpineDensity;\n',
    '    Settings.RockyTreeDensity = GeologyProfile->RockyTreeDensity;\n    Settings.RockyAlpineDensity = GeologyProfile->RockyAlpineDensity;\n')

replace_once(seeds,
    '''    // Bumped to 29: terrain generation is split into smooth macro form,\n    // bounded continuous sub-voxel surface relief, and bounded volumetric rock\n    // deformation. Adaptive refinement now preserves interior fine crossings.\n    static constexpr uint32 CurrentGenerationVersion = 29;\n''',
    '''    // Bumped to 30: terrain and biome geography are authored in real-world\n    // metric scale and converted through the actual canonical voxel size.\n    static constexpr uint32 CurrentGenerationVersion = 30;\n''')

# Sanity guards.
t = Path(terrain_cpp).read_text(encoding='utf-8')
for token in ['MountainSystemSpacingKm', 'MajorValleySpacingKm', 'MountainReliefVoxels', 'ValleyReliefVoxels']:
    if token not in t:
        raise RuntimeError(f'missing physical terrain token {token}')
if 'Settings.ForestTreeDensity * BiomeSample.RockyWeight * 0.22f' in Path(veg_cpp).read_text(encoding='utf-8'):
    raise RuntimeError('legacy hard-coded rocky tree density remains')
if 'CurrentGenerationVersion = 30' not in Path(seeds).read_text(encoding='utf-8'):
    raise RuntimeError('generation v30 missing')
