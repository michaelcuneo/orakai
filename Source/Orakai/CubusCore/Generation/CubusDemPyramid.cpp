#include "CubusCore/Generation/CubusDemPyramid.h"

#include "Async/ParallelFor.h"
#include "Misc/Paths.h"

namespace CubusDemPyramid
{
namespace
{
constexpr float QuiltWeightPower = 2.0f;

struct FSource
{
	CubusDemIsland::FIndexedPatch Metadata;
	CubusDemIsland::FPatch Patch;
	float ExtentM = 0.0f;
};

struct FPlacement
{
	int32 SourceIndex = 0;
	int32 Variant = 0;
	FVector2D CenterM = FVector2D::ZeroVector;
};

struct FLayer
{
	FString FolderPrefix;
	float Amplitude = 1.0f;
	float MinimumExtentM = 0.0f;
	float MaximumExtentM = TNumericLimits<float>::Max();
	int32 SeedSalt = 0;
	TArray<FSource> Sources;
	TArray<FPlacement> Placements;
	int32 GridResolution = 0;
	float GridSpacingM = 0.0f;
	double GridOriginM = 0.0;
};

float Smooth01(const float T)
{
	const float X = FMath::Clamp(T, 0.0f, 1.0f);
	return X * X * (3.0f - 2.0f * X);
}

FVector2D TransformUv(FVector2D UV, const int32 Variant)
{
	UV -= FVector2D(0.5, 0.5);
	if ((Variant & 1) != 0) UV.X = -UV.X;
	if ((Variant & 2) != 0) UV.Y = -UV.Y;
	switch ((Variant >> 2) & 3)
	{
	case 1: UV = FVector2D(-UV.Y, UV.X); break;
	case 2: UV = FVector2D(-UV.X, -UV.Y); break;
	case 3: UV = FVector2D(UV.Y, -UV.X); break;
	default: break;
	}
	return UV + FVector2D(0.5, 0.5);
}

CubusLandscapeEvolution::EProvinceType MapProvinceType(const CubusDemIsland::FIndexedPatch& Entry)
{
	if (Entry.TerrainClass.Contains(TEXT("mountain"))) return CubusLandscapeEvolution::EProvinceType::FoldMountainBelt;
	if (Entry.TerrainClass.Contains(TEXT("coast"))) return CubusLandscapeEvolution::EProvinceType::CoastalShelf;
	if (Entry.TerrainClass.Contains(TEXT("plain")) || Entry.TerrainClass.Contains(TEXT("lowland"))) return CubusLandscapeEvolution::EProvinceType::SedimentaryBasin;
	if (Entry.TerrainClass.Contains(TEXT("rugged")) || Entry.TerrainClass.Contains(TEXT("dissected"))) return CubusLandscapeEvolution::EProvinceType::UpliftedPlateau;
	return CubusLandscapeEvolution::EProvinceType::StablePlain;
}

bool LoadLayer(
	const CubusDemIsland::FSettings& Settings,
	const TArray<CubusDemIsland::FIndexedPatch>& IndexEntries,
	FLayer& Layer,
	FString* OutError)
{
	const FString SourceRoot = CubusDemIsland::FGenerator::ResolveSourceDirectory(Settings);
	for (const CubusDemIsland::FIndexedPatch& Entry : IndexEntries)
	{
		if (!Entry.RelativePath.StartsWith(Layer.FolderPrefix)) continue;

		FSource Source;
		Source.Metadata = Entry;
		const FString PatchPath = FPaths::Combine(SourceRoot, Entry.RelativePath);
		FString Error;
		if (!CubusDemIsland::FGenerator::LoadPatch(PatchPath, Source.Patch, &Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("Skipping multiscale DEM patch %s: %s"), *PatchPath, *Error);
			continue;
		}
		Source.ExtentM = FMath::Min(
			(Source.Patch.Width - 1) * Source.Patch.CellSizeM,
			(Source.Patch.Height - 1) * Source.Patch.CellSizeM);
		if (Source.ExtentM < Layer.MinimumExtentM || Source.ExtentM > Layer.MaximumExtentM) continue;
		Layer.Sources.Add(MoveTemp(Source));
	}

	if (Layer.Sources.IsEmpty())
	{
		if (OutError)
		{
			*OutError = FString::Printf(TEXT("No prepared DEM sources found for tier %s. Run prepare_multiscale_dem_library.py."), *Layer.FolderPrefix);
		}
		return false;
	}

	float MeanExtentM = 0.0f;
	for (const FSource& Source : Layer.Sources) MeanExtentM += Source.ExtentM;
	MeanExtentM /= static_cast<float>(Layer.Sources.Num());

	// The spacing is intentionally smaller than the physical patch extent. Adjacent
	// real source windows overlap enough to blend, but no source is enlarged.
	Layer.GridSpacingM = FMath::Max(1000.0f, MeanExtentM * 0.56f);
	Layer.GridResolution = FMath::CeilToInt(Settings.WorldSizeMeters / Layer.GridSpacingM) + 4;
	const double HalfWorld = Settings.WorldSizeMeters * 0.5;
	Layer.GridOriginM = -HalfWorld - Layer.GridSpacingM * 1.5;
	Layer.Placements.SetNum(Layer.GridResolution * Layer.GridResolution);

	FRandomStream Random(Settings.Seed ^ Layer.SeedSalt);
	for (int32 Gy = 0; Gy < Layer.GridResolution; ++Gy)
	{
		for (int32 Gx = 0; Gx < Layer.GridResolution; ++Gx)
		{
			const FVector2D Center(
				Layer.GridOriginM + Gx * Layer.GridSpacingM,
				Layer.GridOriginM + Gy * Layer.GridSpacingM);
			const float Radial01 = FMath::Clamp(static_cast<float>(Center.Size() / HalfWorld), 0.0f, 1.0f);
			const float DesiredRelief01 = FMath::Clamp(1.08f - Radial01 * 0.72f + Random.FRandRange(-0.22f, 0.22f), 0.0f, 1.0f);

			int32 BestSource = 0;
			float BestScore = -TNumericLimits<float>::Max();
			for (int32 Candidate = 0; Candidate < Layer.Sources.Num(); ++Candidate)
			{
				const FSource& Source = Layer.Sources[Candidate];
				const float ReliefScaleM = FMath::Max(350.0f, Layer.MinimumExtentM * 0.012f);
				const float Relief01 = FMath::Clamp(Source.Metadata.ReliefP90M / ReliefScaleM, 0.0f, 1.0f);
				const float ReliefMatch = 1.0f - FMath::Abs(Relief01 - DesiredRelief01);
				const float Score = ReliefMatch * 0.60f + Source.Metadata.MacroSuitability * 0.30f + Random.FRandRange(0.0f, 0.10f);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestSource = Candidate;
				}
			}

			FPlacement& Placement = Layer.Placements[Gy * Layer.GridResolution + Gx];
			Placement.SourceIndex = BestSource;
			Placement.Variant = Random.RandRange(0, 15);
			Placement.CenterM = Center;
		}
	}

