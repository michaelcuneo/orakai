#include "CubusCore/Generation/CubusTerrainDensityField.h"

FCubusTerrainDensityField::FCubusTerrainDensityField(const FCubusTerrainDensitySettings& InSettings) : Settings(InSettings)
{
	Settings.ContinentAmplitude = FMath::Max(0.0f, Settings.ContinentAmplitude);
	Settings.ContinentFrequency = FMath::Max(0.000001f, Settings.ContinentFrequency);
	Settings.HillAmplitude = FMath::Max(0.0f, Settings.HillAmplitude);
	Settings.HillFrequency = FMath::Max(0.000001f, Settings.HillFrequency);
	Settings.DetailAmplitude = FMath::Max(0.0f, Settings.DetailAmplitude);
	Settings.DetailFrequency = FMath::Max(0.000001f, Settings.DetailFrequency);
	Settings.RidgeAmplitude = FMath::Max(0.0f, Settings.RidgeAmplitude);
	Settings.RidgeFrequency = FMath::Max(0.000001f, Settings.RidgeFrequency);
	Settings.ValleyDepth = FMath::Max(0.0f, Settings.ValleyDepth);
	Settings.ValleyFrequency = FMath::Max(0.000001f, Settings.ValleyFrequency);
	Settings.ValleyWidth = FMath::Clamp(Settings.ValleyWidth, 0.0f, 1.0f);
	Settings.ValleyFalloff = FMath::Clamp(Settings.ValleyFalloff, 0.001f, 1.0f);
	Settings.ValleyWarpAmplitude = FMath::Max(0.0f, Settings.ValleyWarpAmplitude);
	Settings.ValleyWarpFrequency = FMath::Max(0.000001f, Settings.ValleyWarpFrequency);
	Settings.RegionFrequency = FMath::Max(0.000001f, Settings.RegionFrequency);
	Settings.PlainsThreshold = FMath::Clamp(Settings.PlainsThreshold, -1.0f, 1.0f);
	Settings.PlainsBlend = FMath::Clamp(Settings.PlainsBlend, 0.001f, 1.0f);
	Settings.MountainThreshold = FMath::Clamp(Settings.MountainThreshold, Settings.PlainsThreshold, 1.0f);
	Settings.MountainBlend = FMath::Clamp(Settings.MountainBlend, 0.001f, 1.0f);

	Settings.GeologySurfaceBand = FMath::Max(1.0f, Settings.GeologySurfaceBand);
	Settings.GeologyCliffSlopeStart = FMath::Max(0.0f, Settings.GeologyCliffSlopeStart);
	Settings.GeologyCliffSlopeFull = FMath::Max(Settings.GeologyCliffSlopeStart + 0.01f, Settings.GeologyCliffSlopeFull);
	Settings.GeologyStrataFrequency = FMath::Max(0.000001f, Settings.GeologyStrataFrequency);
	Settings.GeologyShelfStrength = FMath::Max(0.0f, Settings.GeologyShelfStrength);
	Settings.GeologyUndercutStrength = FMath::Max(0.0f, Settings.GeologyUndercutStrength);
	Settings.GeologyRockWarpFrequency = FMath::Max(0.000001f, Settings.GeologyRockWarpFrequency);
	Settings.GeologyRockWarpStrength = FMath::Max(0.0f, Settings.GeologyRockWarpStrength);

	Settings.RiverFrequency = FMath::Max(0.000001f, Settings.RiverFrequency);
	Settings.RiverChannelWidth = FMath::Clamp(Settings.RiverChannelWidth, 0.0f, 1.0f);
	Settings.RiverValleyWidth = FMath::Clamp(FMath::Max(Settings.RiverChannelWidth + 0.0001f, Settings.RiverValleyWidth), 0.0001f, 1.0f);
	Settings.RiverValleyDepth = FMath::Max(0.0f, Settings.RiverValleyDepth);
	Settings.RiverChannelDepth = FMath::Max(0.0f, Settings.RiverChannelDepth);
	Settings.RiverWarpAmplitude = FMath::Max(0.0f, Settings.RiverWarpAmplitude);
	Settings.RiverWarpFrequency = FMath::Max(0.000001f, Settings.RiverWarpFrequency);

	if (Settings.CaveMinimumWorldZ > Settings.CaveMaximumWorldZ)
	{
		Swap(Settings.CaveMinimumWorldZ, Settings.CaveMaximumWorldZ);
	}

	Settings.CaveSurfaceClearance = FMath::Max(1, Settings.CaveSurfaceClearance);
	Settings.CavePrimaryFrequency = FMath::Max(0.000001f, Settings.CavePrimaryFrequency);
	Settings.CaveSecondaryFrequency = FMath::Max(0.000001f, Settings.CaveSecondaryFrequency);
	Settings.CaveThreshold = FMath::Clamp(Settings.CaveThreshold, 0.0f, 1.0f);
	Settings.CaveSurfaceSharpness = FMath::Max(0.001f, Settings.CaveSurfaceSharpness);

	Settings.SurfaceMaterialId = FMath::Max(1, Settings.SurfaceMaterialId);
	Settings.SubsurfaceMaterialId = FMath::Max(1, Settings.SubsurfaceMaterialId);
	Settings.RockMaterialId = FMath::Max(1, Settings.RockMaterialId);
	Settings.SnowMaterialId = FMath::Max(1, Settings.SnowMaterialId);
	Settings.RockSlopeThreshold = FMath::Max(0.0f, Settings.RockSlopeThreshold);
	Settings.SurfaceMaterialDepth = FMath::Max(0.01f, Settings.SurfaceMaterialDepth);
	Settings.RockMaterialDepth = FMath::Max(Settings.SurfaceMaterialDepth, Settings.RockMaterialDepth);

	TerrainFormSettings.BaseHeight = Settings.BaseHeight;
	TerrainFormSettings.ContinentAmplitude = Settings.ContinentAmplitude;
	TerrainFormSettings.ContinentFrequency = Settings.ContinentFrequency;
	TerrainFormSettings.HillAmplitude = Settings.HillAmplitude;
	TerrainFormSettings.HillFrequency = Settings.HillFrequency;
	TerrainFormSettings.DetailAmplitude = Settings.DetailAmplitude;
	TerrainFormSettings.DetailFrequency = Settings.DetailFrequency;
	TerrainFormSettings.RidgeAmplitude = Settings.RidgeAmplitude;
	TerrainFormSettings.RidgeFrequency = Settings.RidgeFrequency;
	TerrainFormSettings.ValleyDepth = Settings.ValleyDepth;
	TerrainFormSettings.ValleyFrequency = Settings.ValleyFrequency;
	TerrainFormSettings.ValleyWidth = Settings.ValleyWidth;
	TerrainFormSettings.ValleyFalloff = Settings.ValleyFalloff;
	TerrainFormSettings.ValleyWarpAmplitude = Settings.ValleyWarpAmplitude;
	TerrainFormSettings.ValleyWarpFrequency = Settings.ValleyWarpFrequency;
	TerrainFormSettings.RegionFrequency = Settings.RegionFrequency;
	TerrainFormSettings.PlainsThreshold = Settings.PlainsThreshold;
	TerrainFormSettings.PlainsBlend = Settings.PlainsBlend;
	TerrainFormSettings.MountainThreshold = Settings.MountainThreshold;
	TerrainFormSettings.MountainBlend = Settings.MountainBlend;

	HydrologySettings.bEnabled = Settings.bGenerateRivers && Settings.bUseHeightTerrain;
	HydrologySettings.TerrainFormSettings = TerrainFormSettings;
	HydrologySettings.TerrainOffsetX = Settings.TerrainOffsetX;
	HydrologySettings.TerrainOffsetY = Settings.TerrainOffsetY;
	HydrologySettings.RiverSeed = Settings.RiverSeed;
	HydrologySettings.SeaLevel = Settings.BaseHeight;
	HydrologySettings.ValleyDepth = Settings.RiverValleyDepth;
	HydrologySettings.ChannelDepth = Settings.RiverChannelDepth;
	HydrologySettings.ChannelHalfWidth = FMath::Max(3.0f, Settings.RiverChannelWidth * 96.0f);
	HydrologySettings.ValleyHalfWidth = FMath::Max(
		HydrologySettings.ChannelHalfWidth + 8.0f,
		Settings.RiverValleyWidth * 160.0f
	);
	Settings.BiomeSettings.HydrologySettings = HydrologySettings;
	Settings.BiomeSettings.bGenerateRivers = HydrologySettings.bEnabled;

	SurfaceCache.Reserve(1600);
	ColumnCache.Reserve(1600);
}

