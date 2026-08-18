#include "CubusCore/Generation/CubusDemPyramid.h"

#include "Async/ParallelFor.h"
#include "Misc/Paths.h"

namespace CubusDemPyramid
{
namespace
{
constexpr float QuiltWeightPower = 2.0f;

enum class ELayerRole : uint8
{
	PrimarySurface,
	ResidualDetail
};

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
	ELayerRole Role = ELayerRole::ResidualDetail;
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

/**
 * Island-scale structural model used to assemble the real DEM library.
 *
 * This is intentionally not a height-noise generator. It only says what kind
 * of real landform belongs where: a curved convergent/suture belt, an uplifted
 * shoulder on one side, a broader foreland/basin on the other, and lower-relief
 * terrain toward the ends and outer flanks. Actual landform geometry still
 * comes from the sampled DEM patches.
 */
struct FGeologicPlan
{
	FVector2D Axis = FVector2D(1.0, 0.0);
	FVector2D Normal = FVector2D(0.0, 1.0);
	float HalfLengthM = 220000.0f;
	float CoreHalfWidthM = 36000.0f;
	float ShoulderWidthM = 76000.0f;
	float BasinWidthM = 125000.0f;
	float BendAmplitudeM = 34000.0f;
	float BendWavelengthM = 310000.0f;
	float BendPhase = 0.0f;
	float UpliftSide = 1.0f;
};

struct FGeologicTarget
{
	float Relief01 = 0.5f;
	float Slope01 = 0.5f;
	float Anisotropy01 = 0.5f;
	float StructureAngleDeg = 0.0f;
	float MountainWeight = 0.0f;
	float BasinWeight = 0.0f;
	float ShoulderWeight = 0.0f;
};

float Smooth01(const float T)
{
	const float X = FMath::Clamp(T, 0.0f, 1.0f);
	return X * X * (3.0f - 2.0f * X);
}

float Bell01(const float Distance, const float Radius)
{
	return 1.0f - Smooth01(Distance / FMath::Max(1.0f, Radius));
}

float AngleDistance180(const float A, const float B)
{
	float D = FMath::Abs(FMath::Fmod(A - B, 180.0f));
	if (D > 90.0f) D = 180.0f - D;
	return D;
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

FVector2D TransformDirection(FVector2D Direction, const int32 Variant)
{
	if ((Variant & 1) != 0) Direction.X = -Direction.X;
	if ((Variant & 2) != 0) Direction.Y = -Direction.Y;
	switch ((Variant >> 2) & 3)
	{
	case 1: Direction = FVector2D(-Direction.Y, Direction.X); break;
	case 2: Direction = FVector2D(-Direction.X, -Direction.Y); break;
	case 3: Direction = FVector2D(Direction.Y, -Direction.X); break;
	default: break;
	}
	return Direction;
}

float TransformedStructureAngleDeg(const CubusDemIsland::FIndexedPatch& Metadata, const int32 Variant)
{
	const float AngleRad = FMath::DegreesToRadians(Metadata.DominantStructureAngleDeg);
	FVector2D Direction(FMath::Cos(AngleRad), FMath::Sin(AngleRad));
	Direction = TransformDirection(Direction, Variant);
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	while (Angle < 0.0f) Angle += 180.0f;
	while (Angle >= 180.0f) Angle -= 180.0f;
	return Angle;
}

int32 BestVariantForStructure(const CubusDemIsland::FIndexedPatch& Metadata, const float TargetAngleDeg, const int32 StableFallback)
{
	if (Metadata.DirectionalAnisotropy < 0.12f)
	{
		return StableFallback & 15;
	}

	int32 BestVariant = 0;
	float BestDifference = TNumericLimits<float>::Max();
	for (int32 Variant = 0; Variant < 16; ++Variant)
	{
		const float Difference = AngleDistance180(TransformedStructureAngleDeg(Metadata, Variant), TargetAngleDeg);
		if (Difference < BestDifference)
		{
			BestDifference = Difference;
			BestVariant = Variant;
		}
	}
	return BestVariant;
}

FGeologicPlan BuildGeologicPlan(const CubusDemIsland::FSettings& Settings)
{
	FRandomStream Random(Settings.Seed ^ 0x6a51c3d7);
	FGeologicPlan Plan;
	const float HalfWorld = static_cast<float>(Settings.WorldSizeMeters * 0.5);
	const float AxisAngle = Random.FRandRange(-PI, PI);
	Plan.Axis = FVector2D(FMath::Cos(AxisAngle), FMath::Sin(AxisAngle));
	Plan.Normal = FVector2D(-Plan.Axis.Y, Plan.Axis.X);
	Plan.HalfLengthM = HalfWorld * Random.FRandRange(0.82f, 0.94f);
	Plan.CoreHalfWidthM = HalfWorld * Random.FRandRange(0.115f, 0.165f);
	Plan.ShoulderWidthM = HalfWorld * Random.FRandRange(0.25f, 0.34f);
	Plan.BasinWidthM = HalfWorld * Random.FRandRange(0.42f, 0.56f);
	Plan.BendAmplitudeM = HalfWorld * Random.FRandRange(0.09f, 0.17f);
	Plan.BendWavelengthM = Settings.WorldSizeMeters * Random.FRandRange(0.52f, 0.78f);
	Plan.BendPhase = Random.FRandRange(-PI, PI);
	Plan.UpliftSide = Random.FRand() < 0.5f ? -1.0f : 1.0f;
	return Plan;
}

FGeologicTarget EvaluateGeology(const FGeologicPlan& Plan, const FVector2D& WorldM)
{
	const float Along = FVector2D::DotProduct(WorldM, Plan.Axis);
	const float Across = FVector2D::DotProduct(WorldM, Plan.Normal);
	const float Wave = Along / FMath::Max(1.0f, Plan.BendWavelengthM) * 2.0f * PI + Plan.BendPhase;
	const float SutureAcross =
		FMath::Sin(Wave) * Plan.BendAmplitudeM +
		FMath::Sin(Wave * 0.47f - 1.3f) * Plan.BendAmplitudeM * 0.34f;
	const float SignedAcross = Across - SutureAcross;
	const float DistanceToSuture = FMath::Abs(SignedAcross);

	// Derivative of the curved suture gives the local structural tangent. Real
	// directional DEMs are rotated/mirrored to follow this rather than receiving
	// an unrelated random orientation at every quilt cell.
	const float Derivative =
		FMath::Cos(Wave) * Plan.BendAmplitudeM * (2.0f * PI / FMath::Max(1.0f, Plan.BendWavelengthM)) +
		FMath::Cos(Wave * 0.47f - 1.3f) * Plan.BendAmplitudeM * 0.34f * 0.47f *
			(2.0f * PI / FMath::Max(1.0f, Plan.BendWavelengthM));
	FVector2D Tangent = Plan.Axis + Plan.Normal * Derivative;
	Tangent.Normalize();
	float StructureAngle = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));
	while (StructureAngle < 0.0f) StructureAngle += 180.0f;
	while (StructureAngle >= 180.0f) StructureAngle -= 180.0f;

