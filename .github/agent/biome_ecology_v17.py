from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one anchor in {path}, found {count}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


def require(path: str, needle: str, count: int | None = None) -> None:
    text = Path(path).read_text(encoding="utf-8")
    actual = text.count(needle)
    if actual == 0:
        raise RuntimeError(f"missing required text in {path}: {needle!r}")
    if count is not None and actual != count:
        raise RuntimeError(f"expected {count} copies in {path}, found {actual}: {needle!r}")


# -----------------------------------------------------------------------------
# Biome authoring and runtime data.
# -----------------------------------------------------------------------------
replace_once(
    "Source/Orakai/CubusCore/Data/CubusGeologyProfile.h",
    '''    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes", meta = (ClampMin = "0.000001", UIMin = "0.0001", UIMax = "0.05"))
    float BiomeFrequency = 0.004f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
''',
    '''    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes", meta = (ClampMin = "0.000001", UIMin = "0.0001", UIMax = "0.05"))
    float BiomeFrequency = 0.004f;

    /** Long-term moisture-bearing wind direction in terrain XY space. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes|Climate")
    FVector2D BiomePrevailingWindDirection = FVector2D(0.82f, 0.57f);

    /** Representative warm/sun-facing downslope direction used by the ecology model. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes|Climate")
    FVector2D BiomeSolarDirection = FVector2D(-0.42f, -0.91f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
'''
)

replace_once(
    "Source/Orakai/CubusCore/Data/CubusBiomeTypes.h",
    '''    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumOrganicMatter = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
''',
    '''    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumOrganicMatter = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Substrate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSubstrateHardness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Substrate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSubstrateHardness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Substrate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumFractureDensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Substrate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumFractureDensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSoilCoarseness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSoilCoarseness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumWaterHoldingCapacity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumWaterHoldingCapacity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.h",
    '''    float RockyMinimumWorldZ     = 48.0f; // Legacy asset compatibility only.

    int32 PlainsSurfaceMaterialId  = 1;
''',
    '''    float RockyMinimumWorldZ     = 48.0f; // Legacy asset compatibility only.

    FVector2D PrevailingWindDirection = FVector2D(0.82f, 0.57f);
    FVector2D SolarDirection = FVector2D(-0.42f, -0.91f);

    int32 PlainsSurfaceMaterialId  = 1;
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.h",
    '''    bool bHasHydrologySample = false;
    FCubusHydrologySample HydrologySample;
};
''',
    '''    bool bHasHydrologySample = false;
    FCubusHydrologySample HydrologySample;

    bool bHasSubstrateSample = false;
    float SubstrateHardness = 0.5f;
    float FractureDensity = 0.0f;
};
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.h",
    '''    float SoilMoisture = 0.5f;
    float SoilPermeability = 0.5f;
    float OrganicMatter = 0.5f;
''',
    '''    float SoilMoisture = 0.5f;
    float SoilPermeability = 0.5f;
    float SoilCoarseness = 0.5f;
    float WaterHoldingCapacity = 0.5f;
    float SubstrateHardness = 0.5f;
    float FractureDensity = 0.0f;
    float OrganicMatter = 0.5f;
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.h",
    '''    static FCubusBiomeSample Sample(
        float WorldX,
        float WorldY,
        float SurfaceWorldZ,
        float Slope,
        const FCubusBiomeFieldSettings& Settings,
        const FCubusBiomeTerrainContext& TerrainContext = FCubusBiomeTerrainContext()
    );
''',
    '''    static FCubusBiomeClimateContext SampleClimate(
        float WorldX,
        float WorldY,
        const FCubusBiomeFieldSettings& Settings
    );

    static FCubusBiomeClimateContext LerpClimate(
        const FCubusBiomeClimateContext& A,
        const FCubusBiomeClimateContext& B,
        float Alpha
    );

    static FCubusBiomeSample Sample(
        float WorldX,
        float WorldY,
        float SurfaceWorldZ,
        float Slope,
        const FCubusBiomeFieldSettings& Settings,
        const FCubusBiomeTerrainContext& TerrainContext = FCubusBiomeTerrainContext(),
        const FCubusBiomeClimateContext* ClimateContext = nullptr
    );
'''
)

# -----------------------------------------------------------------------------
# Biome implementation: configurable climate, prepared climate backbone,
# authoritative terrain/hydrology reuse, and geology-driven soils.
# -----------------------------------------------------------------------------
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''    Settings.bEnabled = GeologyProfile->bGenerateBiomes;
    Settings.Frequency = GeologyProfile->BiomeFrequency;
    Settings.ForestThreshold = FMath::IsNearlyEqual(GeologyProfile->ForestThreshold, 0.15f, 0.0001f)
''',
    '''    Settings.bEnabled = GeologyProfile->bGenerateBiomes;
    Settings.Frequency = GeologyProfile->BiomeFrequency;
    Settings.PrevailingWindDirection = GeologyProfile->BiomePrevailingWindDirection;
    Settings.SolarDirection = GeologyProfile->BiomeSolarDirection;
    Settings.ForestThreshold = FMath::IsNearlyEqual(GeologyProfile->ForestThreshold, 0.15f, 0.0001f)
