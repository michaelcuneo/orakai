from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one anchor in {path}, found {count}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


def require(path, needle, count=None):
    text = Path(path).read_text(encoding="utf-8")
    actual = text.count(needle)
    if actual == 0:
        raise RuntimeError(f"missing required text in {path}: {needle!r}")
    if count is not None and actual != count:
        raise RuntimeError(f"expected {count} copies in {path}, found {actual}: {needle!r}")


# -----------------------------------------------------------------------------
# Climate province identity and authored topographic-climate constraints.
# -----------------------------------------------------------------------------
replace_once(
    "Source/Orakai/CubusCore/Data/CubusBiomeTypes.h",
    '''/** Structural elevation zone derived from sea level, treeline and the alpine/nival scale. */
UENUM(BlueprintType)
enum class ECubusElevationZone : uint8
''',
    '''/** Broad, slow-varying climate province independent of local slope and drainage. */
UENUM(BlueprintType)
enum class ECubusClimateProvince : uint8
{
    HumidCool,
    HumidTemperate,
    TemperateTransition,
    ContinentalInterior,
    DryWarm,
    DryCool
};

/** Structural elevation zone derived from sea level, treeline and the alpine/nival scale. */
UENUM(BlueprintType)
enum class ECubusElevationZone : uint8
'''
)

replace_once(
    "Source/Orakai/CubusCore/Data/CubusBiomeTypes.h",
    '''    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSolarExposure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
''',
    '''    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSolarExposure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumOrographicLift = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumOrographicLift = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumRainShadow = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumRainShadow = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSolarOcclusion = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSolarOcclusion = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
'''
)

# -----------------------------------------------------------------------------
# Runtime biome climate/topography context.
# -----------------------------------------------------------------------------
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.h",
    '''    float LocalTemperature = 0.0f;
    float CommunityPatch[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float ParentMaterial = 0.5f;
};

/** Terrain/environment context supplied by the authoritative density column. */
''',
    '''    float LocalTemperature = 0.0f;
    float MoistureTransport = 0.5f;
    float Continentality = 0.5f;
    float CommunityPatch[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float ParentMaterial = 0.5f;
};

/** Coarse long-range terrain/climate interaction, interpolated between lattice nodes. */
struct ORAKAI_API FCubusBiomeTopographicClimateContext
{
    float OrographicLift = 0.0f;
    float RainShadow = 0.0f;
    float SolarOcclusion = 0.0f;
    float SkyViewFactor = 1.0f;
};

/** Terrain/environment context supplied by the authoritative density column. */
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.h",
    '''    bool bHasSubstrateSample = false;
    float SubstrateHardness = 0.5f;
    float FractureDensity = 0.0f;
};
''',
    '''    bool bHasSubstrateSample = false;
    float SubstrateHardness = 0.5f;
    float FractureDensity = 0.0f;

    bool bHasTopographicClimateSample = false;
    FCubusBiomeTopographicClimateContext TopographicClimateSample;
};
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.h",
    '''    // Broad climate.
    float Moisture = 0.5f;
    float Temperature = 0.5f;

    // Elevation structure.
''',
    '''    // Broad climate.
    ECubusClimateProvince ClimateProvince = ECubusClimateProvince::TemperateTransition;
    float ClimateMoistureTransport = 0.5f;
    float ClimateContinentality = 0.5f;
    float Moisture = 0.5f;
    float Temperature = 0.5f;

    // Elevation structure.
'''
)

replace_once(
    "Source/Orakai/CubusCore/Generation/CubusBiomeField.h",
    '''    float WindShelter = 1.0f;
    float RainShadow = 0.0f;
    float Exposure = 0.0f;
''',
    '''    float WindShelter = 1.0f;
    float OrographicLift = 0.0f;
    float HorizonRainShadow = 0.0f;
    float RainShadow = 0.0f;
    float SolarOcclusion = 0.0f;
    float SkyViewFactor = 1.0f;
    float Exposure = 0.0f;
'''
)

