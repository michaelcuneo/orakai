from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'expected one anchor in {path}, found {count}: {old[:140]!r}')
    p.write_text(text.replace(old, new, 1), encoding='utf-8')


def require(path, needle, count=None):
    text = Path(path).read_text(encoding='utf-8')
    actual = text.count(needle)
    if actual == 0:
        raise RuntimeError(f'missing required text in {path}: {needle!r}')
    if count is not None and actual != count:
        raise RuntimeError(f'expected {count} copies in {path}, found {actual}: {needle!r}')


# -----------------------------------------------------------------------------
# Runtime biome sample carries an explicit visible phenotype. This is separate
# from ecological state: it is the compact bridge from communities to assets.
# -----------------------------------------------------------------------------
replace_once(
    'Source/Orakai/CubusCore/Generation/CubusBiomeField.h',
    '''    float CanopyPotential = 0.0f;
    float CanopyOpenness = 1.0f;

    float SurfaceWorldZ = 0.0f;
''',
    '''    float CanopyPotential = 0.0f;
    float CanopyOpenness = 1.0f;

    // Visible community phenotype. These deliberately drive large changes in
    // vegetation structure so distinct ecological communities look distinct.
    float VisualTreeCover = 0.0f;
    float VisualConiferPreference = 0.5f;
    float VisualShrubCover = 0.0f;
    float VisualHerbCover = 0.0f;
    float VisualReedCover = 0.0f;
    float VisualAlpineCover = 0.0f;

    float SurfaceWorldZ = 0.0f;
'''
)

# -----------------------------------------------------------------------------
# Built-in communities get explicit visual profiles. Authored definitions fall
# back to their broad archetype, so existing assets remain compatible.
# -----------------------------------------------------------------------------
biome_cpp = 'Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp'
replace_once(
    biome_cpp,
    '''    ECubusClimateProvince ResolveClimateProvince(const FCubusBiomeClimateContext& Climate)
    {
''',
    '''    struct FCommunityVisualProfile
    {
        float TreeCover = 0.0f;
        float ConiferPreference = 0.5f;
        float ShrubCover = 0.0f;
        float HerbCover = 0.0f;
        float ReedCover = 0.0f;
        float AlpineCover = 0.0f;
    };

    FCommunityVisualProfile ResolveCommunityVisualProfile(const FName Name, const ECubusBiomeKind Archetype)
    {
        FCommunityVisualProfile Result;
        switch (Archetype)
        {
            case ECubusBiomeKind::Forest:
                Result = {0.70f, 0.42f, 0.22f, 0.24f, 0.02f, 0.02f};
                break;
            case ECubusBiomeKind::Rocky:
                Result = {0.03f, 0.72f, 0.24f, 0.14f, 0.00f, 0.42f};
                break;
            case ECubusBiomeKind::Wetland:
                Result = {0.10f, 0.18f, 0.12f, 0.48f, 0.82f, 0.00f};
                break;
            case ECubusBiomeKind::Plains:
            default:
                Result = {0.04f, 0.28f, 0.20f, 0.82f, 0.02f, 0.02f};
                break;
        }

        if (Name == TEXT("RiparianForest"))          return {0.72f, 0.16f, 0.18f, 0.32f, 0.34f, 0.00f};
        if (Name == TEXT("TemperateForest"))         return {0.94f, 0.12f, 0.16f, 0.12f, 0.00f, 0.00f};
        if (Name == TEXT("MontaneForest"))           return {0.88f, 0.66f, 0.18f, 0.18f, 0.00f, 0.08f};
        if (Name == TEXT("WindwardMontaneForest"))   return {1.00f, 0.74f, 0.16f, 0.14f, 0.02f, 0.08f};
        if (Name == TEXT("ShadedRavineForest"))      return {0.96f, 0.28f, 0.24f, 0.16f, 0.18f, 0.00f};
        if (Name == TEXT("CoolValleyForest"))        return {0.90f, 0.38f, 0.22f, 0.20f, 0.12f, 0.00f};
        if (Name == TEXT("DeepColluvialForest"))     return {0.94f, 0.24f, 0.20f, 0.14f, 0.02f, 0.00f};
        if (Name == TEXT("SubalpineWoodland"))       return {0.54f, 0.92f, 0.40f, 0.34f, 0.00f, 0.26f};
        if (Name == TEXT("OpenWoodland"))            return {0.34f, 0.30f, 0.48f, 0.64f, 0.00f, 0.00f};
        if (Name == TEXT("DryRockWoodland"))         return {0.24f, 0.48f, 0.68f, 0.46f, 0.00f, 0.06f};
        if (Name == TEXT("WindPrunedSubalpineScrub"))return {0.06f, 0.92f, 0.92f, 0.20f, 0.00f, 0.62f};
        if (Name == TEXT("FracturedRockHeath"))      return {0.01f, 0.78f, 0.84f, 0.28f, 0.00f, 0.54f};
        if (Name == TEXT("LeewardSteppe"))           return {0.01f, 0.36f, 0.42f, 1.00f, 0.00f, 0.00f};
        if (Name == TEXT("Meadow"))                  return {0.02f, 0.24f, 0.10f, 1.00f, 0.00f, 0.00f};
        if (Name == TEXT("DryGrassland"))            return {0.01f, 0.30f, 0.32f, 0.92f, 0.00f, 0.00f};
        if (Name == TEXT("WetMeadow"))               return {0.01f, 0.18f, 0.10f, 0.94f, 0.36f, 0.00f};
        if (Name == TEXT("FloodplainMeadow"))        return {0.01f, 0.18f, 0.08f, 0.96f, 0.48f, 0.00f};
        if (Name == TEXT("AvalancheMeadow"))         return {0.00f, 0.62f, 0.20f, 0.88f, 0.00f, 0.42f};
        if (Name == TEXT("Marsh"))                   return {0.00f, 0.10f, 0.05f, 0.30f, 1.00f, 0.00f};
        if (Name == TEXT("RiparianWetland"))         return {0.06f, 0.16f, 0.10f, 0.44f, 0.96f, 0.00f};
        if (Name == TEXT("AlpineMeadow"))            return {0.00f, 0.80f, 0.12f, 0.64f, 0.00f, 1.00f};
        if (Name == TEXT("AlpineHeath"))             return {0.00f, 0.82f, 0.62f, 0.18f, 0.00f, 0.92f};
        if (Name == TEXT("Scree"))                   return {0.00f, 0.86f, 0.05f, 0.04f, 0.00f, 0.28f};
        if (Name == TEXT("ExposedRock"))             return {0.00f, 0.82f, 0.02f, 0.01f, 0.00f, 0.08f};
        if (Name == TEXT("NivalRock"))               return {0.00f, 0.88f, 0.00f, 0.00f, 0.00f, 0.00f};
        return Result;
    }

    ECubusClimateProvince ResolveClimateProvince(const FCubusBiomeClimateContext& Climate)
    {
'''
)