'''
)

biome_cpp = Path("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp")
text = biome_cpp.read_text(encoding="utf-8")
marker = "FCubusBiomeSample FCubusBiomeField::Sample(\n"
if text.count(marker) != 1:
    raise RuntimeError("could not find unique biome Sample insertion point")
climate_methods = r'''FCubusBiomeClimateContext FCubusBiomeField::SampleClimate(
    const float WorldX,
    const float WorldY,
    const FCubusBiomeFieldSettings& InSettings
)
{
    FCubusBiomeFieldSettings Settings = InSettings;
    Settings.Frequency = FMath::Max(0.000001f, Settings.Frequency);

    const float BiomeX = WorldX + static_cast<float>(Settings.BiomeOffsetX);
    const float BiomeY = WorldY + static_cast<float>(Settings.BiomeOffsetY);
    const float ClimateProvinceFrequency = Settings.Frequency * 0.18f;
    const float ClimateRegionalFrequency = Settings.Frequency * 0.62f;
    const float ClimateLocalFrequency = Settings.Frequency * 1.55f;
    const float WarpFrequency = ClimateProvinceFrequency * 0.55f;
    const float WarpAmplitude = FMath::Clamp(0.18f / ClimateProvinceFrequency, 96.0f, 520.0f);
    const float WarpedX = BiomeX + SampleFbm(
        BiomeX + 3527.0f,
        BiomeY - 1871.0f,
        WarpFrequency,
        3,
        0.5f
    ) * WarpAmplitude;
    const float WarpedY = BiomeY + SampleFbm(
        BiomeX - 6173.0f,
        BiomeY + 2593.0f,
        WarpFrequency,
        3,
        0.5f
    ) * WarpAmplitude;

    FCubusBiomeClimateContext Result;
    Result.ProvinceMoisture = SampleFbm(WarpedX + 4219.0f, WarpedY - 1877.0f, ClimateProvinceFrequency * 0.95f, 4, 0.52f);
    Result.RegionalMoisture = SampleFbm(WarpedX - 11549.0f, WarpedY + 9011.0f, ClimateRegionalFrequency * 0.90f, 3, 0.50f);
    Result.LocalHumidity = SampleFbm(WarpedX + 1013.0f, WarpedY + 6427.0f, ClimateLocalFrequency, 2, 0.45f);
    Result.ProvinceTemperature = SampleFbm(WarpedX - 8111.0f, WarpedY + 3203.0f, ClimateProvinceFrequency * 0.78f, 4, 0.52f);
    Result.RegionalTemperature = SampleFbm(WarpedX + 14831.0f, WarpedY - 10427.0f, ClimateRegionalFrequency * 0.82f, 3, 0.50f);
    Result.LocalTemperature = SampleFbm(WarpedX - 2791.0f, WarpedY - 15317.0f, ClimateLocalFrequency * 0.72f, 2, 0.45f);
    Result.CommunityPatch[0] = FMath::Clamp(0.5f + SampleFbm(WarpedX + 17011.0f, WarpedY - 9017.0f, Settings.Frequency * 0.95f, 2, 0.48f) * 0.5f, 0.0f, 1.0f);
    Result.CommunityPatch[1] = FMath::Clamp(0.5f + SampleFbm(WarpedX - 2477.0f, WarpedY + 19421.0f, Settings.Frequency * 1.25f, 2, 0.48f) * 0.5f, 0.0f, 1.0f);
    Result.CommunityPatch[2] = FMath::Clamp(0.5f + SampleFbm(WarpedX + 29831.0f, WarpedY + 4711.0f, Settings.Frequency * 1.55f, 2, 0.46f) * 0.5f, 0.0f, 1.0f);
    Result.CommunityPatch[3] = FMath::Clamp(0.5f + SampleFbm(WarpedX - 15331.0f, WarpedY - 22051.0f, Settings.Frequency * 0.72f, 2, 0.50f) * 0.5f, 0.0f, 1.0f);
    Result.ParentMaterial = FMath::Clamp(
        0.5f + SampleFbm(WarpedX + 7937.0f, WarpedY - 12011.0f, Settings.Frequency * 0.88f, 3, 0.50f) * 0.5f,
        0.0f,
        1.0f
    );
    return Result;
}

FCubusBiomeClimateContext FCubusBiomeField::LerpClimate(
    const FCubusBiomeClimateContext& A,
    const FCubusBiomeClimateContext& B,
    const float Alpha
)
{
    const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
    FCubusBiomeClimateContext Result;
    Result.ProvinceMoisture = FMath::Lerp(A.ProvinceMoisture, B.ProvinceMoisture, T);
    Result.RegionalMoisture = FMath::Lerp(A.RegionalMoisture, B.RegionalMoisture, T);
    Result.LocalHumidity = FMath::Lerp(A.LocalHumidity, B.LocalHumidity, T);
    Result.ProvinceTemperature = FMath::Lerp(A.ProvinceTemperature, B.ProvinceTemperature, T);
    Result.RegionalTemperature = FMath::Lerp(A.RegionalTemperature, B.RegionalTemperature, T);
    Result.LocalTemperature = FMath::Lerp(A.LocalTemperature, B.LocalTemperature, T);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Result.CommunityPatch[Index] = FMath::Lerp(A.CommunityPatch[Index], B.CommunityPatch[Index], T);
    }
    Result.ParentMaterial = FMath::Lerp(A.ParentMaterial, B.ParentMaterial, T);
    return Result;
}

'''
biome_cpp.write_text(text.replace(marker, climate_methods + marker, 1), encoding="utf-8")

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''    const FCubusBiomeFieldSettings& InSettings,
    const FCubusBiomeTerrainContext& TerrainContext
)
''',
    '''    const FCubusBiomeFieldSettings& InSettings,
    const FCubusBiomeTerrainContext& TerrainContext,
    const FCubusBiomeClimateContext* ClimateContext
)
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''    const FCubusTerrainFormSample Form = FCubusTerrainForm::Sample(
        TerrainX,
        TerrainY,
        Settings.HydrologySettings.TerrainFormSettings
    );
''',
    '''    const FCubusTerrainFormSample Form = TerrainContext.bHasTerrainFormSample
        ? TerrainContext.TerrainFormSample
        : FCubusTerrainForm::Sample(
            TerrainX,
            TerrainY,
            Settings.HydrologySettings.TerrainFormSettings
        );
'''
)

old_climate_block = r'''    const float BiomeX = WorldX + static_cast<float>(Settings.BiomeOffsetX);
    const float BiomeY = WorldY + static_cast<float>(Settings.BiomeOffsetY);

    /* Broad seeded climate provinces remain coherent; local terrain modifies them afterwards. */
    const float ClimateProvinceFrequency = Settings.Frequency * 0.18f;
    const float ClimateRegionalFrequency = Settings.Frequency * 0.62f;
    const float ClimateLocalFrequency = Settings.Frequency * 1.55f;
    const float WarpFrequency = ClimateProvinceFrequency * 0.55f;
    const float WarpAmplitude = FMath::Clamp(0.18f / ClimateProvinceFrequency, 96.0f, 520.0f);
    const float WarpedX = BiomeX + SampleFbm(
        BiomeX + 3527.0f,
        BiomeY - 1871.0f,
        WarpFrequency,
        3,
        0.5f
    ) * WarpAmplitude;
    const float WarpedY = BiomeY + SampleFbm(
        BiomeX - 6173.0f,
        BiomeY + 2593.0f,
        WarpFrequency,
        3,
        0.5f
    ) * WarpAmplitude;

    const float ProvinceMoisture = SampleFbm(WarpedX + 4219.0f, WarpedY - 1877.0f, ClimateProvinceFrequency * 0.95f, 4, 0.52f);
    const float RegionalMoisture = SampleFbm(WarpedX - 11549.0f, WarpedY + 9011.0f, ClimateRegionalFrequency * 0.90f, 3, 0.50f);
    const float LocalHumidity = SampleFbm(WarpedX + 1013.0f, WarpedY + 6427.0f, ClimateLocalFrequency, 2, 0.45f);
    const float ProvinceTemperature = SampleFbm(WarpedX - 8111.0f, WarpedY + 3203.0f, ClimateProvinceFrequency * 0.78f, 4, 0.52f);
    const float RegionalTemperature = SampleFbm(WarpedX + 14831.0f, WarpedY - 10427.0f, ClimateRegionalFrequency * 0.82f, 3, 0.50f);
    const float LocalTemperature = SampleFbm(WarpedX - 2791.0f, WarpedY - 15317.0f, ClimateLocalFrequency * 0.72f, 2, 0.45f);

    /* Four coherent patch fields are shared by every community. They modulate valid habitat; they never create it. */
    const float CommunityPatch[4] =
    {
        FMath::Clamp(0.5f + SampleFbm(WarpedX + 17011.0f, WarpedY - 9017.0f, Settings.Frequency * 0.95f, 2, 0.48f) * 0.5f, 0.0f, 1.0f),
        FMath::Clamp(0.5f + SampleFbm(WarpedX - 2477.0f, WarpedY + 19421.0f, Settings.Frequency * 1.25f, 2, 0.48f) * 0.5f, 0.0f, 1.0f),
        FMath::Clamp(0.5f + SampleFbm(WarpedX + 29831.0f, WarpedY + 4711.0f, Settings.Frequency * 1.55f, 2, 0.46f) * 0.5f, 0.0f, 1.0f),
        FMath::Clamp(0.5f + SampleFbm(WarpedX - 15331.0f, WarpedY - 22051.0f, Settings.Frequency * 0.72f, 2, 0.50f) * 0.5f, 0.0f, 1.0f)
    };
'''
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    old_climate_block,
    '''    /* Slow climate noise can be supplied from a coarse deterministic lattice. */
    const FCubusBiomeClimateContext Climate = ClimateContext != nullptr
        ? *ClimateContext
        : SampleClimate(WorldX, WorldY, Settings);
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''    const FCubusHydrologySample Hydrology = bHasHydrology
        ? FCubusHydrologyField::Sample(WorldX, WorldY, Settings.HydrologySettings)
        : FCubusHydrologySample();
''',
    '''    const FCubusHydrologySample Hydrology = bHasHydrology
        ? (TerrainContext.bHasHydrologySample
            ? TerrainContext.HydrologySample
            : FCubusHydrologyField::Sample(WorldX, WorldY, Settings.HydrologySettings))
        : FCubusHydrologySample();
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''    const FVector2D PrevailingWind = FVector2D(0.82f, 0.57f).GetSafeNormal();
    const FVector2D SolarDirection = FVector2D(-0.42f, -0.91f).GetSafeNormal();
''',
    '''    FVector2D PrevailingWind = Settings.PrevailingWindDirection;
    if (PrevailingWind.SizeSquared() <= KINDA_SMALL_NUMBER)
    {
        PrevailingWind = FVector2D(0.82f, 0.57f);
    }
    PrevailingWind.Normalize();

    FVector2D SolarDirection = Settings.SolarDirection;
    if (SolarDirection.SizeSquared() <= KINDA_SMALL_NUMBER)
    {
        SolarDirection = FVector2D(-0.42f, -0.91f);
    }
    SolarDirection.Normalize();
'''
)