# -----------------------------------------------------------------------------
# Climate province generation and topographic climate integration.
# -----------------------------------------------------------------------------
biome_cpp = "Source/Orakai/CubusCore/Generation/CubusBiomeField.cpp"
replace_once(
    biome_cpp,
    '''    ECubusElevationZone ResolveElevationZone(const float ElevationNormalized)
    {
''',
    '''    ECubusClimateProvince ResolveClimateProvince(const FCubusBiomeClimateContext& Climate)
    {
        const float Moisture = FMath::Clamp(
            Climate.MoistureTransport * 0.68f + (0.5f + Climate.ProvinceMoisture * 0.5f) * 0.32f,
            0.0f,
            1.0f
        );
        const float Temperature = FMath::Clamp(0.5f + Climate.ProvinceTemperature * 0.5f, 0.0f, 1.0f);

        if (Moisture >= 0.64f)
        {
            return Temperature < 0.44f ? ECubusClimateProvince::HumidCool : ECubusClimateProvince::HumidTemperate;
        }
        if (Moisture <= 0.36f)
        {
            return Temperature >= 0.52f ? ECubusClimateProvince::DryWarm : ECubusClimateProvince::DryCool;
        }
        if (Climate.Continentality >= 0.62f)
        {
            return ECubusClimateProvince::ContinentalInterior;
        }
        return ECubusClimateProvince::TemperateTransition;
    }

    ECubusElevationZone ResolveElevationZone(const float ElevationNormalized)
    {
'''
)

replace_once(
    biome_cpp,
    '''    Result.LocalTemperature = SampleFbm(WarpedX - 2791.0f, WarpedY - 15317.0f, ClimateLocalFrequency * 0.72f, 2, 0.45f);
    Result.CommunityPatch[0] = FMath::Clamp(0.5f + SampleFbm(WarpedX + 17011.0f, WarpedY - 9017.0f, Settings.Frequency * 0.95f, 2, 0.48f) * 0.5f, 0.0f, 1.0f);
''',
    '''    Result.LocalTemperature = SampleFbm(WarpedX - 2791.0f, WarpedY - 15317.0f, ClimateLocalFrequency * 0.72f, 2, 0.45f);
    Result.MoistureTransport = FMath::Clamp(
        0.5f + SampleFbm(WarpedX + 34817.0f, WarpedY - 28411.0f, ClimateProvinceFrequency * 0.54f, 3, 0.52f) * 0.5f,
        0.0f,
        1.0f
    );
    Result.Continentality = FMath::Clamp(
        0.5f + SampleFbm(WarpedX - 26339.0f, WarpedY + 31847.0f, ClimateProvinceFrequency * 0.43f, 3, 0.52f) * 0.5f,
        0.0f,
        1.0f
    );
    Result.CommunityPatch[0] = FMath::Clamp(0.5f + SampleFbm(WarpedX + 17011.0f, WarpedY - 9017.0f, Settings.Frequency * 0.95f, 2, 0.48f) * 0.5f, 0.0f, 1.0f);
'''
)

replace_once(
    biome_cpp,
    '''    Result.LocalTemperature = FMath::Lerp(A.LocalTemperature, B.LocalTemperature, T);
    for (int32 Index = 0; Index < 4; ++Index)
''',
    '''    Result.LocalTemperature = FMath::Lerp(A.LocalTemperature, B.LocalTemperature, T);
    Result.MoistureTransport = FMath::Lerp(A.MoistureTransport, B.MoistureTransport, T);
    Result.Continentality = FMath::Lerp(A.Continentality, B.Continentality, T);
    for (int32 Index = 0; Index < 4; ++Index)
'''
)

replace_once(
    biome_cpp,
    '''    const FCubusBiomeClimateContext Climate = ClimateContext != nullptr
        ? *ClimateContext
        : SampleClimate(WorldX, WorldY, Settings);

    const bool bHasHydrology = Settings.bGenerateRivers && Settings.HydrologySettings.bEnabled;
''',
    '''    const FCubusBiomeClimateContext Climate = ClimateContext != nullptr
        ? *ClimateContext
        : SampleClimate(WorldX, WorldY, Settings);
    Result.ClimateProvince = ResolveClimateProvince(Climate);
    Result.ClimateMoistureTransport = Climate.MoistureTransport;
    Result.ClimateContinentality = Climate.Continentality;

    const bool bHasHydrology = Settings.bGenerateRivers && Settings.HydrologySettings.bEnabled;
'''
)

