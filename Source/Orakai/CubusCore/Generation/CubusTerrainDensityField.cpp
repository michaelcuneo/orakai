#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Generation/CubusWorldScale.h"

namespace CubusTerrainDensityField
{
	uint32 HashCoordinates(const int32 X, const int32 Y, const uint32 Seed)
	{
		uint32 Value = static_cast<uint32>(X) * 0x9E3779B9u;
		Value ^= static_cast<uint32>(Y) * 0x85EBCA6Bu;
		Value ^= Seed * 0xC2B2AE35u;
		Value ^= Value >> 16;
		Value *= 0x7FEB352Du;
		Value ^= Value >> 15;
		Value *= 0x846CA68Bu;
		Value ^= Value >> 16;
		return Value;
	}

	float Hash01(const int32 X, const int32 Y, const uint32 Seed)
	{
		return static_cast<float>(HashCoordinates(X, Y, Seed) & 0x00ffffffu) /
			static_cast<float>(0x00ffffffu);
	}

	uint32 CaveSeed(const FCubusTerrainDensitySettings& Settings)
	{
		uint32 Seed = static_cast<uint32>(Settings.CaveOffsetX) * 0x9E3779B9u;
		Seed ^= static_cast<uint32>(Settings.CaveOffsetY) * 0x85EBCA6Bu;
		Seed ^= static_cast<uint32>(Settings.CaveOffsetZ) * 0xC2B2AE35u;
		return Seed;
	}

	FVector CaveNode(
		const int32 CellX,
		const int32 CellY,
		const FCubusTerrainDensitySettings& Settings,
		const uint32 Seed
	)
	{
		const float CellSize = Settings.CaveNetworkCellSize;
		const float JitterRadius = CellSize * 0.30f;
		const float JitterX = (Hash01(CellX, CellY, Seed ^ 0xA341316Cu) * 2.0f - 1.0f) * JitterRadius;
		const float JitterY = (Hash01(CellX, CellY, Seed ^ 0xC8013EA4u) * 2.0f - 1.0f) * JitterRadius;

		const float MinimumZ = static_cast<float>(Settings.CaveMinimumWorldZ + Settings.CaveOffsetZ);
		const float MaximumZ = static_cast<float>(Settings.CaveMaximumWorldZ + Settings.CaveOffsetZ);
		const float VerticalMargin = FMath::Min(4.0f, FMath::Max(0.0f, (MaximumZ - MinimumZ) * 0.15f));
		const float NodeZ = FMath::Lerp(
			MinimumZ + VerticalMargin,
			MaximumZ - VerticalMargin,
			Hash01(CellX, CellY, Seed ^ 0xAD90777Du)
		);

		return FVector(
			(static_cast<float>(CellX) + 0.5f) * CellSize + JitterX,
			(static_cast<float>(CellY) + 0.5f) * CellSize + JitterY,
			NodeZ
		);
	}