old_soil = r'''    const float DepositionalGround = FMath::Clamp(
        Result.FlowConvergence * 0.48f + Result.Drainage * 0.22f + Result.RiverInfluence * 0.30f,
        0.0f,
        1.0f
    );
    const float ParentMaterial = FMath::Clamp(
        0.5f + SampleFbm(WarpedX + 7937.0f, WarpedY - 12011.0f, Settings.Frequency * 0.88f, 3, 0.50f) * 0.5f,
        0.0f,
        1.0f
    );
    Result.SoilPermeability = FMath::Clamp(
        0.24f + ParentMaterial * 0.48f + Result.RockExposure * 0.18f - Result.FlowConvergence * 0.12f,
        0.0f,
        1.0f
    );
    const float SlopeErosion = SmoothStep(
        Settings.RockySlopeThreshold * 0.28f,
        Settings.RockySlopeThreshold * 1.10f,
        Slope
    );
    Result.Erosion = FMath::Clamp(
        SlopeErosion * 0.50f + Result.WindExposure * 0.22f + Result.RockExposure * 0.20f - DepositionalGround * 0.24f,
        0.0f,
        1.0f
    );
    const float FloodDisturbance = FMath::Clamp(
        Result.FloodplainInfluence * (0.28f + Result.RiverInfluence * 0.72f),
        0.0f,
        1.0f
    );
    Result.Disturbance = FMath::Clamp(
        FMath::Max(
            FMath::Max(Result.Erosion, FloodDisturbance),
            FMath::Max(
                Result.WindExposure * 0.62f + Form.SurfaceRoughness * 0.18f,
                Form.ErosionRills * 0.72f + Result.RockExposure * 0.28f
            )
        ),
        0.0f,
        1.0f
    );
    const float SoilRetention = FMath::Clamp(
        GentleGround * (1.0f - Result.RockExposure) * (1.0f - Result.Erosion * 0.72f),
        0.0f,
        1.0f
    );
    Result.SoilDepth = FMath::Clamp(
        SoilRetention * FMath::Lerp(0.50f, 1.18f, DepositionalGround),
        0.0f,
        1.0f
    );
'''
new_soil = r'''    const float DepositionalGround = FMath::Clamp(
        Result.FlowConvergence * 0.48f + Result.Drainage * 0.22f + Result.RiverInfluence * 0.30f,
        0.0f,
        1.0f
    );

    /*
     * Soil now inherits the density world's actual rock fabric. Hard coherent
     * rock tends toward thin coarse regolith; fractured/softer rock weathers
     * deeper, but fractures also increase drainage through the substrate.
     * Standalone biome sampling retains the old seeded parent-material field
     * as a neutral fallback when no density geology context is available.
     */
    Result.SubstrateHardness = TerrainContext.bHasSubstrateSample
        ? FMath::Clamp(TerrainContext.SubstrateHardness, 0.0f, 1.0f)
        : FMath::Clamp(1.0f - Climate.ParentMaterial * 0.72f, 0.0f, 1.0f);
    Result.FractureDensity = TerrainContext.bHasSubstrateSample
        ? FMath::Clamp(TerrainContext.FractureDensity, 0.0f, 1.0f)
        : FMath::Clamp(Climate.ParentMaterial * 0.44f, 0.0f, 1.0f);
    const float SubstrateWeatherability = FMath::Clamp(
        (1.0f - Result.SubstrateHardness) * 0.62f + Result.FractureDensity * 0.38f,
        0.0f,
        1.0f
    );
    Result.SoilCoarseness = FMath::Clamp(
        Result.SubstrateHardness * 0.46f +
        Result.FractureDensity * 0.32f +
        Result.RockExposure * 0.28f -
        DepositionalGround * 0.20f,
        0.0f,
        1.0f
    );
    Result.SoilPermeability = FMath::Clamp(
        0.16f +
        Result.SoilCoarseness * 0.34f +
        Result.FractureDensity * 0.26f +
        Result.RockExposure * 0.12f -
        Result.FlowConvergence * 0.10f,
        0.0f,
        1.0f
    );
    Result.WaterHoldingCapacity = FMath::Clamp(
        0.28f +
        (1.0f - Result.SoilCoarseness) * 0.36f +
        SubstrateWeatherability * 0.18f +
        DepositionalGround * 0.20f -
        Result.RockExposure * 0.12f,
        0.0f,
        1.0f
    );

    const float SlopeErosion = SmoothStep(
        Settings.RockySlopeThreshold * 0.28f,
        Settings.RockySlopeThreshold * 1.10f,
        Slope
    );
    Result.Erosion = FMath::Clamp(
        SlopeErosion * 0.50f + Result.WindExposure * 0.22f + Result.RockExposure * 0.20f - DepositionalGround * 0.24f,
        0.0f,
        1.0f
    );
    const float FloodDisturbance = FMath::Clamp(
        Result.FloodplainInfluence * (0.28f + Result.RiverInfluence * 0.72f),
        0.0f,
        1.0f
    );
    Result.Disturbance = FMath::Clamp(
        FMath::Max(
            FMath::Max(Result.Erosion, FloodDisturbance),
            FMath::Max(
                Result.WindExposure * 0.62f + Form.SurfaceRoughness * 0.18f,
                Form.ErosionRills * 0.72f + Result.RockExposure * 0.28f
            )
        ),
        0.0f,
        1.0f
    );
    const float SoilRetention = FMath::Clamp(
        GentleGround * (1.0f - Result.RockExposure) * (1.0f - Result.Erosion * 0.72f),
        0.0f,
        1.0f
    );
    Result.SoilDepth = FMath::Clamp(
        SoilRetention *
        FMath::Lerp(0.62f, 1.16f, SubstrateWeatherability) *
        FMath::Lerp(0.54f, 1.18f, DepositionalGround),
        0.0f,
        1.0f
    );
'''
replace_once("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", old_soil, new_soil)