replace_once(
    biome_cpp,
    '''    const FCubusBiomeCommunityBlend& DominantCommunity = Result.CommunityBlend[0];
    Result.BiomeDefinitionIndex = DominantCommunity.DefinitionIndex;
''',
    '''    Result.VisualTreeCover = 0.0f;
    Result.VisualConiferPreference = 0.0f;
    Result.VisualShrubCover = 0.0f;
    Result.VisualHerbCover = 0.0f;
    Result.VisualReedCover = 0.0f;
    Result.VisualAlpineCover = 0.0f;
    for (int32 Index = 0; Index < Result.CommunityBlendCount; ++Index)
    {
        const FCubusBiomeCommunityBlend& Community = Result.CommunityBlend[Index];
        const FCommunityVisualProfile Visual = ResolveCommunityVisualProfile(Community.Name, Community.Archetype);
        Result.VisualTreeCover += Visual.TreeCover * Community.Weight;
        Result.VisualConiferPreference += Visual.ConiferPreference * Community.Weight;
        Result.VisualShrubCover += Visual.ShrubCover * Community.Weight;
        Result.VisualHerbCover += Visual.HerbCover * Community.Weight;
        Result.VisualReedCover += Visual.ReedCover * Community.Weight;
        Result.VisualAlpineCover += Visual.AlpineCover * Community.Weight;
    }
    Result.VisualTreeCover = FMath::Clamp(Result.VisualTreeCover, 0.0f, 1.0f);
    Result.VisualConiferPreference = FMath::Clamp(Result.VisualConiferPreference, 0.0f, 1.0f);
    Result.VisualShrubCover = FMath::Clamp(Result.VisualShrubCover, 0.0f, 1.0f);
    Result.VisualHerbCover = FMath::Clamp(Result.VisualHerbCover, 0.0f, 1.0f);
    Result.VisualReedCover = FMath::Clamp(Result.VisualReedCover, 0.0f, 1.0f);
    Result.VisualAlpineCover = FMath::Clamp(Result.VisualAlpineCover, 0.0f, 1.0f);

    const FCubusBiomeCommunityBlend& DominantCommunity = Result.CommunityBlend[0];
    Result.BiomeDefinitionIndex = DominantCommunity.DefinitionIndex;
'''
)