	const float EndFade = Bell01(FMath::Abs(Along), Plan.HalfLengthM);
	const float Mountain = Bell01(DistanceToSuture, Plan.CoreHalfWidthM) * EndFade;

	// The two sides of a real convergent island are not mirror images. One side
	// gets an uplifted/rugged shoulder; the opposite side is allowed a broad
	// lower-relief foreland/basin before the coast.
	const float UpliftSigned = SignedAcross * Plan.UpliftSide;
	const float ShoulderCenter = Plan.CoreHalfWidthM + Plan.ShoulderWidthM * 0.43f;
	const float Shoulder =
		Bell01(FMath::Abs(UpliftSigned - ShoulderCenter), Plan.ShoulderWidthM * 0.62f) *
		Smooth01((UpliftSigned + Plan.CoreHalfWidthM * 0.25f) / FMath::Max(1.0f, Plan.CoreHalfWidthM)) * EndFade;

	const float BasinCenter = -(Plan.CoreHalfWidthM + Plan.BasinWidthM * 0.42f);
	const float Basin =
		Bell01(FMath::Abs(UpliftSigned - BasinCenter), Plan.BasinWidthM * 0.72f) *
		Smooth01((-UpliftSigned + Plan.CoreHalfWidthM * 0.20f) / FMath::Max(1.0f, Plan.CoreHalfWidthM)) * EndFade;

	// Longitudinal segmentation creates linked massifs and passes along one
	// continuous belt instead of a uniform ridge or unrelated mountain blobs.
	const float Segment = 0.72f + 0.28f * FMath::Square(FMath::Sin(
		Along / FMath::Max(1.0f, Plan.HalfLengthM) * 3.4f + Plan.BendPhase * 0.6f));