	float DistanceToSegment(const FVector& Point, const FVector& A, const FVector& B)
	{
		const FVector Segment = B - A;
		const double LengthSquared = Segment.SizeSquared();
		if (LengthSquared <= UE_SMALL_NUMBER)
		{
			return static_cast<float>(FVector::Distance(Point, A));
		}

		const double Alpha = FMath::Clamp(
			FVector::DotProduct(Point - A, Segment) / LengthSquared,
			0.0,
			1.0
		);
		return static_cast<float>(FVector::Distance(Point, A + Segment * Alpha));
	}
}

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
	Settings.GeologyHardnessFrequency = FMath::Max(0.000001f, Settings.GeologyHardnessFrequency);
	Settings.GeologyFractureFrequency = FMath::Max(0.000001f, Settings.GeologyFractureFrequency);
	Settings.GeologyFoldFrequency = FMath::Max(0.000001f, Settings.GeologyFoldFrequency);
	Settings.GeologyMassFrequency = FMath::Max(0.000001f, Settings.GeologyMassFrequency);
	Settings.GeologyMassStrength = FMath::Max(0.0f, Settings.GeologyMassStrength);
	Settings.GeologyOverhangStrength = FMath::Max(0.0f, Settings.GeologyOverhangStrength);
	Settings.GeologyFractureStrength = FMath::Max(0.0f, Settings.GeologyFractureStrength);

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
	Settings.CaveNetworkCellSize = FMath::Max(8.0f, Settings.CaveNetworkCellSize);
	Settings.CaveTunnelRadius = FMath::Clamp(Settings.CaveTunnelRadius, 0.5f, Settings.CaveNetworkCellSize * 0.35f);
	Settings.CaveChamberChance = FMath::Clamp(Settings.CaveChamberChance, 0.0f, 1.0f);
	Settings.CaveChamberRadius = FMath::Max(Settings.CaveTunnelRadius, Settings.CaveChamberRadius);
	Settings.CaveWallWarpStrength = FMath::Max(0.0f, Settings.CaveWallWarpStrength);

	Settings.SurfaceMaterialId = FMath::Max(1, Settings.SurfaceMaterialId);
	Settings.SubsurfaceMaterialId = FMath::Max(1, Settings.SubsurfaceMaterialId);
	Settings.RockMaterialId = FMath::Max(1, Settings.RockMaterialId);
	Settings.SnowMaterialId = FMath::Max(1, Settings.SnowMaterialId);
	Settings.BiomeSnowMaterialId = FMath::Max(1, Settings.BiomeSnowMaterialId);
	Settings.RockSlopeThreshold = FMath::Max(0.0f, Settings.RockSlopeThreshold);
	Settings.SurfaceMaterialDepth = FMath::Max(0.01f, Settings.SurfaceMaterialDepth);
	Settings.RockMaterialDepth = FMath::Max(Settings.SurfaceMaterialDepth, Settings.RockMaterialDepth);

	TerrainFormSettings.BaseHeight = Settings.BaseHeight;
	TerrainFormSettings.VoxelSizeCm = Settings.VoxelSizeCm;
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
	HydrologySettings.RiverSeed = Settings.RiverSeed != 0
		? Settings.RiverSeed
		: Settings.BiomeSettings.HydrologySettings.RiverSeed;
	HydrologySettings.SeaLevel = Settings.BaseHeight;
	HydrologySettings.ValleyDepth = Settings.RiverValleyDepth;
	HydrologySettings.ChannelDepth = Settings.RiverChannelDepth;
	HydrologySettings.ChannelHalfWidth = FMath::Max(3.0f, Settings.RiverChannelWidth * 96.0f);
	HydrologySettings.ValleyHalfWidth = FMath::Max(
		HydrologySettings.ChannelHalfWidth + 8.0f,
		Settings.RiverValleyWidth * 160.0f
	);

	Settings.BiomeSettings = FCubusBiomeField::BindHydrology(Settings.BiomeSettings, HydrologySettings);

	SurfaceCache.Reserve(1600);
	ColumnCache.Reserve(1600);
	SurfaceMaterialCache.Reserve(1600);
	BiomeClimateCache.Reserve(96);
	BiomeTopographicClimateCache.Reserve(32);
	BiomeMacroHeightCache.Reserve(384);
}

FCubusDensitySample FCubusTerrainDensityField::Sample(const FIntVector& GlobalSampleCoordinate) const
{
	return SampleContinuous(FVector(static_cast<double>(GlobalSampleCoordinate.X), static_cast<double>(GlobalSampleCoordinate.Y), static_cast<double>(GlobalSampleCoordinate.Z)));
}