	float MinReliefM = TNumericLimits<float>::Max();
	float MaxReliefM = 0.0f;
	float MeanReliefM = 0.0f;
	for (const FSource& Source : Layer.Sources)
	{
		MinReliefM = FMath::Min(MinReliefM, Source.Metadata.ReliefP90M);
		MaxReliefM = FMath::Max(MaxReliefM, Source.Metadata.ReliefP90M);
		MeanReliefM += Source.Metadata.ReliefP90M;
	}
	MeanReliefM /= Layer.Sources.Num();
	UE_LOG(LogTemp, Display,
		TEXT("Cubus DEM tier %s: %d sources, mean extent %.1f km, spacing %.1f km, P90 relief %.0f/%.0f/%.0f m, amplitude %.2f"),
		*Layer.FolderPrefix, Layer.Sources.Num(), MeanExtentM / 1000.0f, Layer.GridSpacingM / 1000.0f,
		MinReliefM, MeanReliefM, MaxReliefM, Layer.Amplitude);
	return true;
}

float SampleLayer(const FLayer& Layer, const FVector2D& WorldM, int32* OutDominantSource = nullptr)
{
	const double Gx = (WorldM.X - Layer.GridOriginM) / Layer.GridSpacingM;
	const double Gy = (WorldM.Y - Layer.GridOriginM) / Layer.GridSpacingM;
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(Gx), 0, Layer.GridResolution - 2);
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Gy), 0, Layer.GridResolution - 2);
	const float Fx = Smooth01(static_cast<float>(Gx - X0));
	const float Fy = Smooth01(static_cast<float>(Gy - Y0));
	const int32 PlacementIndices[4] = {
		Y0 * Layer.GridResolution + X0,
		Y0 * Layer.GridResolution + X0 + 1,
		(Y0 + 1) * Layer.GridResolution + X0,
		(Y0 + 1) * Layer.GridResolution + X0 + 1};
	const float BaseWeights[4] = {
		(1.0f - Fx) * (1.0f - Fy),
		Fx * (1.0f - Fy),
		(1.0f - Fx) * Fy,
		Fx * Fy};

	float WeightedRelief = 0.0f;
	float WeightSquaredSum = 0.0f;
	float BestWeight = -1.0f;
	int32 BestSource = 0;
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const FPlacement& Placement = Layer.Placements[PlacementIndices[Corner]];
		const FSource& Source = Layer.Sources[Placement.SourceIndex];
		const FVector2D Local = WorldM - Placement.CenterM;
		FVector2D SourceUv(Local.X / Source.ExtentM + 0.5f, Local.Y / Source.ExtentM + 0.5f);
		SourceUv = TransformUv(SourceUv, Placement.Variant);
		if (SourceUv.X < 0.0f || SourceUv.X > 1.0f || SourceUv.Y < 0.0f || SourceUv.Y > 1.0f) continue;

		const float W = FMath::Pow(FMath::Max(0.0f, BaseWeights[Corner]), QuiltWeightPower);
		if (W <= KINDA_SMALL_NUMBER) continue;
		const float Sample = Source.Patch.SampleBilinear(SourceUv.X, SourceUv.Y) - Source.Patch.MeanElevationM;
		WeightedRelief += Sample * W;
		WeightSquaredSum += W * W;
		if (W > BestWeight)
		{
			BestWeight = W;
			BestSource = Placement.SourceIndex;
		}
	}

	if (OutDominantSource) *OutDominantSource = BestSource;
	return WeightSquaredSum > KINDA_SMALL_NUMBER
		? WeightedRelief / FMath::Sqrt(WeightSquaredSum)
		: 0.0f;
}
}