old_exposure = '''    Result.SolarExposure = FMath::Clamp(
        0.50f + SolarFacing * 0.36f * SlopeAspectStrength + (1.0f - TopographicExposure) * 0.08f,
        0.0f,
        1.0f
    );
    Result.WindExposure = FMath::Clamp(
        TopographicExposure * FMath::Lerp(0.62f, 1.16f, Windwardness),
        0.0f,
        1.0f
    );
    Result.WindShelter = 1.0f - Result.WindExposure;
    Result.RainShadow = FMath::Clamp(
        (1.0f - Windwardness) * (Ecology.FoothillWeight * 0.34f + Ecology.MountainCore * 0.58f),
        0.0f,
        1.0f
    );
    Result.Exposure = FMath::Clamp(
        Result.WindExposure * 0.74f + Result.SolarExposure * 0.10f + Result.RockExposure * 0.16f,
        0.0f,
        1.0f
    );
'''
new_exposure = '''    const FCubusBiomeTopographicClimateContext TopographicClimate = TerrainContext.bHasTopographicClimateSample
        ? TerrainContext.TopographicClimateSample
        : FCubusBiomeTopographicClimateContext();
    Result.OrographicLift = FMath::Clamp(
        TerrainContext.bHasTopographicClimateSample
            ? TopographicClimate.OrographicLift
            : Windwardness * (Ecology.FoothillWeight * 0.46f + Ecology.MountainCore * 0.34f),
        0.0f,
        1.0f
    );
    Result.HorizonRainShadow = FMath::Clamp(
        TerrainContext.bHasTopographicClimateSample ? TopographicClimate.RainShadow : 0.0f,
        0.0f,
        1.0f
    );
    Result.SolarOcclusion = FMath::Clamp(
        TerrainContext.bHasTopographicClimateSample ? TopographicClimate.SolarOcclusion : 0.0f,
        0.0f,
        1.0f
    );
    Result.SkyViewFactor = FMath::Clamp(
        TerrainContext.bHasTopographicClimateSample ? TopographicClimate.SkyViewFactor : 1.0f,
        0.0f,
        1.0f
    );

    const float AspectSolarExposure = FMath::Clamp(
        0.50f + SolarFacing * 0.36f * SlopeAspectStrength + (1.0f - TopographicExposure) * 0.08f,
        0.0f,
        1.0f
    );
    Result.SolarExposure = FMath::Clamp(
        AspectSolarExposure * FMath::Lerp(1.0f, 0.42f, Result.SolarOcclusion) *
        FMath::Lerp(0.88f, 1.04f, Result.SkyViewFactor),
        0.0f,
        1.0f
    );
    Result.WindExposure = FMath::Clamp(
        TopographicExposure * FMath::Lerp(0.62f, 1.16f, Windwardness) *
        FMath::Lerp(0.82f, 1.08f, Result.SkyViewFactor),
        0.0f,
        1.0f
    );
    Result.WindShelter = 1.0f - Result.WindExposure;
    const float LocalRainShadow = FMath::Clamp(
        (1.0f - Windwardness) * (Ecology.FoothillWeight * 0.34f + Ecology.MountainCore * 0.58f),
        0.0f,
        1.0f
    );
    Result.RainShadow = FMath::Clamp(
        LocalRainShadow * 0.48f + Result.HorizonRainShadow * 0.78f,
        0.0f,
        1.0f
    );
    Result.Exposure = FMath::Clamp(
        Result.WindExposure * 0.68f + Result.SolarExposure * 0.08f +
        Result.RockExposure * 0.16f + Result.SkyViewFactor * 0.08f,
        0.0f,
        1.0f
    );
'''
replace_once(biome_cpp, old_exposure, new_exposure)

replace_once(
    biome_cpp,
    '''    const float OrographicMoisture = Windwardness * Ecology.FoothillWeight * 0.13f;
    const float DrainageMoisture = Result.FlowConvergence * 0.12f + Result.RiverInfluence * 0.24f;
''',
    '''    const float OrographicMoisture = Result.OrographicLift * FMath::Lerp(0.08f, 0.22f, Climate.MoistureTransport);
    const float RegionalTransportMoisture = (Climate.MoistureTransport - 0.5f) * 0.18f;
    const float ContinentalDrying = (Climate.Continentality - 0.5f) * 0.10f;
    const float DrainageMoisture = Result.FlowConvergence * 0.12f + Result.RiverInfluence * 0.24f;
'''
)

replace_once(
    biome_cpp,
    '''        Climate.LocalHumidity * 0.08f +
        OrographicMoisture +
        DrainageMoisture +
        Result.ColdAirPooling * 0.055f -
        Result.RainShadow * 0.16f -
        ExposureDrying,
''',
    '''        Climate.LocalHumidity * 0.08f +
        RegionalTransportMoisture +
        OrographicMoisture +
        DrainageMoisture +
        Result.ColdAirPooling * 0.055f -
        ContinentalDrying -
        Result.RainShadow * FMath::Lerp(0.14f, 0.26f, Climate.Continentality) -
        ExposureDrying,
'''
)