	FGeologicTarget Target;
	Target.MountainWeight = Mountain;
	Target.ShoulderWeight = Shoulder;
	Target.BasinWeight = Basin;
	Target.Relief01 = FMath::Clamp(
		0.13f + Mountain * 0.78f * Segment + Shoulder * 0.40f - Basin * 0.10f,
		0.05f, 0.98f);
	Target.Slope01 = FMath::Clamp(
		0.14f + Mountain * 0.72f + Shoulder * 0.34f - Basin * 0.08f,
		0.05f, 0.95f);
	Target.Anisotropy01 = FMath::Clamp(
		0.18f + Mountain * 0.62f + Shoulder * 0.34f,
		0.08f, 0.92f);
	Target.StructureAngleDeg = StructureAngle;
	return Target;
}

CubusLandscapeEvolution::EProvinceType MapProvinceType(const CubusDemIsland::FIndexedPatch& Entry)
{
	if (Entry.TerrainClass.Contains(TEXT("mountain"))) return CubusLandscapeEvolution::EProvinceType::FoldMountainBelt;
	if (Entry.TerrainClass.Contains(TEXT("coast"))) return CubusLandscapeEvolution::EProvinceType::CoastalShelf;
	if (Entry.TerrainClass.Contains(TEXT("plain")) || Entry.TerrainClass.Contains(TEXT("lowland"))) return CubusLandscapeEvolution::EProvinceType::SedimentaryBasin;
	if (Entry.TerrainClass.Contains(TEXT("rugged")) || Entry.TerrainClass.Contains(TEXT("dissected"))) return CubusLandscapeEvolution::EProvinceType::UpliftedPlateau;
	return CubusLandscapeEvolution::EProvinceType::StablePlain;
}

float SourceNeighbourCompatibility(
	const FSource& A,
	const int32 AVariant,
	const FSource& B,
	const int32 BVariant)
{
	const float MaxRelief = FMath::Max(250.0f, FMath::Max(A.Metadata.ReliefP90M, B.Metadata.ReliefP90M));
	const float ReliefSimilarity = 1.0f - FMath::Clamp(FMath::Abs(A.Metadata.ReliefP90M - B.Metadata.ReliefP90M) / MaxRelief, 0.0f, 1.0f);
	const float SlopeSimilarity = 1.0f - FMath::Clamp(FMath::Abs(A.Metadata.MeanSlopeDeg - B.Metadata.MeanSlopeDeg) / 28.0f, 0.0f, 1.0f);
	const float AnisotropySimilarity = 1.0f - FMath::Abs(A.Metadata.DirectionalAnisotropy - B.Metadata.DirectionalAnisotropy);
	const float StructureDifference = AngleDistance180(
		TransformedStructureAngleDeg(A.Metadata, AVariant),
		TransformedStructureAngleDeg(B.Metadata, BVariant));
	const float StructureSimilarity = 1.0f - StructureDifference / 90.0f;
	const float ClassContinuity = A.Metadata.TerrainClass == B.Metadata.TerrainClass ? 1.0f : 0.45f;
	const float RepeatPenalty = &A == &B ? 0.10f : 0.0f;
	return FMath::Clamp(
		ReliefSimilarity * 0.34f +
		SlopeSimilarity * 0.24f +
		AnisotropySimilarity * 0.14f +
		StructureSimilarity * 0.18f +
		ClassContinuity * 0.10f - RepeatPenalty,
		0.0f, 1.0f);
}

bool LoadLayer(
	const CubusDemIsland::FSettings& Settings,
	const TArray<CubusDemIsland::FIndexedPatch>& IndexEntries,
	const FGeologicPlan& GeologicPlan,
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

	Layer.GridSpacingM = FMath::Max(1000.0f, MeanExtentM * 0.62f);
	Layer.GridResolution = FMath::CeilToInt(Settings.WorldSizeMeters / Layer.GridSpacingM) + 4;
	const double HalfWorld = Settings.WorldSizeMeters * 0.5;
	Layer.GridOriginM = -HalfWorld - Layer.GridSpacingM * 1.5;
	Layer.Placements.SetNum(Layer.GridResolution * Layer.GridResolution);

	FRandomStream Random(Settings.Seed ^ Layer.SeedSalt);
	const float ReliefScaleM = FMath::Max(350.0f, Layer.MinimumExtentM * 0.012f);
	for (int32 Gy = 0; Gy < Layer.GridResolution; ++Gy)
	{
		for (int32 Gx = 0; Gx < Layer.GridResolution; ++Gx)
		{
			const FVector2D CenterM(
				Layer.GridOriginM + Gx * Layer.GridSpacingM,
				Layer.GridOriginM + Gy * Layer.GridSpacingM);
			const FGeologicTarget Target = EvaluateGeology(GeologicPlan, CenterM);

			int32 BestSource = 0;
			int32 BestVariant = 0;
			float BestScore = -TNumericLimits<float>::Max();
			for (int32 Candidate = 0; Candidate < Layer.Sources.Num(); ++Candidate)
			{
				const FSource& Source = Layer.Sources[Candidate];
				const int32 StableFallback =
					(Settings.Seed ^ Layer.SeedSalt ^ (Gx * 73856093) ^ (Gy * 19349663) ^ (Candidate * 83492791)) & 15;
				const int32 CandidateVariant = BestVariantForStructure(Source.Metadata, Target.StructureAngleDeg, StableFallback);
				const float Relief01 = FMath::Clamp(Source.Metadata.ReliefP90M / ReliefScaleM, 0.0f, 1.0f);
				const float Slope01 = FMath::Clamp(Source.Metadata.MeanSlopeDeg / 28.0f, 0.0f, 1.0f);
				const float ReliefMatch = 1.0f - FMath::Abs(Relief01 - Target.Relief01);
				const float SlopeMatch = 1.0f - FMath::Abs(Slope01 - Target.Slope01);
				const float AnisotropyMatch = 1.0f - FMath::Abs(Source.Metadata.DirectionalAnisotropy - Target.Anisotropy01);
				const float StructureMatch = 1.0f - AngleDistance180(
					TransformedStructureAngleDeg(Source.Metadata, CandidateVariant), Target.StructureAngleDeg) / 90.0f;

				float FormationFit = 0.5f;
				const bool bMountain = Source.Metadata.TerrainClass.Contains(TEXT("mountain"));
				const bool bRugged = Source.Metadata.TerrainClass.Contains(TEXT("rugged")) || Source.Metadata.TerrainClass.Contains(TEXT("dissected"));
				const bool bLowland = Source.Metadata.TerrainClass.Contains(TEXT("plain")) || Source.Metadata.TerrainClass.Contains(TEXT("lowland"));
				if (bMountain) FormationFit = FMath::Max(FormationFit, Target.MountainWeight);
				if (bRugged) FormationFit = FMath::Max(FormationFit, Target.ShoulderWeight);
				if (bLowland) FormationFit = FMath::Max(FormationFit, Target.BasinWeight);

				float NeighbourFit = 0.65f;
				int32 NeighbourCount = 0;
				if (Gx > 0)
				{
					const FPlacement& Left = Layer.Placements[Gy * Layer.GridResolution + (Gx - 1)];
					NeighbourFit += SourceNeighbourCompatibility(Source, CandidateVariant, Layer.Sources[Left.SourceIndex], Left.Variant);
					++NeighbourCount;
				}
				if (Gy > 0)
				{
					const FPlacement& Up = Layer.Placements[(Gy - 1) * Layer.GridResolution + Gx];
					NeighbourFit += SourceNeighbourCompatibility(Source, CandidateVariant, Layer.Sources[Up.SourceIndex], Up.Variant);
					++NeighbourCount;
				}
				if (NeighbourCount > 0) NeighbourFit /= static_cast<float>(NeighbourCount + 1);

				const float Suitability = Layer.Role == ELayerRole::PrimarySurface
					? Source.Metadata.MacroSuitability
					: Source.Metadata.RegionalSuitability;
				const float TinyJitter = Random.FRandRange(0.0f, 0.025f);
				const float Score =
					ReliefMatch * 0.27f +
					SlopeMatch * 0.14f +
					AnisotropyMatch * 0.10f +
					StructureMatch * 0.14f +
					FormationFit * 0.12f +
					NeighbourFit * 0.13f +
					Suitability * 0.075f +
					TinyJitter;
				if (Score > BestScore)
				{
					BestScore = Score;
					BestSource = Candidate;
					BestVariant = CandidateVariant;
				}
			}

			FPlacement& Placement = Layer.Placements[Gy * Layer.GridResolution + Gx];
			Placement.SourceIndex = BestSource;
			Placement.Variant = BestVariant;
			Placement.CenterM = CenterM;
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

float SourceHeightForRole(const FSource& Source, const FVector2D& SourceUv, const ELayerRole Role)
{
	const float Raw = Source.Patch.SampleBilinear(SourceUv.X, SourceUv.Y);
	if (Role == ELayerRole::PrimarySurface)
	{
		const float LowReference = Source.Metadata.ElevationP05M;
		const float HighReference = FMath::Max(Source.Metadata.ElevationP95M, LowReference + 1.0f);
		return FMath::Clamp(Raw, LowReference, HighReference) - LowReference;
	}

	const float Clipped = FMath::Clamp(Raw, Source.Metadata.ElevationP05M, Source.Metadata.ElevationP95M);
	return Clipped - Source.Metadata.MedianElevationM;
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

	float WeightedHeight = 0.0f;
	float WeightSum = 0.0f;
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
		const float Sample = SourceHeightForRole(Source, SourceUv, Layer.Role);
		WeightedHeight += Sample * W;
		WeightSum += W;
		if (W > BestWeight)
		{
			BestWeight = W;
			BestSource = Placement.SourceIndex;
		}
	}

	if (OutDominantSource) *OutDominantSource = BestSource;
	return WeightSum > KINDA_SMALL_NUMBER ? WeightedHeight / WeightSum : 0.0f;
}
} // namespace

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

	const FGeologicPlan GeologicPlan = BuildGeologicPlan(Settings);
	const float PlanAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(GeologicPlan.Axis.Y, GeologicPlan.Axis.X));
	UE_LOG(LogTemp, Display,
		TEXT("Cubus island geology plan: axis %.1f deg, half-length %.0f km, mountain core %.0f km, shoulder %.0f km, basin %.0f km, bend %.0f km"),
		PlanAngleDeg,
		GeologicPlan.HalfLengthM / 1000.0f,
		GeologicPlan.CoreHalfWidthM / 1000.0f,
		GeologicPlan.ShoulderWidthM / 1000.0f,
		GeologicPlan.BasinWidthM / 1000.0f,
		GeologicPlan.BendAmplitudeM / 1000.0f);