# Exact climate field consumers.
for old, new in (
    ("        ProvinceMoisture * 0.26f +", "        Climate.ProvinceMoisture * 0.26f +"),
    ("        RegionalMoisture * 0.18f +", "        Climate.RegionalMoisture * 0.18f +"),
    ("        LocalHumidity * 0.08f +", "        Climate.LocalHumidity * 0.08f +"),
    ("        ProvinceTemperature * 0.32f +", "        Climate.ProvinceTemperature * 0.32f +"),
    ("        RegionalTemperature * 0.20f +", "        Climate.RegionalTemperature * 0.20f +"),
    ("        LocalTemperature * 0.08f -", "        Climate.LocalTemperature * 0.08f -"),
    ("        ParentMaterial * 0.14f,", "        SubstrateWeatherability * 0.14f,"),
):
    replace_once("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", old, new)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''    Result.SoilSaturation = FMath::Clamp(
        Result.SurfaceWetness * (1.0f - Result.SoilPermeability * 0.55f) +
        Result.GroundwaterPotential * 0.34f,
        0.0f,
        1.0f
    );
''',
    '''    Result.SoilSaturation = FMath::Clamp(
        Result.SurfaceWetness * FMath::Lerp(0.64f, 1.08f, Result.WaterHoldingCapacity) *
        (1.0f - Result.SoilPermeability * 0.44f) +
        Result.GroundwaterPotential * 0.32f,
        0.0f,
        1.0f
    );
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''    auto PatchFactor = [&CommunityPatch](const int32 Slot, const float Strength)
    {
        const float Patch = CommunityPatch[FMath::Abs(Slot) % 4];
''',
    '''    auto PatchFactor = [&Climate](const int32 Slot, const float Strength)
    {
        const float Patch = Climate.CommunityPatch[FMath::Abs(Slot) % 4];
'''
)

# Add geology-sensitive communities before generic meadow.
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''        AddBuiltIn(
  TEXT("Meadow"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
''',
    '''        AddBuiltIn(
  TEXT("DeepColluvialForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * Result.LowerSlopeInfluence * Result.TreeLineWeight * GentleTerrain *
      FMath::Lerp(0.42f, 1.0f, Result.SoilDepth) *
      FMath::Lerp(0.50f, 1.08f, Result.WaterHoldingCapacity) *
      FMath::Lerp(0.58f, 1.04f, Result.WindShelter) *
      (1.0f - Result.Disturbance * 0.46f),
  1, 0.26f
        );
        AddBuiltIn(
  TEXT("FracturedRockHeath"), ECubusBiomeKind::Rocky, Settings.RockySurfaceMaterialId,
  FMath::Max(FoothillBand, FMath::Max(MontaneBand, SubalpineBand * 0.72f)) *
      Result.FractureDensity * Result.CanopyOpenness *
      RangeSuitability(Result.SoilCoarseness, 0.34f, 0.90f, 0.12f) *
      RangeSuitability(Result.SoilMoisture, 0.12f, 0.66f, 0.12f) *
      (1.0f - Result.NivalInfluence),
  2, 0.24f
        );
        AddBuiltIn(
  TEXT("DryRockWoodland"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  FMath::Max(LowlandBand, FoothillBand) * Result.TreeLineWeight * GentleTerrain *
      RangeSuitability(Result.SoilMoisture, 0.10f, 0.52f, 0.12f) *
      RangeSuitability(Result.SoilCoarseness, 0.28f, 0.82f, 0.12f) *
      FMath::Lerp(0.70f, 1.04f, Result.SolarExposure) *
      FMath::Lerp(0.34f, 0.88f, Result.CanopyOpenness) *
      (1.0f - Result.WetlandWeight),
  3, 0.30f
        );
        AddBuiltIn(
  TEXT("Meadow"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''                RangeSuitability(Result.OrganicMatter, Definition.MinimumOrganicMatter, Definition.MaximumOrganicMatter, Softness) *
                RangeSuitability(Result.Erosion, Definition.MinimumErosion, Definition.MaximumErosion, Softness) *
''',
    '''                RangeSuitability(Result.OrganicMatter, Definition.MinimumOrganicMatter, Definition.MaximumOrganicMatter, Softness) *
                RangeSuitability(Result.SubstrateHardness, Definition.MinimumSubstrateHardness, Definition.MaximumSubstrateHardness, Softness) *
                RangeSuitability(Result.FractureDensity, Definition.MinimumFractureDensity, Definition.MaximumFractureDensity, Softness) *
                RangeSuitability(Result.SoilCoarseness, Definition.MinimumSoilCoarseness, Definition.MaximumSoilCoarseness, Softness) *
                RangeSuitability(Result.WaterHoldingCapacity, Definition.MinimumWaterHoldingCapacity, Definition.MaximumWaterHoldingCapacity, Softness) *
                RangeSuitability(Result.Erosion, Definition.MinimumErosion, Definition.MaximumErosion, Softness) *
'''
)

# Project compatibility weights from the selected community blend, so vegetation
# and the named biome cannot disagree about the local ecotone.
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp",
    '''    const FCubusBiomeCommunityBlend& DominantCommunity = Result.CommunityBlend[0];
''',
    '''    Result.PlainsWeight = 0.0f;
    Result.ForestWeight = 0.0f;
    Result.RockyWeight = 0.0f;
    Result.WetlandWeight = 0.0f;
    for (int32 Index = 0; Index < Result.CommunityBlendCount; ++Index)
    {
        const FCubusBiomeCommunityBlend& Community = Result.CommunityBlend[Index];
        switch (Community.Archetype)
        {
            case ECubusBiomeKind::Forest: Result.ForestWeight += Community.Weight; break;
            case ECubusBiomeKind::Rocky: Result.RockyWeight += Community.Weight; break;
            case ECubusBiomeKind::Wetland: Result.WetlandWeight += Community.Weight; break;
            case ECubusBiomeKind::Plains:
            default: Result.PlainsWeight += Community.Weight; break;
        }
    }

    const FCubusBiomeCommunityBlend& DominantCommunity = Result.CommunityBlend[0];
'''
)