# -----------------------------------------------------------------------------
# Vegetation now consumes the explicit visual phenotype strongly. Ecological
# habitat still gates impossible placement, but community identity now matters.
# -----------------------------------------------------------------------------
veg_cpp = 'Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp'
replace_once(
    veg_cpp,
    '''        const float EcologyStrength = DominantEcologyStrength(BiomeSample);

        const float ForestTreeBase = Settings.ForestTreeDensity * BiomeSample.ForestWeight *
''',
    '''        const float EcologyStrength = DominantEcologyStrength(BiomeSample);
        const float VisualTreeCover = FMath::Clamp(BiomeSample.VisualTreeCover, 0.0f, 1.0f);
        const float VisualConiferPreference = FMath::Clamp(BiomeSample.VisualConiferPreference, 0.0f, 1.0f);
        const float VisualShrubCover = FMath::Clamp(BiomeSample.VisualShrubCover, 0.0f, 1.0f);
        const float VisualHerbCover = FMath::Clamp(BiomeSample.VisualHerbCover, 0.0f, 1.0f);
        const float VisualReedCover = FMath::Clamp(BiomeSample.VisualReedCover, 0.0f, 1.0f);
        const float VisualAlpineCover = FMath::Clamp(BiomeSample.VisualAlpineCover, 0.0f, 1.0f);

        const float ForestTreeBase = Settings.ForestTreeDensity * BiomeSample.ForestWeight *
'''
)

replace_once(
    veg_cpp,
    '''        const float TreeBase = ForestTreeBase + WetlandTreeBase + PlainsTreeBase + RockyConiferBase;
        const float ArchetypeBroadleafFraction = FMath::Clamp(
''',
    '''        const float LegacyTreeBase = ForestTreeBase + WetlandTreeBase + PlainsTreeBase + RockyConiferBase;
        const float PhenotypeTreeBase = Settings.ForestTreeDensity * FMath::Lerp(0.04f, 1.85f, VisualTreeCover);
        const float TreeBase = FMath::Lerp(LegacyTreeBase, PhenotypeTreeBase, 0.78f);
        const float ArchetypeBroadleafFraction = FMath::Clamp(
'''
)

replace_once(
    veg_cpp,
    '''        Result.BroadleafScore = ArchetypeBroadleafFraction * BroadleafHabitat;
        Result.ConiferScore = (1.0f - ArchetypeBroadleafFraction) * ConiferHabitat +
            BiomeSample.RockyWeight * ConiferHabitat * 0.28f;
''',
    '''        const float BroadleafFraction = FMath::Clamp(
            FMath::Lerp(ArchetypeBroadleafFraction, 1.0f - VisualConiferPreference, 0.82f),
            0.02f,
            0.98f
        );
        Result.BroadleafScore = BroadleafFraction * BroadleafHabitat;
        Result.ConiferScore = (1.0f - BroadleafFraction) * ConiferHabitat +
            BiomeSample.RockyWeight * ConiferHabitat * 0.18f;
'''
)

replace_once(
    veg_cpp,
    '''        Result.ReedDensity = FMath::Clamp(
            Settings.WetlandReedDensity * (0.35f + BiomeSample.WetlandWeight) * ReedHabitat,
            0.0f,
            1.0f
        );
''',
    '''        Result.ReedDensity = FMath::Clamp(
            Settings.WetlandReedDensity *
            FMath::Lerp(0.08f, 1.90f, VisualReedCover) * ReedHabitat,
            0.0f,
            1.0f
        );
'''
)

replace_once(
    veg_cpp,
    '''        Result.AlpineDensity = FMath::Clamp(
            Settings.RockyAlpineDensity * (0.25f + BiomeSample.RockyWeight) * AlpineHabitat,
            0.0f,
            1.0f
        );

        const float GroundBase = Settings.PlainsGroundCoverDensity *
            (BiomeSample.PlainsWeight + BiomeSample.ForestWeight * 0.35f + BiomeSample.RockyWeight * 0.16f);
''',
    '''        Result.AlpineDensity = FMath::Clamp(
            Settings.RockyAlpineDensity *
            FMath::Lerp(0.08f, 2.10f, VisualAlpineCover) * AlpineHabitat,
            0.0f,
            1.0f
        );

        const float LegacyGroundBase = Settings.PlainsGroundCoverDensity *
            (BiomeSample.PlainsWeight + BiomeSample.ForestWeight * 0.35f + BiomeSample.RockyWeight * 0.16f);
'''
)