FCubusDensitySample FCubusTerrainDensityField::Sample(const FIntVector& GlobalSampleCoordinate) const
{
	return SampleContinuous(FVector(static_cast<double>(GlobalSampleCoordinate.X), static_cast<double>(GlobalSampleCoordinate.Y), static_cast<double>(GlobalSampleCoordinate.Z)));
}

FCubusDensitySample FCubusTerrainDensityField::SampleContinuous(const FVector& GlobalSampleCoordinate) const
{
	const FColumnData& Column = GetColumnData(static_cast<float>(GlobalSampleCoordinate.X), static_cast<float>(GlobalSampleCoordinate.Y));
	const float BaseTerrainDensity = Column.SurfaceSampleZ - static_cast<float>(GlobalSampleCoordinate.Z);
	const float TerrainDensity = Settings.bGenerateVolumetricGeology
		? SampleGeologicalDensity(GlobalSampleCoordinate, Column, BaseTerrainDensity)
		: BaseTerrainDensity;

	float CompositeDensity = TerrainDensity;
	if (Settings.bGenerateCaves)
	{
		CompositeDensity = FMath::Min(CompositeDensity, SampleCaveDensity(GlobalSampleCoordinate, Column.SurfaceVoxelHeight, Column.Slope));
	}

	FCubusDensitySample Result;
	Result.Density = CompositeDensity;
	if (CompositeDensity <= 0.0f)
	{
		Result.MaterialId = 0;
		return Result;
	}

	if (BaseTerrainDensity <= 0.0f && TerrainDensity > 0.0f)
	{
		Result.MaterialId = Settings.BiomeSettings.bEnabled ? Settings.BiomeRockMaterialId : Settings.RockMaterialId;
		return Result;
	}

	const float DepthBelowSurface = Column.SurfaceSampleZ - static_cast<float>(GlobalSampleCoordinate.Z);
	if (DepthBelowSurface <= Settings.SurfaceMaterialDepth)
	{
		Result.MaterialId = Column.SurfaceMaterialId;
	}
	else if (DepthBelowSurface >= Settings.RockMaterialDepth)
	{
		Result.MaterialId = Settings.BiomeSettings.bEnabled ? Settings.BiomeRockMaterialId : Settings.RockMaterialId;
	}
	else
	{
		Result.MaterialId = Settings.BiomeSettings.bEnabled ? Settings.BiomeSubsurfaceMaterialId : Settings.SubsurfaceMaterialId;
	}

	return Result;
}

