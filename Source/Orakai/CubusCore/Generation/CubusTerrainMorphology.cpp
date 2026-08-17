#include "CubusCore/Generation/CubusTerrainMorphology.h"

namespace CubusTerrainMorphology
{
uint32 Hash2D(const int32 X, const int32 Y, const uint32 Seed)
{
	uint32 H = static_cast<uint32>(X) * 0x9E3779B9u;
	H ^= static_cast<uint32>(Y) * 0x85EBCA6Bu;
	H ^= Seed * 0xC2B2AE35u;
	H ^= H >> 16;
	H *= 0x7FEB352Du;
	H ^= H >> 15;
	H *= 0x846CA68Bu;
	H ^= H >> 16;
	return H;
}

float HashSigned(const int32 X, const int32 Y, const uint32 Seed)
{
	return static_cast<float>(Hash2D(X, Y, Seed) & 0x00ffffffu) / static_cast<float>(0x00ffffffu) * 2.0f - 1.0f;
}

float Smooth01(const float T)
{
	const float X = FMath::Clamp(T, 0.0f, 1.0f);
	return X * X * (3.0f - 2.0f * X);
}

FVector2D SeedOffset(const uint32 Seed)
{
	return FVector2D(static_cast<double>(HashSigned(17, 31, Seed)) * 4096.0,
					 static_cast<double>(HashSigned(47, 13, Seed ^ 0x9E3779B9u)) * 4096.0);
}

float GradientNoise(const double X, const double Y, const uint32 Seed)
{
	const FVector2D Offset = SeedOffset(Seed);
	return FMath::PerlinNoise2D(FVector2D(X + Offset.X, Y + Offset.Y));
}

float Fbm(double X, double Y, const uint32 Seed, const int32 Octaves)
{
	float Sum		= 0.0f;
	float Weight	= 0.0f;
	float Amplitude = 1.0f;
	for (int32 Octave = 0; Octave < Octaves; ++Octave)
	{
		Sum += GradientNoise(X, Y, Seed + static_cast<uint32>(Octave) * 1013u) * Amplitude;
		Weight += Amplitude;
		X = X * 2.031 + 13.7;
		Y = Y * 2.017 - 19.2;
		Amplitude *= 0.5f;
	}
	return Weight > 0.0f ? Sum / Weight : 0.0f;
}

float Ridged(double X, double Y, const uint32 Seed, const int32 Octaves)
{
	float Sum		= 0.0f;
	float Weight	= 0.0f;
	float Amplitude = 1.0f;
	float Previous	= 1.0f;
	for (int32 Octave = 0; Octave < Octaves; ++Octave)
	{
		float Ridge = 1.0f - FMath::Abs(GradientNoise(X, Y, Seed + static_cast<uint32>(Octave) * 1619u));
		Ridge *= Ridge;
		Ridge *= FMath::Lerp(0.45f, 1.0f, Previous);
		Sum += Ridge * Amplitude;
		Weight += Amplitude;
		Previous = Ridge;
		X		 = X * 2.07 + 7.4;
		Y		 = Y * 2.03 - 5.8;
		Amplitude *= 0.52f;
	}
	return Weight > 0.0f ? Sum / Weight : 0.0f;
}
} // namespace CubusTerrainMorphology

FCubusTerrainMorphologySample FCubusTerrainMorphology::Sample(const double WorldXmeters, const double WorldYmeters,
															  const FCubusTerrainStructureSample&	Structure,
															  const FCubusTerrainStructureSettings& InSettings)
{
	using namespace CubusTerrainMorphology;

	FCubusTerrainStructureSettings Settings = InSettings;
	Settings.HillScaleKm					= FMath::Max(0.6f, Settings.HillScaleKm);
	Settings.SpurScaleKm					= FMath::Max(0.20f, Settings.SpurScaleKm);
	Settings.SwaleScaleMeters				= FMath::Max(80.0f, Settings.SwaleScaleMeters);
	Settings.FineScaleMeters				= FMath::Max(24.0f, Settings.FineScaleMeters);

	const uint32 Seed = static_cast<uint32>(Settings.Seed);

	// Warp the local coordinates with a long wavelength field. This prevents the
	// subordinate ridge families from becoming visibly parallel/repetitive.
	const double WarpScale = FMath::Max(1600.0, static_cast<double>(Settings.HillScaleKm) * 1800.0);
	const float	 WarpX	   = Fbm(WorldXmeters / WarpScale, WorldYmeters / WarpScale, Seed ^ 0xA24BAED5u, 3);
	const float	 WarpY	   = Fbm(WorldXmeters / WarpScale, WorldYmeters / WarpScale, Seed ^ 0x9FB21C65u, 3);
	const double X		   = WorldXmeters + static_cast<double>(WarpX) * Settings.MorphologyWarpMeters;
	const double Y		   = WorldYmeters + static_cast<double>(WarpY) * Settings.MorphologyWarpMeters;

	FCubusTerrainMorphologySample Result;

	const float MacroUpland =
		FMath::Clamp(Structure.RangeWeight * 0.72f + Structure.MassifWeight * 0.48f + Structure.RangeCoreWeight * 0.24f, 0.0f, 1.0f);
	const float BasinSuppression = 1.0f - Smooth01(Structure.BasinWeight);

	// Large coherent provinces decide whether a location is plain, rolling
	// country or upland before any smaller detail is allowed to contribute.
	const double ProvinceScale = FMath::Max(2400.0, static_cast<double>(Settings.HillScaleKm) * 2600.0);
	const float	 Province	   = Fbm(X / ProvinceScale, Y / ProvinceScale, Seed ^ 0x4CF5AD43u, 3);
	const float	 RollingMask   = Smooth01((Province + 0.08f) / 0.58f) * BasinSuppression;
	const float	 UplandMask	   = FMath::Clamp(MacroUpland + Smooth01((Province - 0.14f) / 0.56f) * 0.34f, 0.0f, 1.0f);
	const float	 PlainMask	   = 1.0f - Smooth01(FMath::Clamp(RollingMask * 0.72f + UplandMask * 0.58f, 0.0f, 1.0f));
	Result.PlainWeight		   = PlainMask;
	Result.RollingWeight	   = RollingMask;
	Result.UplandWeight		   = UplandMask;

	const double HillScale = static_cast<double>(Settings.HillScaleKm) * 1000.0;
	const float	 HillBase  = Fbm(X / HillScale, Y / HillScale, Seed ^ 0xC13FA9A9u, 5);
	const float	 HillShape = HillBase;
	Result.HillMeters =
		HillShape * Settings.HillReliefMeters * FMath::Lerp(0.08f, 1.0f, RollingMask) * FMath::Lerp(0.72f, 1.12f, UplandMask);

	Result.SpurMeters  = 0.0f;
	Result.SwaleMeters = 0.0f;

	// Tens-of-metres breakup. This is intentionally low amplitude: it gives the
	// 1 m DEM something to resolve without turning the landscape into noise.
	const double FineScale = static_cast<double>(Settings.FineScaleMeters);
	const float	 Fine	   = Fbm(X / FineScale, Y / FineScale, Seed ^ 0x3C6EF372u, 4);
	const float	 FineMask = FMath::Clamp(UplandMask * 0.82f + RollingMask * 0.30f, 0.0f, 1.0f) * FMath::Lerp(0.12f, 1.0f, 1.0f - PlainMask);
	Result.FineMeters	  = Fine * Settings.FineReliefMeters * FineMask;

	Result.HeightOffsetMeters = Result.HillMeters + Result.SpurMeters + Result.SwaleMeters + Result.FineMeters;
	return Result;
}