bool Compose(
	const CubusDemIsland::FSettings& Settings,
	const TArray<CubusDemIsland::FIndexedPatch>& IndexEntries,
	FResult& OutResult,
	FString* OutError)
{
	OutResult = FResult{};
	if (Settings.Resolution < 33 || Settings.WorldSizeMeters <= 10000.0)
	{
		if (OutError) *OutError = TEXT("Invalid global DEM dimensions for multiscale composition.");
		return false;
	}

	FLayer Macro128;
	Macro128.FolderPrefix = TEXT("Macro128km/");
	Macro128.Amplitude = 0.90f;
	Macro128.MinimumExtentM = 100000.0f;
	Macro128.MaximumExtentM = 160000.0f;
	Macro128.SeedSalt = 0x128128;

	FLayer Macro64;
	Macro64.FolderPrefix = TEXT("Macro64km/");
	Macro64.Amplitude = 0.42f;
	Macro64.MinimumExtentM = 50000.0f;
	Macro64.MaximumExtentM = 85000.0f;
	Macro64.SeedSalt = 0x064064;

	FLayer Macro32;
	Macro32.FolderPrefix = TEXT("Macro32km/");
	Macro32.Amplitude = 0.22f;
	Macro32.MinimumExtentM = 24000.0f;
	Macro32.MaximumExtentM = 43000.0f;
	Macro32.SeedSalt = 0x032032;

	if (!LoadLayer(Settings, IndexEntries, Macro128, OutError)) return false;
	if (!LoadLayer(Settings, IndexEntries, Macro64, OutError)) return false;
	if (!LoadLayer(Settings, IndexEntries, Macro32, OutError)) return false;

	OutResult.Macro128SourceCount = Macro128.Sources.Num();
	OutResult.Macro64SourceCount = Macro64.Sources.Num();
	OutResult.Macro32SourceCount = Macro32.Sources.Num();

	const int32 CellCount = Settings.Resolution * Settings.Resolution;
	OutResult.ReliefM.SetNumUninitialized(CellCount);
	OutResult.ProvinceId.SetNumUninitialized(CellCount);
	const double HalfWorld = Settings.WorldSizeMeters * 0.5;
	const int32 R = Settings.Resolution;

	ParallelFor(CellCount, [&Settings, &Macro128, &Macro64, &Macro32, &OutResult, HalfWorld, R](const int32 Cell)
	{
		const int32 X = Cell % R;
		const int32 Y = Cell / R;
		const double U = static_cast<double>(X) / static_cast<double>(R - 1);
		const double V = static_cast<double>(Y) / static_cast<double>(R - 1);
		const FVector2D WorldM(-HalfWorld + U * Settings.WorldSizeMeters, -HalfWorld + V * Settings.WorldSizeMeters);

		int32 Dominant128 = 0;
		const float Relief128 = SampleLayer(Macro128, WorldM, &Dominant128);
		const float Relief64 = SampleLayer(Macro64, WorldM);
		const float Relief32 = SampleLayer(Macro32, WorldM);
		OutResult.ReliefM[Cell] = Settings.ReliefScale * (
			Relief128 * Macro128.Amplitude +
			Relief64 * Macro64.Amplitude +
			Relief32 * Macro32.Amplitude);

		OutResult.ProvinceId[Cell] = static_cast<uint8>(MapProvinceType(Macro128.Sources[Dominant128].Metadata));
	});

	UE_LOG(LogTemp, Display,
		TEXT("Cubus multiscale DEM pyramid composed: %d x 128 km, %d x 64 km, %d x 32 km real source patches"),
		OutResult.Macro128SourceCount, OutResult.Macro64SourceCount, OutResult.Macro32SourceCount);
	return true;
}
} // namespace CubusDemPyramid