# -----------------------------------------------------------------------------
# Density column owns the authoritative form, hydrology and substrate samples.
# Slow climate is cached on a deterministic 4-voxel lattice and interpolated.
# -----------------------------------------------------------------------------
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h",
    '''    struct FSurfaceData
    {
        float SurfaceVoxelHeight = 0.0f;
        FCubusTerrainFormSample FormSample;
    };
''',
    '''    struct FSurfaceData
    {
        float SurfaceVoxelHeight = 0.0f;
        FCubusTerrainFormSample FormSample;
        bool bHasHydrologySample = false;
        FCubusHydrologySample HydrologySample;
    };
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h",
    '''    static constexpr float CoordinateCacheScale = 20.0f;

    mutable TMap<FIntPoint, FSurfaceData> SurfaceCache;
    mutable TMap<FIntPoint, FColumnData> ColumnCache;
''',
    '''    static constexpr float CoordinateCacheScale = 20.0f;
    static constexpr float BiomeClimateCacheCellSize = 4.0f;

    mutable TMap<FIntPoint, FSurfaceData> SurfaceCache;
    mutable TMap<FIntPoint, FColumnData> ColumnCache;
    mutable TMap<FIntPoint, FCubusBiomeClimateContext> BiomeClimateCache;
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h",
    '''    const FSurfaceData& GetCachedSurfaceData(float WorldX, float WorldY) const;
    float GetCachedSurfaceVoxelHeight(float WorldX, float WorldY) const;
    const FColumnData& GetColumnData(float WorldX, float WorldY) const;
''',
    '''    const FSurfaceData& GetCachedSurfaceData(float WorldX, float WorldY) const;
    float GetCachedSurfaceVoxelHeight(float WorldX, float WorldY) const;
    const FColumnData& GetColumnData(float WorldX, float WorldY) const;
    const FCubusBiomeClimateContext& GetCachedBiomeClimateCell(const FIntPoint& CellCoordinate) const;
    FCubusBiomeClimateContext GetInterpolatedBiomeClimate(float WorldX, float WorldY) const;
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h",
    '''    float SampleRiverDistance(float WorldX, float WorldY) const;
    float ApplyRiverLowering(float SurfaceHeight, float WorldX, float WorldY) const;
''',
    '''    float SampleRiverDistance(float WorldX, float WorldY) const;
    float ApplyRiverLowering(float SurfaceHeight, float WorldX, float WorldY) const;
    float ApplyRiverLowering(float SurfaceHeight, const FCubusHydrologySample& Hydrology) const;
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp",
    '''\tSurfaceCache.Reserve(1600);
\tColumnCache.Reserve(1600);
''',
    '''\tSurfaceCache.Reserve(1600);
\tColumnCache.Reserve(1600);
\tBiomeClimateCache.Reserve(256);
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp",
    '''\t\tSurface.FormSample = FCubusTerrainForm::Sample(TerrainX, TerrainY, TerrainFormSettings);
\t\tconst FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(TerrainX, TerrainY, Settings.LandmarkSettings);
\t\tSurface.SurfaceVoxelHeight = ApplyRiverLowering(Surface.FormSample.Height + LandmarkSample.HeightOffset, WorldX, WorldY);
''',
    '''\t\tSurface.FormSample = FCubusTerrainForm::Sample(TerrainX, TerrainY, TerrainFormSettings);
\t\tconst FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(TerrainX, TerrainY, Settings.LandmarkSettings);
\t\tconst float UncarvedSurfaceHeight = Surface.FormSample.Height + LandmarkSample.HeightOffset;
\t\tif (HydrologySettings.bEnabled)
\t\t{
\t\t\tSurface.HydrologySample = FCubusHydrologyField::Sample(WorldX, WorldY, HydrologySettings);
\t\t\tSurface.bHasHydrologySample = true;
\t\t\tSurface.SurfaceVoxelHeight = ApplyRiverLowering(UncarvedSurfaceHeight, Surface.HydrologySample);
\t\t}
\t\telse
\t\t{
\t\t\tSurface.SurfaceVoxelHeight = UncarvedSurfaceHeight;
\t\t}
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp",
    '''\tColumn.BiomeSample = FCubusBiomeField::Sample(
\t\tWorldX,
\t\tWorldY,
\t\tColumn.SurfaceVoxelHeight,
\t\tColumn.Slope,
\t\tSettings.BiomeSettings
\t);
''',
    '''\tFCubusBiomeTerrainContext BiomeTerrainContext;
\tBiomeTerrainContext.Drainage = Column.FormSample.Drainage;
\tBiomeTerrainContext.RockExposure = Column.RockExposure;
\tBiomeTerrainContext.MountainCore = Column.FormSample.MountainCore;
\tBiomeTerrainContext.FoothillWeight = Column.FormSample.FoothillWeight;
\tBiomeTerrainContext.Ridge = Column.FormSample.Ridge;
\tBiomeTerrainContext.Gradient = Column.Gradient;
\tBiomeTerrainContext.bHasTerrainFormSample = true;
\tBiomeTerrainContext.TerrainFormSample = Column.FormSample;
\tBiomeTerrainContext.bHasHydrologySample = Surface.bHasHydrologySample;
\tBiomeTerrainContext.HydrologySample = Surface.HydrologySample;
\tBiomeTerrainContext.bHasSubstrateSample = true;
\tBiomeTerrainContext.SubstrateHardness = Column.RockHardness;
\tBiomeTerrainContext.FractureDensity = Column.Fracture;

\tconst FCubusBiomeClimateContext Climate = GetInterpolatedBiomeClimate(WorldX, WorldY);
\tColumn.BiomeSample = FCubusBiomeField::Sample(
\t\tWorldX,
\t\tWorldY,
\t\tColumn.SurfaceVoxelHeight,
\t\tColumn.Slope,
\t\tSettings.BiomeSettings,
\t\tBiomeTerrainContext,
\t\t&Climate
\t);
'''
)

density_cpp = Path("Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp")
text = density_cpp.read_text(encoding="utf-8")
marker = "FCubusTerrainDensityField::FTerrainRegionWeights FCubusTerrainDensityField::SampleTerrainRegions("
if text.count(marker) != 1:
    raise RuntimeError("could not find unique density climate-cache insertion point")