float FCubusTerrainDensityField::SampleSurfaceVoxelHeight(const float WorldX, const float WorldY) const
{
	if (!Settings.bUseHeightTerrain)
	{
		return Settings.FlatSurfaceWorldZ;
	}

	const float TerrainX = WorldX + static_cast<float>(Settings.TerrainOffsetX);
	const float TerrainY = WorldY + static_cast<float>(Settings.TerrainOffsetY);
	const float BaseSurfaceHeight = FCubusTerrainForm::Sample(TerrainX, TerrainY, TerrainFormSettings).Height;
	const FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(TerrainX, TerrainY, Settings.LandmarkSettings);
	return ApplyRiverLowering(BaseSurfaceHeight + LandmarkSample.HeightOffset, WorldX, WorldY);
}

FIntPoint FCubusTerrainDensityField::MakeCoordinateCacheKey(const float WorldX, const float WorldY)
{
	return FIntPoint(FMath::RoundToInt(WorldX * CoordinateCacheScale), FMath::RoundToInt(WorldY * CoordinateCacheScale));
}

const FCubusTerrainDensityField::FSurfaceData& FCubusTerrainDensityField::GetCachedSurfaceData(const float WorldSampleX, const float WorldSampleY) const
{
	const FIntPoint Key = MakeCoordinateCacheKey(WorldSampleX, WorldSampleY);
	if (const FSurfaceData* ExistingSurface = SurfaceCache.Find(Key))
	{
		return *ExistingSurface;
	}

	FSurfaceData Surface;
	const float WorldX = WorldSampleX - 0.5f;
	const float WorldY = WorldSampleY - 0.5f;

	if (!Settings.bUseHeightTerrain)
	{
		Surface.SurfaceVoxelHeight = Settings.FlatSurfaceWorldZ;
		Surface.FormSample.Height = Settings.FlatSurfaceWorldZ;
	}
	else
	{
		const float TerrainX = WorldX + static_cast<float>(Settings.TerrainOffsetX);
		const float TerrainY = WorldY + static_cast<float>(Settings.TerrainOffsetY);
		Surface.FormSample = FCubusTerrainForm::Sample(TerrainX, TerrainY, TerrainFormSettings);
		const FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(TerrainX, TerrainY, Settings.LandmarkSettings);
		Surface.SurfaceVoxelHeight = ApplyRiverLowering(Surface.FormSample.Height + LandmarkSample.HeightOffset, WorldX, WorldY);
	}

	SurfaceCache.Add(Key, Surface);
	return SurfaceCache.FindChecked(Key);
}