replace_once(
    veg_cpp,
    '''        const float ShrubFraction = FMath::Clamp(Settings.PlainsShrubFraction, 0.0f, 1.0f);
        Result.ShrubDensity = FMath::Clamp(GroundBase * FMath::Lerp(0.24f, 0.78f, ShrubFraction) * ShrubHabitat, 0.0f, 1.0f);
        Result.GrassDensity = FMath::Clamp(GroundBase * FMath::Lerp(1.0f, 0.52f, ShrubFraction) * GrassHabitat, 0.0f, 1.0f);
''',
    '''        const float ShrubFraction = FMath::Clamp(Settings.PlainsShrubFraction, 0.0f, 1.0f);
        const float PhenotypeShrubBase = Settings.PlainsGroundCoverDensity * FMath::Lerp(0.04f, 1.75f, VisualShrubCover);
        const float PhenotypeHerbBase = Settings.PlainsGroundCoverDensity * FMath::Lerp(0.04f, 1.85f, VisualHerbCover);
        const float ShrubBase = FMath::Lerp(LegacyGroundBase * FMath::Lerp(0.24f, 0.78f, ShrubFraction), PhenotypeShrubBase, 0.82f);
        const float HerbBase = FMath::Lerp(LegacyGroundBase * FMath::Lerp(1.0f, 0.52f, ShrubFraction), PhenotypeHerbBase, 0.82f);
        Result.ShrubDensity = FMath::Clamp(ShrubBase * ShrubHabitat, 0.0f, 1.0f);
        Result.GrassDensity = FMath::Clamp(HerbBase * GrassHabitat, 0.0f, 1.0f);
'''
)

# Make per-instance stature reflect the dominant community structure too.
replace_once(
    veg_cpp,
    '''            Instance.Scale = FMath::Lerp(0.85f, 1.15f, HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 307)));
            Instance.TypeId = Selection.TypeId;
''',
    '''            Instance.Scale = FMath::Lerp(0.85f, 1.15f, HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 307)));
            if (IsTreeType(Selection.TypeId))
            {
                Instance.Scale *= FMath::Lerp(0.86f, 1.18f, BiomeSample.VisualTreeCover);
            }
            else if (Selection.TypeId == CubusVegetationType::Shrub)
            {
                Instance.Scale *= FMath::Lerp(0.84f, 1.16f, BiomeSample.VisualShrubCover);
            }
            Instance.TypeId = Selection.TypeId;
'''
)

# Same phenotype scale for regional/far tree generation. There are two more
# identical tree-instance blocks after the main chunk loop.
p = Path(veg_cpp)
text = p.read_text(encoding='utf-8')
old = '''            Instance.Scale = FMath::Lerp(0.85f, 1.15f, HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 307)));
            Instance.TypeId = Selection.TypeId;
'''
new = '''            Instance.Scale = FMath::Lerp(0.85f, 1.15f, HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 307)));
            Instance.Scale *= FMath::Lerp(0.86f, 1.18f, BiomeSample.VisualTreeCover);
            Instance.TypeId = Selection.TypeId;
'''
remaining = text.count(old)
if remaining < 1:
    raise RuntimeError(f'expected regional tree instance blocks, found {remaining}')
text = text.replace(old, new)
p.write_text(text, encoding='utf-8')

# Startup diagnostic: one process-level line that proves whether the active
# profile is feeding configured biome generation or the generic fallback.
replace_once(
    veg_cpp,
    '''    Settings.bUseConfiguredBiomes = IsValid(GeologyProfile) && GeologyProfile->bGenerateBiomes;
    Settings.BiomeSettings = FCubusBiomeField::MakeSettings(GeologyProfile, GenerationSeeds.Biomes, GenerationSeeds.Rivers);
''',
    '''    Settings.bUseConfiguredBiomes = IsValid(GeologyProfile) && GeologyProfile->bGenerateBiomes;
    Settings.BiomeSettings = FCubusBiomeField::MakeSettings(GeologyProfile, GenerationSeeds.Biomes, GenerationSeeds.Rivers);

    static TAtomic<bool> bLoggedBiomeRuntime(false);
    bool bExpected = false;
    if (bLoggedBiomeRuntime.CompareExchange(bExpected, true))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus biome runtime: profile=%s enabled=%s definitions=%d generationVersion=%u"),
            *GetPathNameSafe(GeologyProfile),
            Settings.bUseConfiguredBiomes ? TEXT("YES") : TEXT("NO - VEGETATION FALLBACK ACTIVE"),
            Settings.BiomeSettings.Definitions.Num(),
            FCubusGenerationSeeds::CurrentGenerationVersion
        );
    }
'''
)