replace_once(
    biome_cpp,
    '''    const float SolarTemperature = (Result.SolarExposure - 0.5f) * 0.10f;
    Result.Temperature = FMath::Clamp(
''',
    '''    const float ContinentalThermalContrast = FMath::Lerp(0.78f, 1.30f, Climate.Continentality);
    const float SolarTemperature = (Result.SolarExposure - 0.5f) * 0.10f * ContinentalThermalContrast;
    Result.Temperature = FMath::Clamp(
'''
)

replace_once(
    biome_cpp,
    '''        SolarTemperature -
        Result.WindExposure * 0.035f -
        Result.ColdAirPooling * 0.11f,
''',
    '''        SolarTemperature -
        Result.WindExposure * 0.035f -
        Result.ColdAirPooling * 0.11f * ContinentalThermalContrast,
'''
)

replace_once(
    biome_cpp,
    '''                RangeSuitability(Result.SolarExposure, Definition.MinimumSolarExposure, Definition.MaximumSolarExposure, Softness) *
                RangeSuitability(Result.Fertility, Definition.MinimumFertility, Definition.MaximumFertility, Softness) *
''',
    '''                RangeSuitability(Result.SolarExposure, Definition.MinimumSolarExposure, Definition.MaximumSolarExposure, Softness) *
                RangeSuitability(Result.OrographicLift, Definition.MinimumOrographicLift, Definition.MaximumOrographicLift, Softness) *
                RangeSuitability(Result.RainShadow, Definition.MinimumRainShadow, Definition.MaximumRainShadow, Softness) *
                RangeSuitability(Result.SolarOcclusion, Definition.MinimumSolarOcclusion, Definition.MaximumSolarOcclusion, Softness) *
                RangeSuitability(Result.Fertility, Definition.MinimumFertility, Definition.MaximumFertility, Softness) *
'''
)

# New topography-sensitive communities before the geology-specific communities.
replace_once(
    biome_cpp,
    '''        AddBuiltIn(
  TEXT("DeepColluvialForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
''',
    '''        AddBuiltIn(
  TEXT("WindwardMontaneForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * FMath::Max(FoothillBand * 0.72f, MontaneBand) * Result.TreeLineWeight *
      Result.OrographicLift * RangeSuitability(Result.SoilMoisture, 0.34f, 0.94f, 0.12f) *
      FMath::Lerp(0.52f, 1.10f, Result.SoilDepth) *
      FMath::Lerp(0.62f, 1.08f, Result.CanopyPotential) *
      (1.0f - Result.RainShadow * 0.72f),
  0, 0.30f
        );
        AddBuiltIn(
  TEXT("ShadedRavineForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * FMath::Max(Result.ValleyFloorInfluence, Result.LowerSlopeInfluence * 0.82f) *
      Result.TreeLineWeight * FMath::Lerp(0.32f, 1.0f, Result.SolarOcclusion) *
      FMath::Lerp(0.52f, 1.06f, Result.SoilDepth) *
      FMath::Lerp(0.50f, 1.08f, Result.WaterHoldingCapacity) *
      (1.0f - Result.Disturbance * 0.42f),
  1, 0.24f
        );
        AddBuiltIn(
  TEXT("LeewardSteppe"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
  FMath::Max(LowlandBand, FoothillBand) * GentleTerrain * Result.RainShadow * Result.CanopyOpenness *
      RangeSuitability(Result.SoilMoisture, 0.06f, 0.46f, 0.12f) *
      FMath::Lerp(0.68f, 1.08f, Result.SolarExposure) *
      FMath::Lerp(0.72f, 1.08f, Result.SoilCoarseness) *
      (1.0f - Result.WetlandWeight),
  2, 0.34f
        );
        AddBuiltIn(
  TEXT("DeepColluvialForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
'''
)