float FCubusTerrainDensityField::GetCachedSurfaceVoxelHeight(const float WorldSampleX, const float WorldSampleY) const
{
	return GetCachedSurfaceData(WorldSampleX, WorldSampleY).SurfaceVoxelHeight;
}

const FCubusTerrainDensityField::FColumnData& FCubusTerrainDensityField::GetColumnData(const float WorldSampleX, const float WorldSampleY) const
{
	const FIntPoint Key = MakeCoordinateCacheKey(WorldSampleX, WorldSampleY);
	if (const FColumnData* ExistingColumn = ColumnCache.Find(Key))
	{
		return *ExistingColumn;
	}

	FColumnData Column;
	const FSurfaceData& Surface = GetCachedSurfaceData(WorldSampleX, WorldSampleY);
	Column.SurfaceVoxelHeight = Surface.SurfaceVoxelHeight;
	Column.SurfaceSampleZ = Column.SurfaceVoxelHeight + 1.0f;
	Column.FormSample = Surface.FormSample;

	const float HeightPositiveX = GetCachedSurfaceVoxelHeight(WorldSampleX + 1, WorldSampleY);
	const float HeightNegativeX = GetCachedSurfaceVoxelHeight(WorldSampleX - 1, WorldSampleY);
	const float HeightPositiveY = GetCachedSurfaceVoxelHeight(WorldSampleX, WorldSampleY + 1);
	const float HeightNegativeY = GetCachedSurfaceVoxelHeight(WorldSampleX, WorldSampleY - 1);
	const float GradientX = (HeightPositiveX - HeightNegativeX) * 0.5f;
	const float GradientY = (HeightPositiveY - HeightNegativeY) * 0.5f;
	Column.Gradient = FVector2D(GradientX, GradientY);
	Column.Slope = Column.Gradient.Size();

	const float TerrainX = WorldSampleX - 0.5f + static_cast<float>(Settings.TerrainOffsetX);
	const float TerrainY = WorldSampleY - 0.5f + static_cast<float>(Settings.TerrainOffsetY);
	const FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(TerrainX, TerrainY, Settings.LandmarkSettings);

	if (LandmarkSample.IsInside())
	{
		Column.SurfaceMaterialId = FMath::Max(1, Settings.LandmarkSettings.SurfaceMaterialId);
	}
	else if (Column.Slope >= Settings.RockSlopeThreshold)
	{
		Column.SurfaceMaterialId = Settings.RockMaterialId;
	}
	else
	{
		Column.SurfaceMaterialId = Settings.BiomeSettings.bEnabled
			? FCubusBiomeField::Sample(WorldSampleX - 0.5f, WorldSampleY - 0.5f, Column.SurfaceVoxelHeight, Column.Slope, Settings.BiomeSettings).SurfaceMaterialId
			: Settings.SurfaceMaterialId;
		if (Column.SurfaceVoxelHeight >= Settings.SnowMinimumHeight && Column.SurfaceMaterialId == Settings.SurfaceMaterialId)
		{
			Column.SurfaceMaterialId = Settings.SnowMaterialId;
		}
	}

	ColumnCache.Add(Key, Column);
	return ColumnCache.FindChecked(Key);
}