# -----------------------------------------------------------------------------
# Generation identity changes because visible vegetation distribution changes.
# -----------------------------------------------------------------------------
replace_once(
    'Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h',
    '''    // Bumped to 18: climate provinces and cached long-range terrain horizons now
    // drive windward moisture, rain shadow and solar occlusion; slow climate
    // interpolation also moves to an 8-voxel lattice for cheaper cold generation.
    static constexpr uint32 CurrentGenerationVersion = 18;
''',
    '''    // Bumped to 19: ecological communities now emit an explicit visible phenotype
    // that strongly drives tree composition and ground-cover structure.
    static constexpr uint32 CurrentGenerationVersion = 19;
'''
)

# -----------------------------------------------------------------------------
# Regression tests prove two communities with the same broad archetype can now
# produce materially different visible phenotypes.
# -----------------------------------------------------------------------------
test_path = Path('Source/Orakai/CubusCore/Tests/CubusBiomeFieldTests.cpp')
text = test_path.read_text(encoding='utf-8')
marker = 'IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n    FCubusBiomeEcologicalNicheTest,'
if text.count(marker) != 1:
    raise RuntimeError('missing biome test insertion marker')
new_test = r'''IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBiomeVisualPhenotypeTest,
    "Orakai.Cubus.Generation.BiomeVisualPhenotype",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusBiomeVisualPhenotypeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusBiomeFieldSettings Settings;
    Settings.bEnabled = true;

    bool bFoundDenseForest = false;
    bool bFoundOpenCommunity = false;
    float MaximumTreeCover = 0.0f;
    float MinimumTreeCover = 1.0f;
    float MaximumHerbCover = 0.0f;

    for (int32 Y = -640; Y <= 640; Y += 32)
    {
        for (int32 X = -640; X <= 640; X += 32)
        {
            const FCubusBiomeSample Sample = FCubusBiomeField::Sample(
                static_cast<float>(X), static_cast<float>(Y), 24.0f, 0.24f, Settings
            );
            MaximumTreeCover = FMath::Max(MaximumTreeCover, Sample.VisualTreeCover);
            MinimumTreeCover = FMath::Min(MinimumTreeCover, Sample.VisualTreeCover);
            MaximumHerbCover = FMath::Max(MaximumHerbCover, Sample.VisualHerbCover);
            bFoundDenseForest |= Sample.VisualTreeCover >= 0.60f;
            bFoundOpenCommunity |= Sample.VisualTreeCover <= 0.20f && Sample.VisualHerbCover >= 0.45f;
        }
    }

    TestTrue(TEXT("Biome phenotype produces dense forest structure"), bFoundDenseForest);
    TestTrue(TEXT("Biome phenotype produces visibly open communities"), bFoundOpenCommunity);
    TestTrue(TEXT("Tree-cover phenotype spans a meaningful range"), MaximumTreeCover - MinimumTreeCover >= 0.35f);
    TestTrue(TEXT("Herbaceous communities can dominate ground cover"), MaximumHerbCover >= 0.60f);
    return true;
}

'''
test_path.write_text(text.replace(marker, new_test + marker, 1), encoding='utf-8')

require('Source/Orakai/CubusCore/Generation/CubusBiomeField.h', 'VisualTreeCover', 1)
require(biome_cpp, 'ResolveCommunityVisualProfile', 2)
require(biome_cpp, 'TEXT("LeewardSteppe")', 2)
require(veg_cpp, 'VisualConiferPreference', 2)
require(veg_cpp, 'Cubus biome runtime:', 1)
require('Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h', 'CurrentGenerationVersion = 19', 1)
require('Source/Orakai/CubusCore/Tests/CubusBiomeFieldTests.cpp', 'FCubusBiomeVisualPhenotypeTest', 2)