climate_cache_methods = r'''const FCubusBiomeClimateContext& FCubusTerrainDensityField::GetCachedBiomeClimateCell(
    const FIntPoint& CellCoordinate
) const
{
    if (const FCubusBiomeClimateContext* Existing = BiomeClimateCache.Find(CellCoordinate))
    {
        return *Existing;
    }

    const float WorldX = static_cast<float>(CellCoordinate.X) * BiomeClimateCacheCellSize;
    const float WorldY = static_cast<float>(CellCoordinate.Y) * BiomeClimateCacheCellSize;
    BiomeClimateCache.Add(
        CellCoordinate,
        FCubusBiomeField::SampleClimate(WorldX, WorldY, Settings.BiomeSettings)
    );
    return BiomeClimateCache.FindChecked(CellCoordinate);
}

FCubusBiomeClimateContext FCubusTerrainDensityField::GetInterpolatedBiomeClimate(
    const float WorldX,
    const float WorldY
) const
{
    const float GridX = WorldX / BiomeClimateCacheCellSize;
    const float GridY = WorldY / BiomeClimateCacheCellSize;
    const int32 CellX = FMath::FloorToInt(GridX);
    const int32 CellY = FMath::FloorToInt(GridY);
    const float AlphaX = GridX - static_cast<float>(CellX);
    const float AlphaY = GridY - static_cast<float>(CellY);

    /* Copy immediately: later TMap inserts may rehash and invalidate references. */
    const FCubusBiomeClimateContext C00 = GetCachedBiomeClimateCell(FIntPoint(CellX, CellY));
    const FCubusBiomeClimateContext C10 = GetCachedBiomeClimateCell(FIntPoint(CellX + 1, CellY));
    const FCubusBiomeClimateContext C01 = GetCachedBiomeClimateCell(FIntPoint(CellX, CellY + 1));
    const FCubusBiomeClimateContext C11 = GetCachedBiomeClimateCell(FIntPoint(CellX + 1, CellY + 1));
    const FCubusBiomeClimateContext Bottom = FCubusBiomeField::LerpClimate(C00, C10, AlphaX);
    const FCubusBiomeClimateContext Top = FCubusBiomeField::LerpClimate(C01, C11, AlphaX);
    return FCubusBiomeField::LerpClimate(Bottom, Top, AlphaY);
}

'''
density_cpp.write_text(text.replace(marker, climate_cache_methods + marker, 1), encoding="utf-8")

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp",
    '''float FCubusTerrainDensityField::ApplyRiverLowering(const float SurfaceHeight, const float WorldX, const float WorldY) const
{
\tif (!HydrologySettings.bEnabled)
\t{
\t\treturn SurfaceHeight;
\t}

\tconst FCubusHydrologySample Hydrology = FCubusHydrologyField::Sample(WorldX, WorldY, HydrologySettings);
\tif (!Hydrology.IsChannel())
''',
    '''float FCubusTerrainDensityField::ApplyRiverLowering(const float SurfaceHeight, const float WorldX, const float WorldY) const
{
\tif (!HydrologySettings.bEnabled)
\t{
\t\treturn SurfaceHeight;
\t}

\treturn ApplyRiverLowering(
\t\tSurfaceHeight,
\t\tFCubusHydrologyField::Sample(WorldX, WorldY, HydrologySettings)
\t);
}

float FCubusTerrainDensityField::ApplyRiverLowering(
\tconst float SurfaceHeight,
\tconst FCubusHydrologySample& Hydrology
) const
{
\tif (!Hydrology.IsChannel())
'''
)

# -----------------------------------------------------------------------------
# Vegetation retains the richer biome context instead of dropping it.
# -----------------------------------------------------------------------------
replace_once(
    "Source/Orakai/CubusCore/Data/CubusVegetationInstance.h",
    '''    uint8 SolarExposure = 128;
    uint8 Erosion = 0;
    uint8 SlopeDegrees = 0;
''',
    '''    uint8 SolarExposure = 128;
    uint8 Erosion = 0;
    uint8 ColdAirPooling = 0;
    uint8 Disturbance = 0;
    uint8 CanopyPotential = 0;
    uint8 SoilCoarseness = 128;
    uint8 WaterHoldingCapacity = 128;
    uint8 SlopeDegrees = 0;
'''
)

replace_once(
    "Source/Orakai/CubusCore/Data/CubusVegetationInstance.h",
    '''    float GetSolarExposure() const { return DecodeUnit(SolarExposure); }
    float GetErosion() const { return DecodeUnit(Erosion); }
    float GetSlopeDegrees() const { return DecodeUnit(SlopeDegrees) * 90.0f; }
''',
    '''    float GetSolarExposure() const { return DecodeUnit(SolarExposure); }
    float GetErosion() const { return DecodeUnit(Erosion); }
    float GetColdAirPooling() const { return DecodeUnit(ColdAirPooling); }
    float GetDisturbance() const { return DecodeUnit(Disturbance); }
    float GetCanopyPotential() const { return DecodeUnit(CanopyPotential); }
    float GetSoilCoarseness() const { return DecodeUnit(SoilCoarseness); }
    float GetWaterHoldingCapacity() const { return DecodeUnit(WaterHoldingCapacity); }
    float GetSlopeDegrees() const { return DecodeUnit(SlopeDegrees) * 90.0f; }
'''
)