FCubusTerrainDensityField::FTerrainRegionWeights FCubusTerrainDensityField::SampleTerrainRegions(const float WorldX, const float WorldY) const
{
	const float RegionNoise = SampleNoise2D(WorldX + 10427.0f, WorldY - 8633.0f, Settings.RegionFrequency);
	const float PlainsExit = SmoothStep(Settings.PlainsThreshold - Settings.PlainsBlend, Settings.PlainsThreshold + Settings.PlainsBlend, RegionNoise);
	const float MountainEntry = SmoothStep(Settings.MountainThreshold - Settings.MountainBlend, Settings.MountainThreshold + Settings.MountainBlend, RegionNoise);

	FTerrainRegionWeights Result;
	Result.Plains = 1.0f - PlainsExit;
	Result.Mountains = MountainEntry;
	Result.Rolling = FMath::Max(0.0f, 1.0f - Result.Plains - Result.Mountains);
	const float TotalWeight = Result.Plains + Result.Rolling + Result.Mountains;
	if (TotalWeight > KINDA_SMALL_NUMBER)
	{
		Result.Plains /= TotalWeight;
		Result.Rolling /= TotalWeight;
		Result.Mountains /= TotalWeight;
	}
	else
	{
		Result.Plains = 0.0f;
		Result.Rolling = 1.0f;
		Result.Mountains = 0.0f;
	}
	return Result;
}

float FCubusTerrainDensityField::SampleNoise2D(const float WorldX, const float WorldY, const float Frequency) const
{
	const float SafeFrequency = FMath::Max(0.000001f, Frequency);
	return FMath::PerlinNoise2D(FVector2D(WorldX * SafeFrequency, WorldY * SafeFrequency));
}

float FCubusTerrainDensityField::SampleNoise3D(const float WorldX, const float WorldY, const float WorldZ, const float Frequency) const
{
	const float SafeFrequency = FMath::Max(0.000001f, Frequency);
	return FMath::PerlinNoise3D(FVector(WorldX * SafeFrequency, WorldY * SafeFrequency, WorldZ * SafeFrequency));
}

float FCubusTerrainDensityField::SampleRidgedNoise(const float WorldX, const float WorldY, const float Frequency) const
{
	const float NoiseValue = SampleNoise2D(WorldX, WorldY, Frequency);
	const float RidgeValue = 1.0f - FMath::Abs(NoiseValue);
	return RidgeValue * RidgeValue;
}

float FCubusTerrainDensityField::SampleValleyMask(const float WorldX, const float WorldY) const
{
	const float WarpX = SampleNoise2D(WorldX + 4871.0f, WorldY - 3253.0f, Settings.ValleyWarpFrequency) * Settings.ValleyWarpAmplitude;
	const float WarpY = SampleNoise2D(WorldX - 761.0f, WorldY + 5987.0f, Settings.ValleyWarpFrequency) * Settings.ValleyWarpAmplitude;
	const float ValleyNoise = SampleNoise2D(WorldX + WarpX, WorldY + WarpY, Settings.ValleyFrequency);
	const float DistanceFromChannel = FMath::Abs(ValleyNoise);
	const float OuterEdge = FMath::Min(1.0f, Settings.ValleyWidth + Settings.ValleyFalloff);
	if (DistanceFromChannel >= OuterEdge) return 0.0f;
	if (DistanceFromChannel <= Settings.ValleyWidth) return 1.0f;
	const float BlendRange = FMath::Max(0.001f, OuterEdge - Settings.ValleyWidth);
	const float NormalizedDistance = (DistanceFromChannel - Settings.ValleyWidth) / BlendRange;
	const float SmoothDistance = NormalizedDistance * NormalizedDistance * (3.0f - 2.0f * NormalizedDistance);
	return 1.0f - SmoothDistance;
}