# -----------------------------------------------------------------------------
# Density-field optimisation and cached long-range horizon model.
# -----------------------------------------------------------------------------
density_h = "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h"
replace_once(
    density_h,
    '''    static constexpr float CoordinateCacheScale = 20.0f;
    static constexpr float BiomeClimateCacheCellSize = 4.0f;

    mutable TMap<FIntPoint, FSurfaceData> SurfaceCache;
    mutable TMap<FIntPoint, FColumnData> ColumnCache;
    mutable TMap<FIntPoint, FCubusBiomeClimateContext> BiomeClimateCache;
''',
    '''    static constexpr float CoordinateCacheScale = 20.0f;
    // Climate noise is extremely low-frequency; 8 voxels preserves its shape while
    // reducing FBM lattice nodes substantially versus the previous 4-voxel grid.
    static constexpr float BiomeClimateCacheCellSize = 8.0f;
    // Long-range horizon work is much more expensive, so it is evaluated sparsely.
    static constexpr float BiomeTopographicClimateCacheCellSize = 16.0f;
    static constexpr float BiomeMacroHeightCacheScale = 4.0f;

    mutable TMap<FIntPoint, FSurfaceData> SurfaceCache;
    mutable TMap<FIntPoint, FColumnData> ColumnCache;
    mutable TMap<FIntPoint, FCubusBiomeClimateContext> BiomeClimateCache;
    mutable TMap<FIntPoint, FCubusBiomeTopographicClimateContext> BiomeTopographicClimateCache;
    mutable TMap<FIntPoint, float> BiomeMacroHeightCache;
'''
)

replace_once(
    density_h,
    '''    const FCubusBiomeClimateContext& GetCachedBiomeClimateCell(const FIntPoint& CellCoordinate) const;
    FCubusBiomeClimateContext GetInterpolatedBiomeClimate(float WorldX, float WorldY) const;

    FTerrainRegionWeights SampleTerrainRegions(float WorldX, float WorldY) const;
''',
    '''    const FCubusBiomeClimateContext& GetCachedBiomeClimateCell(const FIntPoint& CellCoordinate) const;
    FCubusBiomeClimateContext GetInterpolatedBiomeClimate(float WorldX, float WorldY) const;
    float GetCachedBiomeMacroHeight(float WorldX, float WorldY) const;
    const FCubusBiomeTopographicClimateContext& GetCachedBiomeTopographicClimateCell(const FIntPoint& CellCoordinate) const;
    FCubusBiomeTopographicClimateContext GetInterpolatedBiomeTopographicClimate(float WorldX, float WorldY) const;

    FTerrainRegionWeights SampleTerrainRegions(float WorldX, float WorldY) const;
'''
)

density_cpp = "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp"
replace_once(
    density_cpp,
    '''\tSurfaceCache.Reserve(1600);
\tColumnCache.Reserve(1600);
\tBiomeClimateCache.Reserve(256);
''',
    '''\tSurfaceCache.Reserve(1600);
\tColumnCache.Reserve(1600);
\tBiomeClimateCache.Reserve(96);
\tBiomeTopographicClimateCache.Reserve(32);
\tBiomeMacroHeightCache.Reserve(384);
'''
)

replace_once(
    density_cpp,
    '''\tBiomeTerrainContext.bHasSubstrateSample = true;
\tBiomeTerrainContext.SubstrateHardness = Column.RockHardness;
\tBiomeTerrainContext.FractureDensity = Column.Fracture;

\tconst FCubusBiomeClimateContext Climate = GetInterpolatedBiomeClimate(WorldX, WorldY);
''',
    '''\tBiomeTerrainContext.bHasSubstrateSample = true;
\tBiomeTerrainContext.SubstrateHardness = Column.RockHardness;
\tBiomeTerrainContext.FractureDensity = Column.Fracture;
\tBiomeTerrainContext.bHasTopographicClimateSample = true;
\tBiomeTerrainContext.TopographicClimateSample = GetInterpolatedBiomeTopographicClimate(WorldX, WorldY);

\tconst FCubusBiomeClimateContext Climate = GetInterpolatedBiomeClimate(WorldX, WorldY);
'''
)

# Insert horizon cache methods after climate interpolation.
p = Path(density_cpp)
text = p.read_text(encoding="utf-8")
marker = "FCubusTerrainDensityField::FTerrainRegionWeights FCubusTerrainDensityField::SampleTerrainRegions("
if text.count(marker) != 1:
    raise RuntimeError("missing density topographic-climate insertion point")