	FLayer Macro128;
	Macro128.FolderPrefix = TEXT("Macro128km/");
	Macro128.Role = ELayerRole::PrimarySurface;
	Macro128.Amplitude = 1.0f;
	Macro128.MinimumExtentM = 100000.0f;
	Macro128.MaximumExtentM = 160000.0f;
	Macro128.SeedSalt = 0x128128;

	FLayer Macro64;
	Macro64.FolderPrefix = TEXT("Macro64km/");
	Macro64.Role = ELayerRole::ResidualDetail;
	Macro64.Amplitude = 0.20f;
	Macro64.MinimumExtentM = 50000.0f;
	Macro64.MaximumExtentM = 85000.0f;
	Macro64.SeedSalt = 0x064064;

	FLayer Macro32;
	Macro32.FolderPrefix = TEXT("Macro32km/");
	Macro32.Role = ELayerRole::ResidualDetail;
	Macro32.Amplitude = 0.08f;
	Macro32.MinimumExtentM = 24000.0f;
	Macro32.MaximumExtentM = 43000.0f;
	Macro32.SeedSalt = 0x032032;

	if (!LoadLayer(Settings, IndexEntries, GeologicPlan, Macro128, OutError)) return false;
	if (!LoadLayer(Settings, IndexEntries, GeologicPlan, Macro64, OutError)) return false;
	if (!LoadLayer(Settings, IndexEntries, GeologicPlan, Macro32, OutError)) return false;

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
		const float Surface128 = SampleLayer(Macro128, WorldM, &Dominant128);
		const float Residual64 = SampleLayer(Macro64, WorldM);
		const float Residual32 = SampleLayer(Macro32, WorldM);
		OutResult.ReliefM[Cell] = Settings.ReliefScale * (
			Surface128 * Macro128.Amplitude +
			Residual64 * Macro64.Amplitude +
			Residual32 * Macro32.Amplitude);

		OutResult.ProvinceId[Cell] = static_cast<uint8>(MapProvinceType(Macro128.Sources[Dominant128].Metadata));
	});

	UE_LOG(LogTemp, Display,
		TEXT("Cubus geologically assembled DEM pyramid: %d x 128 km broad surfaces, %d x 64 km residuals, %d x 32 km residuals"),
		OutResult.Macro128SourceCount, OutResult.Macro64SourceCount, OutResult.Macro32SourceCount);
	return true;
}
} // namespace CubusDemPyramid