float FCubusTerrainDensityField::SampleRiverDistance(const float WorldX, const float WorldY) const
{
	const FCubusHydrologySample Hydrology = FCubusHydrologyField::Sample(WorldX, WorldY, HydrologySettings);
	return FCubusHydrologyField::NormalizeRiverDistance(Hydrology, HydrologySettings);
}

float FCubusTerrainDensityField::ApplyRiverLowering(const float SurfaceHeight, const float WorldX, const float WorldY) const
{
	if (!HydrologySettings.bEnabled)
	{
		return SurfaceHeight;
	}

	const FCubusHydrologySample Hydrology = FCubusHydrologyField::Sample(WorldX, WorldY, HydrologySettings);
	if (!Hydrology.IsChannel())
	{
		return SurfaceHeight;
	}

	const float Strength = 0.20f + Hydrology.ChannelStrength * 0.80f;
	const float ChannelHalfWidth = FMath::Lerp(
		HydrologySettings.ChannelHalfWidth * 0.75f,
		HydrologySettings.ChannelHalfWidth * 1.35f,
		Hydrology.ChannelStrength
	);
	const float ValleyHalfWidth = FMath::Lerp(
		ChannelHalfWidth * 2.5f,
		HydrologySettings.ValleyHalfWidth,
		Hydrology.ChannelStrength
	);

	if (Hydrology.DistanceToChannel >= ValleyHalfWidth)
	{
		return SurfaceHeight;
	}

	const float ValleyInfluence = 1.0f - SmoothStep(
		ChannelHalfWidth,
		ValleyHalfWidth,
		Hydrology.DistanceToChannel
	);
	const float ChannelInfluence = 1.0f - SmoothStep(
		0.0f,
		ChannelHalfWidth * 1.8f,
		Hydrology.DistanceToChannel
	);

	const float ValleyDepth = HydrologySettings.ValleyDepth * Strength;
	const float ChannelDepth = HydrologySettings.ChannelDepth * Strength;
	const float ValleyFloorHeight = Hydrology.HydraulicHeight - ValleyDepth;
	const float ValleyTarget = FMath::Lerp(SurfaceHeight, ValleyFloorHeight, ValleyInfluence);
	const float ChannelFloorHeight = Hydrology.HydraulicHeight - ValleyDepth - ChannelDepth;
	const float ChannelTarget = FMath::Lerp(ValleyTarget, ChannelFloorHeight, ChannelInfluence);

	return FMath::Min(SurfaceHeight, ChannelTarget);
}