methods = r'''float FCubusTerrainDensityField::GetCachedBiomeMacroHeight(const float WorldX, const float WorldY) const
{
    const FIntPoint Key(
        FMath::RoundToInt(WorldX * BiomeMacroHeightCacheScale),
        FMath::RoundToInt(WorldY * BiomeMacroHeightCacheScale)
    );
    if (const float* Existing = BiomeMacroHeightCache.Find(Key))
    {
        return *Existing;
    }

    const float QuantizedWorldX = static_cast<float>(Key.X) / BiomeMacroHeightCacheScale;
    const float QuantizedWorldY = static_cast<float>(Key.Y) / BiomeMacroHeightCacheScale;
    const float TerrainX = QuantizedWorldX + static_cast<float>(Settings.TerrainOffsetX);
    const float TerrainY = QuantizedWorldY + static_cast<float>(Settings.TerrainOffsetY);

    /*
     * Horizon climate intentionally uses macro terrain only. River cuts and local
     * landmark relief should not create regional rain shadows or solar horizons.
     */
    const float Height = Settings.bUseHeightTerrain
        ? FCubusTerrainForm::Sample(TerrainX, TerrainY, TerrainFormSettings).Height
        : Settings.FlatSurfaceWorldZ;
    BiomeMacroHeightCache.Add(Key, Height);
    return Height;
}

const FCubusBiomeTopographicClimateContext& FCubusTerrainDensityField::GetCachedBiomeTopographicClimateCell(
    const FIntPoint& CellCoordinate
) const
{
    if (const FCubusBiomeTopographicClimateContext* Existing = BiomeTopographicClimateCache.Find(CellCoordinate))
    {
        return *Existing;
    }

    const float WorldX = static_cast<float>(CellCoordinate.X) * BiomeTopographicClimateCacheCellSize;
    const float WorldY = static_cast<float>(CellCoordinate.Y) * BiomeTopographicClimateCacheCellSize;
    const float LocalHeight = GetCachedBiomeMacroHeight(WorldX, WorldY);

    FVector2D Wind = Settings.BiomeSettings.PrevailingWindDirection;
    if (Wind.SizeSquared() <= KINDA_SMALL_NUMBER)
    {
        Wind = FVector2D(0.82f, 0.57f);
    }
    Wind.Normalize();

    FVector2D Solar = Settings.BiomeSettings.SolarDirection;
    if (Solar.SizeSquared() <= KINDA_SMALL_NUMBER)
    {
        Solar = FVector2D(-0.42f, -0.91f);
    }
    Solar.Normalize();

    FCubusBiomeTopographicClimateContext Result;
    constexpr float Distances[] = {24.0f, 64.0f, 128.0f};
    constexpr float Weights[] = {0.46f, 0.34f, 0.20f};

    float Lift = 0.0f;
    float RainBarrier = 0.0f;
    float SolarBarrier = 0.0f;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Distances); ++Index)
    {
        const float Distance = Distances[Index];
        const float Weight = Weights[Index];

        const float UpwindHeight = GetCachedBiomeMacroHeight(
            WorldX - Wind.X * Distance,
            WorldY - Wind.Y * Distance
        );
        const float RelativeLift = FMath::Clamp(
            (LocalHeight - UpwindHeight) / FMath::Max(8.0f, Distance * 0.34f),
            0.0f,
            1.0f
        );
        Lift += RelativeLift * Weight;
        RainBarrier = FMath::Max(
            RainBarrier,
            FMath::Clamp((UpwindHeight - LocalHeight) / FMath::Max(8.0f, Distance * 0.28f), 0.0f, 1.0f) *
            FMath::Lerp(1.0f, 0.72f, static_cast<float>(Index) / 2.0f)
        );

        const float SolarHeight = GetCachedBiomeMacroHeight(
            WorldX + Solar.X * Distance,
            WorldY + Solar.Y * Distance
        );
        SolarBarrier = FMath::Max(
            SolarBarrier,
            FMath::Clamp((SolarHeight - LocalHeight) / FMath::Max(8.0f, Distance * 0.24f), 0.0f, 1.0f) *
            FMath::Lerp(1.0f, 0.74f, static_cast<float>(Index) / 2.0f)
        );
    }

    constexpr FVector2D SkyDirections[] = {
        FVector2D(1.0f, 0.0f), FVector2D(-1.0f, 0.0f),
        FVector2D(0.0f, 1.0f), FVector2D(0.0f, -1.0f),
        FVector2D(0.70710678f, 0.70710678f), FVector2D(-0.70710678f, 0.70710678f),
        FVector2D(0.70710678f, -0.70710678f), FVector2D(-0.70710678f, -0.70710678f)
    };
    constexpr float SkyDistance = 48.0f;
    float HorizonClosure = 0.0f;
    for (const FVector2D& Direction : SkyDirections)
    {
        const float HorizonHeight = GetCachedBiomeMacroHeight(
            WorldX + Direction.X * SkyDistance,
            WorldY + Direction.Y * SkyDistance
        );
        HorizonClosure += FMath::Clamp(
            (HorizonHeight - LocalHeight) / (SkyDistance * 0.32f),
            0.0f,
            1.0f
        );
    }

    Result.OrographicLift = FMath::Clamp(Lift, 0.0f, 1.0f);
    Result.RainShadow = FMath::Clamp(RainBarrier, 0.0f, 1.0f);
    Result.SolarOcclusion = FMath::Clamp(SolarBarrier, 0.0f, 1.0f);
    Result.SkyViewFactor = FMath::Clamp(1.0f - HorizonClosure / static_cast<float>(UE_ARRAY_COUNT(SkyDirections)), 0.0f, 1.0f);

    BiomeTopographicClimateCache.Add(CellCoordinate, Result);
    return BiomeTopographicClimateCache.FindChecked(CellCoordinate);
}

FCubusBiomeTopographicClimateContext FCubusTerrainDensityField::GetInterpolatedBiomeTopographicClimate(
    const float WorldX,
    const float WorldY
) const
{
    const float GridX = WorldX / BiomeTopographicClimateCacheCellSize;
    const float GridY = WorldY / BiomeTopographicClimateCacheCellSize;
    const int32 CellX = FMath::FloorToInt(GridX);
    const int32 CellY = FMath::FloorToInt(GridY);
    const float AlphaX = GridX - static_cast<float>(CellX);
    const float AlphaY = GridY - static_cast<float>(CellY);

    const FCubusBiomeTopographicClimateContext C00 = GetCachedBiomeTopographicClimateCell(FIntPoint(CellX, CellY));
    const FCubusBiomeTopographicClimateContext C10 = GetCachedBiomeTopographicClimateCell(FIntPoint(CellX + 1, CellY));
    const FCubusBiomeTopographicClimateContext C01 = GetCachedBiomeTopographicClimateCell(FIntPoint(CellX, CellY + 1));
    const FCubusBiomeTopographicClimateContext C11 = GetCachedBiomeTopographicClimateCell(FIntPoint(CellX + 1, CellY + 1));

    auto LerpContext = [](const FCubusBiomeTopographicClimateContext& A,
                          const FCubusBiomeTopographicClimateContext& B,
                          const float Alpha)
    {
        FCubusBiomeTopographicClimateContext Result;
        Result.OrographicLift = FMath::Lerp(A.OrographicLift, B.OrographicLift, Alpha);
        Result.RainShadow = FMath::Lerp(A.RainShadow, B.RainShadow, Alpha);
        Result.SolarOcclusion = FMath::Lerp(A.SolarOcclusion, B.SolarOcclusion, Alpha);
        Result.SkyViewFactor = FMath::Lerp(A.SkyViewFactor, B.SkyViewFactor, Alpha);
        return Result;
    };

    const FCubusBiomeTopographicClimateContext Bottom = LerpContext(C00, C10, AlphaX);
    const FCubusBiomeTopographicClimateContext Top = LerpContext(C01, C11, AlphaX);
    return LerpContext(Bottom, Top, AlphaY);
}

'''
p.write_text(text.replace(marker, methods + marker, 1), encoding="utf-8")