FCubusDensitySample FCubusTerrainDensityField::SampleContinuous(const FVector& GlobalSampleCoordinate) const
{
	const FColumnData& Column = GetColumnData(static_cast<float>(GlobalSampleCoordinate.X), static_cast<float>(GlobalSampleCoordinate.Y));
	const float MacroTerrainDensity = Column.SurfaceSampleZ - static_cast<float>(GlobalSampleCoordinate.Z);

	/*
	 * Fine terrain belongs to the continuous density field, not TerrainForm.
	 *
	 * TerrainForm is the stable kilometre/hillside support surface used by
	 * hydrology, biome classification and coarse streaming. This band adds
	 * bounded metre-to-decimetre relief directly to the scalar field. At the
	 * default 80 cm canonical voxel, 4x adaptive sampling evaluates it every
	 * 20 cm and therefore resolves genuine geometry that an 80 cm mesh cannot.
	 *
	 * The amplitude is deliberately less than half a canonical voxel. Fine
	 * relief can wrinkle soil and weather rock, but it cannot manufacture a
	 * detached spike or move a cliff by metres.
	 */
	float FineSurfaceDisplacement = 0.0f;
	if (Settings.bUseHeightTerrain && FMath::Abs(MacroTerrainDensity) < 2.5f)
	{
		const float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.TerrainOffsetX);
		const float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.TerrainOffsetY);
		const float Roughness = FMath::Clamp(Column.FormSample.SurfaceRoughness, 0.0f, 1.0f);
		const float ValleyCalm = 1.0f - FMath::Clamp(Column.FormSample.ValleyCarve * 0.82f, 0.0f, 0.88f);
		const float MountainTexture = FMath::Lerp(0.72f, 1.0f, Column.FormSample.MountainWeight);

		const float MesoscopicNoise = SampleNoise2D(
			WorldX + 42017.0f,
			WorldY - 18311.0f,
			FMath::Max(0.30f, Settings.DetailFrequency * 3.8f)
		);
		const float FineNoise = SampleNoise2D(
			WorldX * 0.79f - WorldY * 0.61f - 11719.0f,
			WorldX * 0.61f + WorldY * 0.79f + 23801.0f,
			FMath::Max(0.68f, Settings.DetailFrequency * 8.5f)
		);
		const float FineWarp = SampleNoise2D(
			WorldX - 7193.0f,
			WorldY + 3119.0f,
			FMath::Max(0.16f, Settings.DetailFrequency * 2.0f)
		);
		const float WarpedFineNoise = SampleNoise2D(
			WorldX + FineWarp * 0.42f + 13007.0f,
			WorldY - FineWarp * 0.42f - 9113.0f,
			FMath::Max(1.05f, Settings.DetailFrequency * 13.0f)
		);

		const float FineAmplitude =
			FMath::Lerp(0.08f, 0.34f, Roughness) *
			ValleyCalm *
			MountainTexture;
		FineSurfaceDisplacement = FMath::Clamp(
			(MesoscopicNoise * 0.54f + FineNoise * 0.31f + WarpedFineNoise * 0.15f) * FineAmplitude,
			-0.34f,
			0.34f
		);
	}

	const float BaseTerrainDensity = MacroTerrainDensity + FineSurfaceDisplacement;
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
		Result.MaterialId = GetCachedSurfaceMaterialData(
			static_cast<float>(GlobalSampleCoordinate.X),
			static_cast<float>(GlobalSampleCoordinate.Y)
		).SurfaceMaterialId;
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

FCubusBiomeSample FCubusTerrainDensityField::SampleSurfaceBiome(const float WorldX, const float WorldY) const
{
	return GetCachedSurfaceMaterialData(WorldX + 0.5f, WorldY + 0.5f).BiomeSample;
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
		const float UncarvedSurfaceHeight = Surface.FormSample.Height + LandmarkSample.HeightOffset;
		if (HydrologySettings.bEnabled)
		{
			Surface.HydrologySample = FCubusHydrologyField::Sample(WorldX, WorldY, HydrologySettings);
			Surface.bHasHydrologySample = true;
			Surface.SurfaceVoxelHeight = ApplyRiverLowering(UncarvedSurfaceHeight, Surface.HydrologySample);
		}
		else
		{
			Surface.SurfaceVoxelHeight = UncarvedSurfaceHeight;
		}
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
	// Copy before neighbour probes: GetCachedSurfaceVoxelHeight() can insert
	// into SurfaceCache and rehash its TMap, invalidating references.
	const FSurfaceData Surface = GetCachedSurfaceData(WorldSampleX, WorldSampleY);
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

	const float WorldX = WorldSampleX - 0.5f;
	const float WorldY = WorldSampleY - 0.5f;
	const float TerrainX = WorldX + static_cast<float>(Settings.TerrainOffsetX);
	const float TerrainY = WorldY + static_cast<float>(Settings.TerrainOffsetY);

	const float HardnessSignal = SampleNoise2D(
		TerrainX + 19031.0f,
		TerrainY - 12763.0f,
		Settings.GeologyHardnessFrequency
	);
	Column.RockHardness = FMath::Clamp(0.5f + HardnessSignal * 0.5f, 0.0f, 1.0f);

	const float FractureLines = SampleRidgedNoise(
		TerrainX - 7127.0f,
		TerrainY + 15551.0f,
		Settings.GeologyFractureFrequency
	);
	const float FractureProvince = FMath::Clamp(
		0.5f + SampleNoise2D(
			TerrainX + 3721.0f,
			TerrainY + 9281.0f,
			Settings.GeologyHardnessFrequency * 0.72f
		) * 0.5f,
		0.0f,
		1.0f
	);
	Column.Fracture = FMath::Clamp(FractureLines * FractureProvince, 0.0f, 1.0f);
	Column.StrataTilt = SampleNoise2D(
		TerrainX - 1091.0f,
		TerrainY - 23981.0f,
		Settings.GeologyFoldFrequency
	);

	const float CliffExposure = SmoothStep(
		Settings.GeologyCliffSlopeStart,
		Settings.GeologyCliffSlopeFull,
		Column.Slope
	);
	const float LandformExposure = FMath::Clamp(
		Column.FormSample.MountainCore * 0.82f +
		Column.FormSample.FoothillWeight * 0.46f +
		Column.FormSample.Ridge * 0.32f,
		0.0f,
		1.0f
	);
	const float DrainageProtection = 1.0f - FMath::Clamp(
		Column.FormSample.Drainage * Column.FormSample.Drainage,
		0.0f,
		1.0f
	);
	Column.RockExposure = CliffExposure * LandformExposure * DrainageProtection;


	ColumnCache.Add(Key, Column);
	return ColumnCache.FindChecked(Key);
}