replace_once(
    "Source/Orakai/CubusCore/Vegetation/CubusVegetationTypes.h",
    '''    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumErosion = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
''',
    '''    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumErosion = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumColdAirPooling = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumColdAirPooling = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Disturbance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumDisturbance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Disturbance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumDisturbance = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumCanopyPotential = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumCanopyPotential = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSoilCoarseness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSoilCoarseness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumWaterHoldingCapacity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumWaterHoldingCapacity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''        Habitat.SolarExposure = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.SolarExposure);
        Habitat.Erosion = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Erosion);
        Habitat.SlopeDegrees = FCubusVegetationHabitatSample::QuantizeSlopeDegrees(
''',
    '''        Habitat.SolarExposure = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.SolarExposure);
        Habitat.Erosion = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Erosion);
        Habitat.ColdAirPooling = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.ColdAirPooling);
        Habitat.Disturbance = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Disturbance);
        Habitat.CanopyPotential = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.CanopyPotential);
        Habitat.SoilCoarseness = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.SoilCoarseness);
        Habitat.WaterHoldingCapacity = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.WaterHoldingCapacity);
        Habitat.SlopeDegrees = FCubusVegetationHabitatSample::QuantizeSlopeDegrees(
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''        const float Erosion = FMath::Clamp(BiomeSample.Erosion, 0.0f, 1.0f);
        const float Alpine = FMath::Clamp(BiomeSample.AlpineInfluence, 0.0f, 1.0f);
''',
    '''        const float Erosion = FMath::Clamp(BiomeSample.Erosion, 0.0f, 1.0f);
        const float ColdPool = FMath::Clamp(BiomeSample.ColdAirPooling, 0.0f, 1.0f);
        const float Disturbance = FMath::Clamp(BiomeSample.Disturbance, 0.0f, 1.0f);
        const float Canopy = FMath::Clamp(BiomeSample.CanopyPotential, 0.0f, 1.0f);
        const float CanopyOpen = 1.0f - Canopy;
        const float Coarseness = FMath::Clamp(BiomeSample.SoilCoarseness, 0.0f, 1.0f);
        const float WaterHolding = FMath::Clamp(BiomeSample.WaterHoldingCapacity, 0.0f, 1.0f);
        const float Alpine = FMath::Clamp(BiomeSample.AlpineInfluence, 0.0f, 1.0f);
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''            FMath::Lerp(0.35f, 1.0f, Fertility) *
            FMath::Lerp(1.0f, 0.35f, Exposure) *
            FMath::Lerp(1.0f, 0.30f, Rock) *
''',
    '''            FMath::Lerp(0.35f, 1.0f, Fertility) *
            FMath::Lerp(0.55f, 1.10f, WaterHolding) *
            FMath::Lerp(1.0f, 0.52f, Coarseness) *
            FMath::Lerp(1.0f, 0.35f, Exposure) *
            FMath::Lerp(1.0f, 0.30f, Rock) *
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''        Result.TreeDensity = FMath::Clamp(
            TreeBase * FMath::Lerp(0.55f, 1.30f, EcologyStrength) * TreeHabitat,
            0.0f,
            1.0f
        );
''',
    '''        Result.TreeDensity = FMath::Clamp(
            TreeBase *
            FMath::Lerp(0.55f, 1.30f, EcologyStrength) *
            TreeHabitat *
            FMath::Lerp(0.48f, 1.18f, Canopy) *
            FMath::Lerp(1.0f, 0.42f, Disturbance),
            0.0f,
            1.0f
        );
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''            FMath::SmoothStep(0.42f, 0.76f, Saturation) *
            FMath::Lerp(0.55f, 1.0f, Fertility) * VeryGentle * (1.0f - Nival);
''',
    '''            FMath::SmoothStep(0.42f, 0.76f, Saturation) *
            FMath::Lerp(0.55f, 1.0f, Fertility) *
            FMath::Lerp(0.72f, 1.08f, WaterHolding) * VeryGentle * (1.0f - Nival);
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''            FMath::Lerp(0.45f, 1.0f, FMath::Max(Rock, BiomeSample.RockyWeight)) *
            (1.0f - River * 0.8f);
''',
    '''            FMath::Lerp(0.45f, 1.0f, FMath::Max(Rock, BiomeSample.RockyWeight)) *
            FMath::Lerp(0.82f, 1.12f, Coarseness) *
            (1.0f - River * 0.8f);
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''        const float ShrubHabitat =
            RangeSuitability(Moisture, 0.12f, 0.76f) *
''',
    '''        const float ModerateDisturbance = 1.0f - FMath::Clamp(FMath::Abs(Disturbance - 0.45f) / 0.55f, 0.0f, 1.0f);
        const float ShrubHabitat =
            RangeSuitability(Moisture, 0.12f, 0.76f) *
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''            FMath::Lerp(1.1f, 0.62f, Soil) *
            FMath::Lerp(1.0f, 0.58f, Erosion) *
            (1.0f - River * 0.72f) * (1.0f - Nival) *
''',
    '''            FMath::Lerp(1.1f, 0.62f, Soil) *
            FMath::Lerp(1.0f, 0.58f, Erosion) *
            FMath::Lerp(0.76f, 1.14f, CanopyOpen) *
            FMath::Lerp(0.86f, 1.10f, ModerateDisturbance) *
            (1.0f - River * 0.72f) * (1.0f - Nival) *
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBlockVegetationGenerator.cpp",
    '''            FMath::Lerp(0.48f, 1.0f, Fertility) *
            FMath::Lerp(1.0f, 0.55f, Rock) *
            FMath::Lerp(1.0f, 0.66f, Erosion) *
            (1.0f - Nival) * Gentle;
''',
    '''            FMath::Lerp(0.48f, 1.0f, Fertility) *
            FMath::Lerp(1.0f, 0.55f, Rock) *
            FMath::Lerp(1.0f, 0.66f, Erosion) *
            FMath::Lerp(0.58f, 1.12f, CanopyOpen) *
            FMath::Lerp(0.92f, 1.06f, ColdPool) *
            FMath::Lerp(0.82f, 1.08f, WaterHolding) *
            (1.0f - Nival) * Gentle;
'''
)

replace_once(
    "Source/Orakai/CubusCore/Vegetation/CubusVegetationCatalog.cpp",
    '''        const float Solar = SmoothRangeSuitability(Sample.GetSolarExposure(), Habitat.MinimumSolarExposure, Habitat.MaximumSolarExposure, Softness);
        const float Erosion = SmoothRangeSuitability(Sample.GetErosion(), Habitat.MinimumErosion, Habitat.MaximumErosion, Softness);

        const float MaximumSlope = FMath::Clamp(Habitat.MaximumSlopeDegrees, 0.1f, 90.0f);
''',
    '''        const float Solar = SmoothRangeSuitability(Sample.GetSolarExposure(), Habitat.MinimumSolarExposure, Habitat.MaximumSolarExposure, Softness);
        const float Erosion = SmoothRangeSuitability(Sample.GetErosion(), Habitat.MinimumErosion, Habitat.MaximumErosion, Softness);
        const float ColdPool = SmoothRangeSuitability(Sample.GetColdAirPooling(), Habitat.MinimumColdAirPooling, Habitat.MaximumColdAirPooling, Softness);
        const float Disturbance = SmoothRangeSuitability(Sample.GetDisturbance(), Habitat.MinimumDisturbance, Habitat.MaximumDisturbance, Softness);
        const float Canopy = SmoothRangeSuitability(Sample.GetCanopyPotential(), Habitat.MinimumCanopyPotential, Habitat.MaximumCanopyPotential, Softness);
        const float Coarseness = SmoothRangeSuitability(Sample.GetSoilCoarseness(), Habitat.MinimumSoilCoarseness, Habitat.MaximumSoilCoarseness, Softness);
        const float WaterHolding = SmoothRangeSuitability(Sample.GetWaterHoldingCapacity(), Habitat.MinimumWaterHoldingCapacity, Habitat.MaximumWaterHoldingCapacity, Softness);

        const float MaximumSlope = FMath::Clamp(Habitat.MaximumSlopeDegrees, 0.1f, 90.0f);
'''
)

replace_once(
    "Source/Orakai/CubusCore/Vegetation/CubusVegetationCatalog.cpp",
    '''            Moisture * Temperature * Soil * Drainage * River * Rock * Exposure * Fertility *
            Elevation * TreeLine * Saturation * Groundwater * Solar * Erosion * SlopeSuitability
        );
        return FMath::Pow(Product, 1.0f / 15.0f);
''',
    '''            Moisture * Temperature * Soil * Drainage * River * Rock * Exposure * Fertility *
            Elevation * TreeLine * Saturation * Groundwater * Solar * Erosion *
            ColdPool * Disturbance * Canopy * Coarseness * WaterHolding * SlopeSuitability
        );
        return FMath::Pow(Product, 1.0f / 20.0f);