# -----------------------------------------------------------------------------
# Generation version: both biome output and density cache identity change.
# -----------------------------------------------------------------------------
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h",
    '''    // Bumped to 17: biomes now reuse authoritative terrain/hydrology, interpolate
    // slow climate fields, consume real substrate geology and retain the richer
    // disturbance/canopy/substrate habitat through vegetation species selection.
    static constexpr uint32 CurrentGenerationVersion = 17;
''',
    '''    // Bumped to 18: climate provinces and cached long-range terrain horizons now
    // drive windward moisture, rain shadow and solar occlusion; slow climate
    // interpolation also moves to an 8-voxel lattice for cheaper cold generation.
    static constexpr uint32 CurrentGenerationVersion = 18;
'''
)

# -----------------------------------------------------------------------------
# Regression tests for province interpolation and horizon-driven ecology.
# -----------------------------------------------------------------------------
biome_tests = Path("Source/Orakai/CubusCore/Tests/CubusBiomeFieldTests.cpp")
text = biome_tests.read_text(encoding="utf-8")
marker = "IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n    FCubusBiomeEcologicalNicheTest,"
if text.count(marker) != 1:
    raise RuntimeError("missing biome test insertion marker")
new_test = r'''IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCubusBiomeRegionalTopographicClimateTest,
    "Orakai.Cubus.Generation.BiomeRegionalTopographicClimate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FCubusBiomeRegionalTopographicClimateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FCubusBiomeFieldSettings Settings;
    Settings.bEnabled = true;
    Settings.BiomeOffsetX = 8123;
    Settings.BiomeOffsetY = -3491;

    const FCubusBiomeClimateContext A = FCubusBiomeField::SampleClimate(64.0f, 96.0f, Settings);
    const FCubusBiomeClimateContext B = FCubusBiomeField::SampleClimate(72.0f, 96.0f, Settings);
    const FCubusBiomeClimateContext Mid = FCubusBiomeField::LerpClimate(A, B, 0.5f);
    TestTrue(TEXT("Climate moisture transport remains normalized"), Mid.MoistureTransport >= 0.0f && Mid.MoistureTransport <= 1.0f);
    TestTrue(TEXT("Climate continentality remains normalized"), Mid.Continentality >= 0.0f && Mid.Continentality <= 1.0f);

    FCubusBiomeTerrainContext OpenSlope;
    OpenSlope.Gradient = FVector2D(0.40f, 0.0f);
    OpenSlope.FoothillWeight = 0.75f;
    OpenSlope.bHasTopographicClimateSample = true;
    OpenSlope.TopographicClimateSample.OrographicLift = 0.72f;
    OpenSlope.TopographicClimateSample.RainShadow = 0.04f;
    OpenSlope.TopographicClimateSample.SolarOcclusion = 0.02f;
    OpenSlope.TopographicClimateSample.SkyViewFactor = 0.94f;

    FCubusBiomeTerrainContext LeeRavine = OpenSlope;
    LeeRavine.TopographicClimateSample.OrographicLift = 0.06f;
    LeeRavine.TopographicClimateSample.RainShadow = 0.78f;
    LeeRavine.TopographicClimateSample.SolarOcclusion = 0.72f;
    LeeRavine.TopographicClimateSample.SkyViewFactor = 0.34f;

    const FCubusBiomeSample Windward = FCubusBiomeField::Sample(64.0f, 96.0f, 28.0f, 0.40f, Settings, OpenSlope, &Mid);
    const FCubusBiomeSample Leeward = FCubusBiomeField::Sample(64.0f, 96.0f, 28.0f, 0.40f, Settings, LeeRavine, &Mid);

    TestTrue(TEXT("Windward horizon lift increases moisture"), Windward.Moisture > Leeward.Moisture);
    TestTrue(TEXT("Terrain horizon can suppress solar exposure"), Leeward.SolarExposure < Windward.SolarExposure);
    TestTrue(TEXT("Horizon rain shadow reaches the final biome sample"), Leeward.RainShadow > Windward.RainShadow);
    TestTrue(TEXT("Climate province is deterministic"), Windward.ClimateProvince == FCubusBiomeField::Sample(64.0f, 96.0f, 28.0f, 0.40f, Settings, OpenSlope, &Mid).ClimateProvince);
    return true;
}

'''
biome_tests.write_text(text.replace(marker, new_test + marker, 1), encoding="utf-8")

# -----------------------------------------------------------------------------
# Static guards against partial application.
# -----------------------------------------------------------------------------
require("Source/Orakai/CubusCore/Data/CubusBiomeTypes.h", "enum class ECubusClimateProvince", 1)
require("Source/Orakai/CubusCore/Generation/CubusBiomeField.h", "FCubusBiomeTopographicClimateContext", 2)
require(biome_cpp, "ResolveClimateProvince", 2)
require(biome_cpp, "TEXT(\"WindwardMontaneForest\")", 1)
require(biome_cpp, "TEXT(\"ShadedRavineForest\")", 1)
require(biome_cpp, "TEXT(\"LeewardSteppe\")", 1)
require(density_h, "BiomeClimateCacheCellSize = 8.0f", 1)
require(density_h, "BiomeTopographicClimateCacheCellSize = 16.0f", 1)
require(density_cpp, "GetCachedBiomeTopographicClimateCell", 6)
require(density_cpp, "GetCachedBiomeMacroHeight", 5)
require("Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h", "CurrentGenerationVersion = 18", 1)