const FCubusTerrainDensityField::FSurfaceMaterialData& FCubusTerrainDensityField::GetCachedSurfaceMaterialData(
	const float WorldSampleX,
	const float WorldSampleY
) const
{
	/*
	 * Material/ecology classification is intentionally canonical-rate. Fine
	 * 20 cm density samples still evaluate the true continuous geometry, but
	 * neighbouring samples share the same 80 cm ecological decision. Biome
	 * geography varies at metre-to-kilometre scales, so no meaningful visual
	 * information is lost by refusing to classify it 16x per canonical cell.
	 */
	const FIntPoint Key(FMath::RoundToInt(WorldSampleX), FMath::RoundToInt(WorldSampleY));
	if (const FSurfaceMaterialData* Existing = SurfaceMaterialCache.Find(Key))
	{
		return *Existing;
	}

	const float CanonicalSampleX = static_cast<float>(Key.X);
	const float CanonicalSampleY = static_cast<float>(Key.Y);
	const FSurfaceData Surface = GetCachedSurfaceData(CanonicalSampleX, CanonicalSampleY);
	const FColumnData& Column = GetColumnData(CanonicalSampleX, CanonicalSampleY);
	const float WorldX = CanonicalSampleX - 0.5f;
	const float WorldY = CanonicalSampleY - 0.5f;
	const float TerrainX = WorldX + static_cast<float>(Settings.TerrainOffsetX);
	const float TerrainY = WorldY + static_cast<float>(Settings.TerrainOffsetY);

	FCubusBiomeTerrainContext BiomeTerrainContext;
	BiomeTerrainContext.Drainage = Column.FormSample.Drainage;
	BiomeTerrainContext.RockExposure = Column.RockExposure;
	BiomeTerrainContext.MountainCore = Column.FormSample.MountainCore;
	BiomeTerrainContext.FoothillWeight = Column.FormSample.FoothillWeight;
	BiomeTerrainContext.Ridge = Column.FormSample.Ridge;
	BiomeTerrainContext.Gradient = Column.Gradient;
	BiomeTerrainContext.bHasTerrainFormSample = true;
	BiomeTerrainContext.TerrainFormSample = Column.FormSample;
	BiomeTerrainContext.bHasHydrologySample = Surface.bHasHydrologySample;
	BiomeTerrainContext.HydrologySample = Surface.HydrologySample;
	BiomeTerrainContext.bHasSubstrateSample = true;
	BiomeTerrainContext.SubstrateHardness = Column.RockHardness;
	BiomeTerrainContext.FractureDensity = Column.Fracture;
	BiomeTerrainContext.bHasTopographicClimateSample = true;
	BiomeTerrainContext.TopographicClimateSample = GetInterpolatedBiomeTopographicClimate(WorldX, WorldY);

	FSurfaceMaterialData MaterialData;
	const FCubusBiomeClimateContext Climate = GetInterpolatedBiomeClimate(WorldX, WorldY);
	MaterialData.BiomeSample = FCubusBiomeField::Sample(
		WorldX,
		WorldY,
		Column.SurfaceVoxelHeight,
		Column.Slope,
		Settings.BiomeSettings,
		BiomeTerrainContext,
		&Climate
	);

	const FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(TerrainX, TerrainY, Settings.LandmarkSettings);
	if (LandmarkSample.IsInside())
	{
		MaterialData.SurfaceMaterialId = FMath::Max(1, Settings.LandmarkSettings.SurfaceMaterialId);
	}
	else
	{
		MaterialData.SurfaceMaterialId = Column.Slope >= Settings.RockSlopeThreshold
			? Settings.RockMaterialId
			: (Settings.BiomeSettings.bEnabled ? MaterialData.BiomeSample.SurfaceMaterialId : Settings.SurfaceMaterialId);

		const float StructuralSnowFloor =
			Settings.BaseHeight + FMath::Max(64.0f, Settings.RidgeAmplitude * 4.0f);
		const float ConfiguredSnowLine = Settings.BiomeSettings.bEnabled
			? Settings.BiomeSnowMinimumHeight
			: Settings.SnowMinimumHeight;
		const float SnowLine = FMath::Max(ConfiguredSnowLine, StructuralSnowFloor);
		const float SnowRetention = 1.0f - SmoothStep(
			Settings.RockSlopeThreshold * 0.95f,
			FMath::Max(Settings.RockSlopeThreshold * 1.70f, Settings.RockSlopeThreshold + 0.01f),
			Column.Slope
		);

		if (Column.SurfaceVoxelHeight >= SnowLine && SnowRetention >= 0.30f)
		{
			MaterialData.SurfaceMaterialId = Settings.BiomeSettings.bEnabled
				? Settings.BiomeSnowMaterialId
				: Settings.SnowMaterialId;
		}
	}

	SurfaceMaterialCache.Add(Key, MaterialData);
	return SurfaceMaterialCache.FindChecked(Key);
}