float FCubusTerrainDensityField::SampleGeologicalDensity(const FVector& GlobalSampleCoordinate, const FColumnData& Column,
														 const float BaseTerrainDensity) const
{
	const float DistanceFromSurface = FMath::Abs(BaseTerrainDensity);
	if (DistanceFromSurface >= Settings.GeologySurfaceBand)
	{
		return BaseTerrainDensity;
	}

	const float CliffMask = SmoothStep(Settings.GeologyCliffSlopeStart, Settings.GeologyCliffSlopeFull, Column.Slope);
	const float LandformMask = FMath::Clamp(
		Column.FormSample.MountainCore * 0.75f +
		Column.FormSample.FoothillWeight * 0.45f +
		Column.FormSample.Ridge * 0.20f,
		0.0f, 1.0f);
	const float DrainageMask = 1.0f - FMath::Clamp(Column.FormSample.Drainage * Column.FormSample.Drainage, 0.0f, 1.0f);
	const float Exposure = CliffMask * LandformMask * DrainageMask;
	if (Exposure <= KINDA_SMALL_NUMBER)
	{
		return BaseTerrainDensity;
	}

	const float SurfaceBandMask = 1.0f - SmoothStep(0.0f, Settings.GeologySurfaceBand, DistanceFromSurface);
	const float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.TerrainOffsetX);
	const float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.TerrainOffsetY);
	const float WorldZ = static_cast<float>(GlobalSampleCoordinate.Z);

	FVector2D Downhill = -Column.Gradient;
	if (!Downhill.Normalize())
	{
		return BaseTerrainDensity;
	}
	const FVector2D AlongCliff(-Downhill.Y, Downhill.X);
	const float Across = WorldX * Downhill.X + WorldY * Downhill.Y;
	const float Along = WorldX * AlongCliff.X + WorldY * AlongCliff.Y;

	const float RockWarp = SampleNoise3D(
		WorldX + 1709.0f,
		WorldY - 3187.0f,
		WorldZ + 733.0f,
		Settings.GeologyRockWarpFrequency) * Settings.GeologyRockWarpStrength;

	const float StrataPhase =
		WorldZ * Settings.GeologyStrataFrequency +
		Across * Settings.GeologyStrataDip +
		SampleNoise2D(Along + 9113.0f, Across - 4327.0f, Settings.GeologyStrataFrequency * 0.18f) * 0.35f;
	const float StrataWave = FMath::Sin(StrataPhase * 2.0f * PI);
	const float ShelfBand = FMath::Square(FMath::Max(0.0f, StrataWave));
	const float UndercutBand = FMath::Square(FMath::Max(0.0f, -StrataWave));

	const float GeologicalDisplacement =
		Settings.GeologyShelfStrength * ShelfBand -
		Settings.GeologyUndercutStrength * UndercutBand +
		RockWarp;

	return BaseTerrainDensity + GeologicalDisplacement * Exposure * SurfaceBandMask;
}

float FCubusTerrainDensityField::SampleCaveDensity(const FVector& GlobalSampleCoordinate, const float SurfaceVoxelHeight, const float SurfaceSlope) const
{
	const float WorldZ = static_cast<float>(GlobalSampleCoordinate.Z);
	const float VerticalDepth = SurfaceVoxelHeight - WorldZ;
	const float ApproximateShellDepth = VerticalDepth / FMath::Sqrt(1.0f + SurfaceSlope * SurfaceSlope);
	if (WorldZ < Settings.CaveMinimumWorldZ || WorldZ > Settings.CaveMaximumWorldZ ||
		ApproximateShellDepth < static_cast<float>(Settings.CaveSurfaceClearance))
	{
		return MAX_flt;
	}

	const float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.CaveOffsetX);
	const float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.CaveOffsetY);
	const float ShiftedWorldZ = static_cast<float>(GlobalSampleCoordinate.Z) + static_cast<float>(Settings.CaveOffsetZ);
	const float PrimaryNoise = FMath::Abs(SampleNoise3D(WorldX, WorldY, ShiftedWorldZ, Settings.CavePrimaryFrequency));
	const float SecondaryNoise = FMath::Abs(SampleNoise3D(WorldX + 1871.0f, WorldY - 953.0f, ShiftedWorldZ + 421.0f, Settings.CaveSecondaryFrequency));
	return (PrimaryNoise + SecondaryNoise - Settings.CaveThreshold) * Settings.CaveSurfaceSharpness;
}

float FCubusTerrainDensityField::SmoothStep(const float EdgeMinimum, const float EdgeMaximum, const float Value)
{
	if (FMath::IsNearlyEqual(EdgeMinimum, EdgeMaximum))
	{
		return Value >= EdgeMaximum ? 1.0f : 0.0f;
	}
	const float Alpha = FMath::Clamp((Value - EdgeMinimum) / (EdgeMaximum - EdgeMinimum), 0.0f, 1.0f);
	return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}