'''
)

# -----------------------------------------------------------------------------
# Tests: configurable exposure, geology-derived soil, compact habitat retention.
# -----------------------------------------------------------------------------
biome_tests = Path("Source/Orakai/CubusCore/Tests/CubusBiomeFieldTests.cpp")
text = biome_tests.read_text(encoding="utf-8")
insert_marker = "IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n    FCubusBiomeEcologicalNicheTest,"
if text.count(insert_marker) != 1:
    raise RuntimeError("could not find biome test insertion marker")
new_test = r'''IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBiomeGeologyClimateTest,
    "Orakai.Cubus.Generation.BiomeGeologyClimate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusBiomeGeologyClimateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusBiomeFieldSettings Settings;
    Settings.bEnabled = true;
    Settings.BiomeOffsetX = 441;
    Settings.BiomeOffsetY = -917;
    Settings.PrevailingWindDirection = FVector2D(1.0f, 0.0f);
    Settings.SolarDirection = FVector2D(0.0f, -1.0f);

    FCubusBiomeTerrainContext HardRock;
    HardRock.Gradient = FVector2D(1.0f, 0.0f);
    HardRock.MountainCore = 0.8f;
    HardRock.Ridge = 0.5f;
    HardRock.bHasSubstrateSample = true;
    HardRock.SubstrateHardness = 0.92f;
    HardRock.FractureDensity = 0.68f;

    FCubusBiomeTerrainContext SoftRock = HardRock;
    SoftRock.SubstrateHardness = 0.18f;
    SoftRock.FractureDensity = 0.16f;

    const FCubusBiomeSample Hard = FCubusBiomeField::Sample(96.0f, 32.0f, 24.0f, 0.35f, Settings, HardRock);
    const FCubusBiomeSample Soft = FCubusBiomeField::Sample(96.0f, 32.0f, 24.0f, 0.35f, Settings, SoftRock);
    TestTrue(TEXT("Hard fractured substrate produces coarser soil"), Hard.SoilCoarseness > Soft.SoilCoarseness);
    TestTrue(TEXT("Water holding capacity remains normalized"), Hard.WaterHoldingCapacity >= 0.0f && Hard.WaterHoldingCapacity <= 1.0f);
    TestTrue(TEXT("Substrate hardness reaches the biome sample"), FMath::IsNearlyEqual(Hard.SubstrateHardness, 0.92f, 0.001f));

    FCubusBiomeFieldSettings LeeSettings = Settings;
    LeeSettings.PrevailingWindDirection = FVector2D(-1.0f, 0.0f);
    const FCubusBiomeSample Windward = FCubusBiomeField::Sample(96.0f, 32.0f, 24.0f, 0.35f, Settings, HardRock);
    const FCubusBiomeSample Leeward = FCubusBiomeField::Sample(96.0f, 32.0f, 24.0f, 0.35f, LeeSettings, HardRock);
    TestTrue(TEXT("Configured prevailing wind changes topographic exposure"),
        !FMath::IsNearlyEqual(Windward.WindExposure, Leeward.WindExposure, 0.001f));

    const FCubusBiomeClimateContext ClimateA = FCubusBiomeField::SampleClimate(96.0f, 32.0f, Settings);
    const FCubusBiomeClimateContext ClimateB = FCubusBiomeField::SampleClimate(100.0f, 32.0f, Settings);
    const FCubusBiomeClimateContext ClimateMid = FCubusBiomeField::LerpClimate(ClimateA, ClimateB, 0.5f);
    TestTrue(TEXT("Prepared climate interpolation stays bounded"),
        ClimateMid.ParentMaterial >= 0.0f && ClimateMid.ParentMaterial <= 1.0f);
    return true;
}

'''
biome_tests.write_text(text.replace(insert_marker, new_test + insert_marker, 1), encoding="utf-8")

replace_once(
    "Source/Orakai/CubusCore/Tests/CubusVegetationEcologyTests.cpp",
    '''    Sample.SolarExposure = FCubusVegetationHabitatSample::QuantizeUnit(0.81f);
    Sample.Erosion = FCubusVegetationHabitatSample::QuantizeUnit(0.42f);
    Sample.SlopeDegrees = FCubusVegetationHabitatSample::QuantizeSlopeDegrees(37.0f);
''',
    '''    Sample.SolarExposure = FCubusVegetationHabitatSample::QuantizeUnit(0.81f);
    Sample.Erosion = FCubusVegetationHabitatSample::QuantizeUnit(0.42f);
    Sample.ColdAirPooling = FCubusVegetationHabitatSample::QuantizeUnit(0.64f);
    Sample.Disturbance = FCubusVegetationHabitatSample::QuantizeUnit(0.31f);
    Sample.CanopyPotential = FCubusVegetationHabitatSample::QuantizeUnit(0.77f);
    Sample.SoilCoarseness = FCubusVegetationHabitatSample::QuantizeUnit(0.58f);
    Sample.WaterHoldingCapacity = FCubusVegetationHabitatSample::QuantizeUnit(0.69f);
    Sample.SlopeDegrees = FCubusVegetationHabitatSample::QuantizeSlopeDegrees(37.0f);
'''
)

replace_once(
    "Source/Orakai/CubusCore/Tests/CubusVegetationEcologyTests.cpp",
    '''    TestTrue(TEXT("Erosion survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetErosion(), 0.42f, 0.005f));
    TestTrue(TEXT("Slope survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetSlopeDegrees(), 37.0f, 0.5f));
''',
    '''    TestTrue(TEXT("Erosion survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetErosion(), 0.42f, 0.005f));
    TestTrue(TEXT("Cold-air pooling survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetColdAirPooling(), 0.64f, 0.005f));
    TestTrue(TEXT("Disturbance survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetDisturbance(), 0.31f, 0.005f));
    TestTrue(TEXT("Canopy potential survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetCanopyPotential(), 0.77f, 0.005f));
    TestTrue(TEXT("Soil coarseness survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetSoilCoarseness(), 0.58f, 0.005f));
    TestTrue(TEXT("Water holding survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetWaterHoldingCapacity(), 0.69f, 0.005f));
    TestTrue(TEXT("Slope survives compact habitat quantization"), FMath::IsNearlyEqual(Sample.GetSlopeDegrees(), 37.0f, 0.5f));
'''
)

# Generation identity changes because climate interpolation, geology-aware soil,
# community competition and vegetation habitat all affect deterministic output.
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h",
    '''    // Bumped to 16: vegetation species and placement ecology now consume the v2
    // elevation, treeline, hydrology, solar-exposure and erosion habitat fields.
    static constexpr uint32 CurrentGenerationVersion = 16;
''',
    '''    // Bumped to 17: biomes now reuse authoritative terrain/hydrology, interpolate
    // slow climate fields, consume real substrate geology and retain the richer
    // disturbance/canopy/substrate habitat through vegetation species selection.
    static constexpr uint32 CurrentGenerationVersion = 17;
'''
)

# -----------------------------------------------------------------------------
# Static guards: do not commit a partial ecology path.
# -----------------------------------------------------------------------------
require("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", "FCubusBiomeClimateContext FCubusBiomeField::SampleClimate(", 1)
require("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", "TerrainContext.bHasTerrainFormSample", 1)
require("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", "TerrainContext.bHasHydrologySample", 1)
require("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", "TerrainContext.bHasSubstrateSample", 2)
require("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", "TEXT(\"DeepColluvialForest\")", 1)
require("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", "TEXT(\"FracturedRockHeath\")", 1)
require("Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp", "TEXT(\"DryRockWoodland\")", 1)
require("Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp", "GetInterpolatedBiomeClimate", 2)
require("Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp", "BiomeTerrainContext.bHasSubstrateSample = true;", 1)
require("Source/Orakai/CubusCore/Vegetation/CubusVegetationCatalog.cpp", "1.0f / 20.0f", 1)
require("Source/Orakai/CubusCore/Data/CubusVegetationInstance.h", "GetCanopyPotential", 1)
require("Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h", "CurrentGenerationVersion = 17", 1)