const FCubusBiomeClimateContext& FCubusTerrainDensityField::GetCachedBiomeClimateCell(
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

float FCubusTerrainDensityField::GetCachedBiomeMacroHeight(const float WorldX, const float WorldY) const
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

    static const FVector2D SkyDirections[] = {
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

float FCubusTerrainDensityField::SampleRidgedNoise3D(const float WorldX, const float WorldY, const float WorldZ, const float Frequency) const
{
	const float NoiseValue = SampleNoise3D(WorldX, WorldY, WorldZ, Frequency);
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

	return ApplyRiverLowering(
		SurfaceHeight,
		FCubusHydrologyField::Sample(WorldX, WorldY, HydrologySettings)
	);
}

float FCubusTerrainDensityField::ApplyRiverLowering(
	const float SurfaceHeight,
	const FCubusHydrologySample& Hydrology
) const
{
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

float FCubusTerrainDensityField::SampleGeologicalDensity(
	const FVector& GlobalSampleCoordinate,
	const FColumnData& Column,
	const float BaseTerrainDensity
) const
{
	const float DistanceFromSurface = FMath::Abs(BaseTerrainDensity);
	const float EffectiveSurfaceBand = FMath::Min(Settings.GeologySurfaceBand, 6.0f);
	if (DistanceFromSurface >= EffectiveSurfaceBand || Column.RockExposure <= KINDA_SMALL_NUMBER)
	{
		return BaseTerrainDensity;
	}

	const float SurfaceBandMask = 1.0f - SmoothStep(0.0f, EffectiveSurfaceBand, DistanceFromSurface);
	const float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.TerrainOffsetX);
	const float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.TerrainOffsetY);
	const float WorldZ = static_cast<float>(GlobalSampleCoordinate.Z);

	/*
	 * Structural permission comes from the macro landform. A steep slope alone
	 * is not permission to turn every cliff into a noisy saw blade.
	 */
	const float StructuralCarrier = FMath::Clamp(
		Column.FormSample.Escarpment * 0.62f +
		Column.FormSample.MountainCore * 0.22f +
		Column.FormSample.MassifWeight * 0.18f +
		Column.FormSample.Ridge * 0.12f,
		0.0f,
		1.0f
	);
	const float ValleyProtection = 1.0f - FMath::Clamp(
		Column.FormSample.ValleyCarve * 0.86f,
		0.0f,
		0.92f
	);
	const float GeologyCarrier = FMath::Clamp(
		Column.RockExposure *
		FMath::Lerp(0.34f, 1.0f, StructuralCarrier) *
		ValleyProtection,
		0.0f,
		1.0f
	);
	if (GeologyCarrier <= KINDA_SMALL_NUMBER)
	{
		return BaseTerrainDensity;
	}

	FVector2D Downhill = -Column.Gradient;
	if (!Downhill.Normalize())
	{
		return BaseTerrainDensity;
	}
	const FVector2D AlongCliff(-Downhill.Y, Downhill.X);
	const float Across = WorldX * Downhill.X + WorldY * Downhill.Y;
	const float Along = WorldX * AlongCliff.X + WorldY * AlongCliff.Y;

	/* Broad strata. This is shape modulation, never a multi-voxel shelf stamp. */
	const float StrataFrequency = FMath::Clamp(
		Settings.GeologyStrataFrequency,
		0.008f,
		0.16f
	);
	const float FoldedDip = FMath::Clamp(
		Settings.GeologyStrataDip + Column.StrataTilt * 0.012f,
		-0.035f,
		0.035f
	);
	const float StrataPhase =
		WorldZ * StrataFrequency +
		Across * FoldedDip +
		SampleNoise2D(Along + 9113.0f, Across - 4327.0f, StrataFrequency * 0.16f) * 0.20f;
	const float StrataWave = FMath::Sin(StrataPhase * 2.0f * PI);

	const float RockMassFrequency = TerrainFormSettings.bUsePhysicalWorldScale
		? CubusWorldScale::FrequencyForWavelengthMeters(220.0f, Settings.VoxelSizeCm)
		: FMath::Clamp(Settings.GeologyMassFrequency, 0.006f, 0.18f);
	const float RockWarpFrequency = TerrainFormSettings.bUsePhysicalWorldScale
		? CubusWorldScale::FrequencyForWavelengthMeters(70.0f, Settings.VoxelSizeCm)
		: FMath::Clamp(Settings.GeologyRockWarpFrequency, 0.008f, 0.22f);
	const float FractureFrequency = TerrainFormSettings.bUsePhysicalWorldScale
		? CubusWorldScale::FrequencyForWavelengthMeters(90.0f, Settings.VoxelSizeCm)
		: FMath::Clamp(Settings.GeologyFractureFrequency, 0.006f, 0.18f);

	const float RockMass = SampleNoise3D(
		WorldX - 6211.0f,
		WorldY + 4177.0f,
		WorldZ - 1987.0f,
		RockMassFrequency
	);
	const float RockWarp = SampleNoise3D(
		WorldX + 1709.0f,
		WorldY - 3187.0f,
		WorldZ + 733.0f,
		RockWarpFrequency
	);
	const float FractureVolume = SampleRidgedNoise3D(
		WorldX + 12011.0f,
		WorldY - 4919.0f,
		WorldZ + 2791.0f,
		FractureFrequency
	);
	const float FractureCut =
		SmoothStep(0.80f, 0.97f, FractureVolume) *
		Column.Fracture;

	/*
	 * A vertical-varying lateral signal permits small real overhangs where the
	 * cliff structure supports them. Its budget is centimetres/decimetres in
	 * voxel-space, not the old several-metre displacement.
	 */
	const float OverhangSignal = SampleNoise3D(
		Across + 2381.0f,
		Along + 7151.0f,
		WorldZ - 3319.0f,
		0.11f
	);
	const float OverhangPermission =
		SmoothStep(0.42f, 0.88f, StructuralCarrier) *
		Column.RockHardness;

	const float RawDeformation =
		StrataWave * 0.22f * FMath::Lerp(0.62f, 1.0f, Column.RockHardness) +
		RockMass * 0.24f +
		RockWarp * 0.12f +
		OverhangSignal * 0.30f * OverhangPermission -
		FractureCut * 0.20f;

	const float MaximumDisplacement = FMath::Lerp(
		0.16f,
		0.72f,
		GeologyCarrier
	) * SurfaceBandMask;
	const float GeologicalDisplacement = FMath::Clamp(
		RawDeformation,
		-MaximumDisplacement,
		MaximumDisplacement
	);

	return BaseTerrainDensity + GeologicalDisplacement;
}

float FCubusTerrainDensityField::SampleCaveDensity(
	const FVector& GlobalSampleCoordinate,
	const float SurfaceVoxelHeight,
	const float SurfaceSlope
) const
{
	const float WorldZ = static_cast<float>(GlobalSampleCoordinate.Z);
	const float VerticalDepth = SurfaceVoxelHeight - WorldZ;
	const float ApproximateShellDepth = VerticalDepth / FMath::Sqrt(1.0f + SurfaceSlope * SurfaceSlope);
	if (
		WorldZ < Settings.CaveMinimumWorldZ ||
		WorldZ > Settings.CaveMaximumWorldZ ||
		ApproximateShellDepth < static_cast<float>(Settings.CaveSurfaceClearance)
	)
	{
		return MAX_flt;
	}

	const FVector Query(
		GlobalSampleCoordinate.X + static_cast<double>(Settings.CaveOffsetX),
		GlobalSampleCoordinate.Y + static_cast<double>(Settings.CaveOffsetY),
		GlobalSampleCoordinate.Z + static_cast<double>(Settings.CaveOffsetZ)
	);
	const float CellSize = Settings.CaveNetworkCellSize;
	const int32 CentreCellX = FMath::FloorToInt(static_cast<float>(Query.X) / CellSize);
	const int32 CentreCellY = FMath::FloorToInt(static_cast<float>(Query.Y) / CellSize);
	const uint32 Seed = CubusTerrainDensityField::CaveSeed(Settings);

	float MinimumSignedDistance = MAX_flt;
	for (int32 CellY = CentreCellY - 1; CellY <= CentreCellY + 1; ++CellY)
	{
		for (int32 CellX = CentreCellX - 1; CellX <= CentreCellX + 1; ++CellX)
		{
			const float Activity = CubusTerrainDensityField::Hash01(
				CellX,
				CellY,
				Seed ^ 0xB5297A4Du
			);
			if (Activity > Settings.CaveThreshold)
			{
				continue;
			}

			const FVector Node = CubusTerrainDensityField::CaveNode(CellX, CellY, Settings, Seed);
			const float DirectionChoice = CubusTerrainDensityField::Hash01(
				CellX,
				CellY,
				Seed ^ 0x68E31DA4u
			);
			const int32 TargetX = CellX + (DirectionChoice < 0.46f ? 1 : (DirectionChoice < 0.84f ? 0 : 1));
			const int32 TargetY = CellY + (DirectionChoice < 0.46f ? 0 : 1);
			const FVector Target = CubusTerrainDensityField::CaveNode(TargetX, TargetY, Settings, Seed);

			const float RadiusVariation = FMath::Lerp(
				0.72f,
				1.28f,
				CubusTerrainDensityField::Hash01(CellX, CellY, Seed ^ 0x1B56C4E9u)
			);
			const float TunnelRadius = Settings.CaveTunnelRadius * RadiusVariation;
			const float SegmentDistance = CubusTerrainDensityField::DistanceToSegment(Query, Node, Target);
			MinimumSignedDistance = FMath::Min(MinimumSignedDistance, SegmentDistance - TunnelRadius);

			const float ChamberRoll = CubusTerrainDensityField::Hash01(CellX, CellY, Seed ^ 0x9E3779B1u);
			if (ChamberRoll < Settings.CaveChamberChance)
			{
				const float ChamberVariation = FMath::Lerp(
					0.75f,
					1.25f,
					CubusTerrainDensityField::Hash01(CellX, CellY, Seed ^ 0xD1B54A35u)
				);
				const float ChamberDistance = static_cast<float>(FVector::Distance(Query, Node)) -
					Settings.CaveChamberRadius * ChamberVariation;
				MinimumSignedDistance = FMath::Min(MinimumSignedDistance, ChamberDistance);
			}
		}
	}

	if (MinimumSignedDistance >= MAX_flt * 0.5f)
	{
		return MAX_flt;
	}

	const float WallWarp =
		SampleNoise3D(
			static_cast<float>(Query.X) + 1871.0f,
			static_cast<float>(Query.Y) - 953.0f,
			static_cast<float>(Query.Z) + 421.0f,
			Settings.CavePrimaryFrequency
		) * 0.65f +
		SampleNoise3D(
			static_cast<float>(Query.X) - 3167.0f,
			static_cast<float>(Query.Y) + 2297.0f,
			static_cast<float>(Query.Z) - 811.0f,
			Settings.CaveSecondaryFrequency
		) * 0.35f;

	return (
		MinimumSignedDistance -
		WallWarp * Settings.CaveWallWarpStrength
	) * Settings.CaveSurfaceSharpness;
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
