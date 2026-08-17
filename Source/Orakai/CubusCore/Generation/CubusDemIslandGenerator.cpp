#include "CubusCore/Generation/CubusDemIslandGenerator.h"

#include "Async/ParallelFor.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace CubusDemIsland
{
namespace
{
constexpr uint32 DemMagic = 0x4d454443u;
constexpr uint32 DemVersion = 1u;
constexpr float QuiltWeightPower = 2.0f;

struct FHeader
{
	uint32 Magic = DemMagic;
	uint32 Version = DemVersion;
	int32 Width = 0;
	int32 Height = 0;
	float CellSizeM = 1.0f;
	float MinimumElevationM = 0.0f;
	float MaximumElevationM = 0.0f;
	float MeanElevationM = 0.0f;
};

struct FMacroSource
{
	FIndexedPatch Metadata;
	FPatch Patch;
	float ExtentM = 0.0f;
};

struct FPlacement
{
	int32 SourceIndex = 0;
	int32 Variant = 0;
	FVector2D CenterM = FVector2D::ZeroVector;
};

struct FCoastFeature
{
	FVector2D A = FVector2D::ZeroVector;
	FVector2D B = FVector2D::ZeroVector;
	float RadiusM = 10000.0f;
	bool bAddsLand = false;
};

struct FCoastShape
{
	float AxisXM = 200000.0f;
	float AxisYM = 170000.0f;
	float Exponent = 2.4f;
	float RotationRad = 0.0f;
	int32 PeninsulaCount = 0;
	int32 BayCount = 0;
	int32 InletCount = 0;
	TArray<FCoastFeature> Features;
};

uint32 Hash(uint32 V)
{
	V ^= V >> 16;
	V *= 0x7feb352du;
	V ^= V >> 15;
	V *= 0x846ca68bu;
	V ^= V >> 16;
	return V;
}

float HashSigned(const int32 X, const int32 Y, const int32 Seed)
{
	const uint32 H = Hash(static_cast<uint32>(X) * 73856093u ^ static_cast<uint32>(Y) * 19349663u ^ static_cast<uint32>(Seed) * 83492791u);
	return static_cast<float>(H & 0x00ffffffu) / static_cast<float>(0x00ffffffu) * 2.0f - 1.0f;
}

float Smooth01(const float T)
{
	const float X = FMath::Clamp(T, 0.0f, 1.0f);
	return X * X * (3.0f - 2.0f * X);
}

float ValueNoise(const double X, const double Y, const int32 Seed)
{
	const int32 X0 = FMath::FloorToInt(X);
	const int32 Y0 = FMath::FloorToInt(Y);
	const float Fx = static_cast<float>(X - X0);
	const float Fy = static_cast<float>(Y - Y0);
	const float Sx = Smooth01(Fx);
	const float Sy = Smooth01(Fy);
	const float A = FMath::Lerp(HashSigned(X0, Y0, Seed), HashSigned(X0 + 1, Y0, Seed), Sx);
	const float B = FMath::Lerp(HashSigned(X0, Y0 + 1, Seed), HashSigned(X0 + 1, Y0 + 1, Seed), Sx);
	return FMath::Lerp(A, B, Sy);
}

float Fbm(double X, double Y, const int32 Seed)
{
	float Sum = 0.0f;
	float Weight = 0.0f;
	float Amp = 1.0f;
	for (int32 Octave = 0; Octave < 4; ++Octave)
	{
		Sum += ValueNoise(X, Y, Seed + Octave * 1013) * Amp;
		Weight += Amp;
		X = X * 2.03 + 17.1;
		Y = Y * 2.01 - 11.7;
		Amp *= 0.5f;
	}
	return Weight > 0.0f ? Sum / Weight : 0.0f;
}

FVector2D Rotate2D(const FVector2D& V, const float AngleRad)
{
	const float C = FMath::Cos(AngleRad);
	const float S = FMath::Sin(AngleRad);
	return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);
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

double SuperellipseRadiusM(const float AngleRad, const FCoastShape& Shape)
{
	const double C = FMath::Abs(FMath::Cos(AngleRad));
	const double S = FMath::Abs(FMath::Sin(AngleRad));
	const double P = Shape.Exponent;
	const double Denominator = FMath::Pow(
		FMath::Pow(C / FMath::Max(1.0f, Shape.AxisXM), P) +
		FMath::Pow(S / FMath::Max(1.0f, Shape.AxisYM), P),
		1.0 / P);
	return Denominator > UE_DOUBLE_SMALL_NUMBER ? 1.0 / Denominator : FMath::Min(Shape.AxisXM, Shape.AxisYM);
}

FVector2D ShorePointWorld(const float LocalAngleRad, const FCoastShape& Shape)
{
	const double R = SuperellipseRadiusM(LocalAngleRad, Shape);
	const FVector2D Local(FMath::Cos(LocalAngleRad) * R, FMath::Sin(LocalAngleRad) * R);
	return Rotate2D(Local, Shape.RotationRad);
}

FVector2D RadialWorld(const float LocalAngleRad, const FCoastShape& Shape)
{
	return Rotate2D(FVector2D(FMath::Cos(LocalAngleRad), FMath::Sin(LocalAngleRad)), Shape.RotationRad).GetSafeNormal();
}

float SignedCapsuleInsideM(const FVector2D& P, const FCoastFeature& Feature)
{
	const FVector2D Segment = Feature.B - Feature.A;
	const double SegmentLengthSq = Segment.SizeSquared();
	const double T = SegmentLengthSq > UE_DOUBLE_SMALL_NUMBER
		? FMath::Clamp(FVector2D::DotProduct(P - Feature.A, Segment) / SegmentLengthSq, 0.0, 1.0)
		: 0.0;
	const FVector2D Closest = Feature.A + Segment * T;
	return Feature.RadiusM - static_cast<float>(FVector2D::Distance(P, Closest));
}

FCoastShape BuildCoastShape(const FSettings& Settings)
{
	FCoastShape Shape;
	FRandomStream Random(Settings.Seed ^ 0x4c8a91d3);
	const float Half = static_cast<float>(Settings.WorldSizeMeters * 0.5);

	Shape.AxisXM = Half * Random.FRandRange(0.76f, 0.90f);
	Shape.AxisYM = Half * Random.FRandRange(0.62f, 0.82f);
	Shape.Exponent = Random.FRandRange(1.9f, 2.65f);
	Shape.RotationRad = Random.FRandRange(-PI, PI);
	Shape.PeninsulaCount = Random.RandRange(5, 8);
	Shape.BayCount = Random.RandRange(7, 11);
	Shape.InletCount = Random.RandRange(2, 5);
	Shape.Features.Reserve(Shape.PeninsulaCount + Shape.BayCount + Shape.InletCount);

	const auto AddDistributedFeatures = [&Shape, &Random](const int32 Count, const bool bAddsLand,
		const float MinLengthM, const float MaxLengthM, const float MinRadiusM, const float MaxRadiusM,
		const float PhaseOffset, const float MaxTangentialFraction)
	{
		if (Count <= 0) return;
		const float Sector = 2.0f * PI / static_cast<float>(Count);
		for (int32 I = 0; I < Count; ++I)
		{
			const float LocalAngle = PhaseOffset + (I + 0.5f) * Sector + Random.FRandRange(-0.34f, 0.34f) * Sector;
			const FVector2D Radial = RadialWorld(LocalAngle, Shape);
			const FVector2D Tangent(-Radial.Y, Radial.X);
			const FVector2D Shore = ShorePointWorld(LocalAngle, Shape);
			const float LengthM = Random.FRandRange(MinLengthM, MaxLengthM);
			const float RadiusM = Random.FRandRange(MinRadiusM, MaxRadiusM);
			const float BendM = Random.FRandRange(-MaxTangentialFraction, MaxTangentialFraction) * LengthM;

			FCoastFeature Feature;
			Feature.bAddsLand = bAddsLand;
			Feature.RadiusM = RadiusM;
			if (bAddsLand)
			{
				// Start well inside the parent landmass so the lobe is guaranteed to be
				// connected; bend the outer end along the coast to avoid radial starbursts.
				Feature.A = Shore - Radial * RadiusM * 1.7f;
				Feature.B = Shore + Radial * LengthM + Tangent * BendM;
			}
			else
			{
				// Water features always begin outside the parent coast, so every cut is
				// connected to the ocean rather than accidentally creating an inland lake.
				Feature.A = Shore + Radial * RadiusM * 1.8f;
				Feature.B = Shore - Radial * LengthM + Tangent * BendM;
			}
			Shape.Features.Add(Feature);
		}
	};

	const float Scale = Half / 250000.0f;
	AddDistributedFeatures(Shape.PeninsulaCount, true,
		22000.0f * Scale, 62000.0f * Scale, 8500.0f * Scale, 22000.0f * Scale,
		Random.FRandRange(0.0f, 2.0f * PI), 0.42f);
	AddDistributedFeatures(Shape.BayCount, false,
		18000.0f * Scale, 52000.0f * Scale, 10000.0f * Scale, 28000.0f * Scale,
		Random.FRandRange(0.0f, 2.0f * PI), 0.38f);
	AddDistributedFeatures(Shape.InletCount, false,
		42000.0f * Scale, 90000.0f * Scale, 4500.0f * Scale, 10500.0f * Scale,
		Random.FRandRange(0.0f, 2.0f * PI), 0.55f);

	return Shape;
}

float CoastSignedDistanceM(const FVector2D& WorldM, const FSettings& Settings, const FCoastShape& Shape)
{
	const double BroadScale = FMath::Max(60000.0, Settings.WorldSizeMeters * 0.17);
	const float WarpAmplitude = FMath::Clamp(Settings.WarpMeters, 0.0f, static_cast<float>(Settings.WorldSizeMeters * 0.04));
	const FVector2D Warped(
		WorldM.X + Fbm(WorldM.X / BroadScale, WorldM.Y / BroadScale, Settings.Seed ^ 0x51d7348d) * WarpAmplitude,
		WorldM.Y + Fbm(WorldM.X / (BroadScale * 1.13), WorldM.Y / (BroadScale * 1.13), Settings.Seed ^ 0x94d049bb) * WarpAmplitude);

	const FVector2D Local = Rotate2D(Warped, -Shape.RotationRad);
	const double P = Shape.Exponent;
	const double Super = FMath::Pow(
		FMath::Pow(FMath::Abs(Local.X) / FMath::Max(1.0f, Shape.AxisXM), P) +
		FMath::Pow(FMath::Abs(Local.Y) / FMath::Max(1.0f, Shape.AxisYM), P),
		1.0 / P);
	float DistanceM = static_cast<float>((1.0 - Super) * FMath::Min(Shape.AxisXM, Shape.AxisYM));

	// Broad and medium coastline undulation prevents untouched portions of the
	// parent landmass from reverting to a mathematically perfect superellipse.
	DistanceM += Fbm(WorldM.X / 85000.0, WorldM.Y / 85000.0, Settings.Seed ^ 0x7f4a7c15) * 15000.0f;
	DistanceM += Fbm(WorldM.X / 31000.0, WorldM.Y / 31000.0, Settings.Seed ^ 0x1ce4e5b9) * 4500.0f;

	for (const FCoastFeature& Feature : Shape.Features)
	{
		const float FeatureInsideM = SignedCapsuleInsideM(Warped, Feature);
		if (Feature.bAddsLand)
		{
			// SDF union: positive inside either the parent landmass or peninsula.
			DistanceM = FMath::Max(DistanceM, FeatureInsideM);
		}
		else
		{
			// SDF subtraction: positive inside the water capsule must become ocean.
			DistanceM = FMath::Min(DistanceM, -FeatureInsideM);
		}
	}
	return DistanceM;
}

CubusLandscapeEvolution::EProvinceType MapProvinceType(const FIndexedPatch& Entry)
{
	if (Entry.TerrainClass.Contains(TEXT("mountain"))) return CubusLandscapeEvolution::EProvinceType::FoldMountainBelt;
	if (Entry.TerrainClass.Contains(TEXT("coast"))) return CubusLandscapeEvolution::EProvinceType::CoastalShelf;
	if (Entry.TerrainClass.Contains(TEXT("plain")) || Entry.TerrainClass.Contains(TEXT("lowland"))) return CubusLandscapeEvolution::EProvinceType::SedimentaryBasin;
	if (Entry.TerrainClass.Contains(TEXT("rugged")) || Entry.TerrainClass.Contains(TEXT("dissected"))) return CubusLandscapeEvolution::EProvinceType::UpliftedPlateau;
	return CubusLandscapeEvolution::EProvinceType::StablePlain;
}

float ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, const float DefaultValue)
{
	double Value = 0.0;
	return Object.IsValid() && Object->TryGetNumberField(Name, Value) ? static_cast<float>(Value) : DefaultValue;
}

FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
{
	FString Value;
	if (Object.IsValid()) Object->TryGetStringField(Name, Value);
	return Value;
}

void Measure(const CubusLandscapeEvolution::FGlobalDem& Dem, CubusLandscapeEvolution::FGenerationStats* Stats)
{
	if (!Stats) return;
	Stats->CellCount = Dem.NumCells();
	Stats->MinimumElevationM = TNumericLimits<float>::Max();
	Stats->MaximumElevationM = TNumericLimits<float>::Lowest();
	for (const float E : Dem.ElevationM)
	{
		Stats->MinimumElevationM = FMath::Min(Stats->MinimumElevationM, E);
		Stats->MaximumElevationM = FMath::Max(Stats->MaximumElevationM, E);
	}
	CubusLandscapeEvolution::FGenerator::MeasureQuality(Dem, *Stats);
}
}

bool FPatch::IsValid() const
{
	return Width > 1 && Height > 1 && ElevationM.Num() == Width * Height && CellSizeM > 0.0f;
}

float FPatch::SampleBilinear(const float InU, const float InV) const
{
	if (!IsValid()) return 0.0f;
	const float U = FMath::Clamp(InU, 0.0f, 1.0f) * static_cast<float>(Width - 1);
	const float V = FMath::Clamp(InV, 0.0f, 1.0f) * static_cast<float>(Height - 1);
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(U), 0, Width - 1);
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt(V), 0, Height - 1);
	const int32 X1 = FMath::Min(X0 + 1, Width - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
	const float Fx = U - X0;
	const float Fy = V - Y0;
	const float A = FMath::Lerp(ElevationM[Y0 * Width + X0], ElevationM[Y0 * Width + X1], Fx);
	const float B = FMath::Lerp(ElevationM[Y1 * Width + X0], ElevationM[Y1 * Width + X1], Fx);
	return FMath::Lerp(A, B, Fy);
}

FString FGenerator::ResolveSourceDirectory(const FSettings& Settings)
{
	return FPaths::IsRelative(Settings.SourceDirectory)
		? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), Settings.SourceDirectory))
		: FPaths::ConvertRelativePathToFull(Settings.SourceDirectory);
}

bool FGenerator::DiscoverPatches(const FSettings& Settings, TArray<FString>& OutPatchPaths, FString* OutError)
{
	OutPatchPaths.Reset();
	const FString Directory = ResolveSourceDirectory(Settings);
	IFileManager::Get().FindFilesRecursive(OutPatchPaths, *Directory, TEXT("*.cdem"), true, false, false);
	OutPatchPaths.Sort();
	if (OutPatchPaths.IsEmpty())
	{
		if (OutError) *OutError = FString::Printf(TEXT("No prepared DEM patches found in %s."), *Directory);
		return false;
	}
	return true;
}

bool FGenerator::LoadPatch(const FString& Path, FPatch& OutPatch, FString* OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		if (OutError) *OutError = FString::Printf(TEXT("Could not read DEM patch: %s"), *Path);
		return false;
	}
	if (Bytes.Num() < static_cast<int32>(sizeof(FHeader)))
	{
		if (OutError) *OutError = FString::Printf(TEXT("DEM patch header is truncated: %s"), *Path);
		return false;
	}
	FHeader Header;
	FMemory::Memcpy(&Header, Bytes.GetData(), sizeof(FHeader));
	if (Header.Magic != DemMagic || Header.Version != DemVersion || Header.Width < 2 || Header.Height < 2 || Header.CellSizeM <= 0.0f)
	{
		if (OutError) *OutError = FString::Printf(TEXT("Unsupported/corrupt DEM patch header: %s"), *Path);
		return false;
	}
	const int64 SampleCount = static_cast<int64>(Header.Width) * static_cast<int64>(Header.Height);
	const int64 ExpectedBytes = static_cast<int64>(sizeof(FHeader)) + SampleCount * static_cast<int64>(sizeof(float));
	if (ExpectedBytes != Bytes.Num())
	{
		if (OutError) *OutError = FString::Printf(TEXT("DEM patch has wrong byte count: %s"), *Path);
		return false;
	}
	OutPatch.SourcePath = Path;
	OutPatch.Width = Header.Width;
	OutPatch.Height = Header.Height;
	OutPatch.CellSizeM = Header.CellSizeM;
	OutPatch.MinimumElevationM = Header.MinimumElevationM;
	OutPatch.MaximumElevationM = Header.MaximumElevationM;
	OutPatch.MeanElevationM = Header.MeanElevationM;
	OutPatch.ElevationM.SetNumUninitialized(static_cast<int32>(SampleCount));
	FMemory::Memcpy(OutPatch.ElevationM.GetData(), Bytes.GetData() + sizeof(FHeader), SampleCount * sizeof(float));
	return true;
}

bool FGenerator::LoadIndex(const FSettings& Settings, TArray<FIndexedPatch>& OutEntries, FString* OutError)
{
	OutEntries.Reset();
	const FString IndexPath = FPaths::Combine(ResolveSourceDirectory(Settings), Settings.IndexFileName);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *IndexPath))
	{
		if (OutError) *OutError = FString::Printf(TEXT("DEM terrain index not found: %s. Run analyze_dem_library.py."), *IndexPath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		if (OutError) *OutError = FString::Printf(TEXT("Could not parse DEM terrain index: %s"), *IndexPath);
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Patches = nullptr;
	if (!Root->TryGetArrayField(TEXT("patches"), Patches) || Patches == nullptr)
	{
		if (OutError) *OutError = TEXT("DEM terrain index has no patches array.");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Patches)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Object.IsValid()) continue;
		FIndexedPatch Entry;
		Entry.RelativePath = ReadString(Object, TEXT("path"));
		Entry.SourceKey = ReadString(Object, TEXT("source_key"));
		Entry.TerrainClass = ReadString(Object, TEXT("terrain_class"));
		Entry.ReliefP90M = ReadNumber(Object, TEXT("relief_p90_m"), 0.0f);
		Entry.MeanSlopeDeg = ReadNumber(Object, TEXT("mean_slope_deg"), 0.0f);
		Entry.DominantStructureAngleDeg = ReadNumber(Object, TEXT("dominant_structure_angle_deg"), 0.0f);
		Entry.DirectionalAnisotropy = ReadNumber(Object, TEXT("directional_anisotropy"), 0.0f);
		Entry.MacroSuitability = ReadNumber(Object, TEXT("macro_suitability"), 0.0f);
		Entry.RegionalSuitability = ReadNumber(Object, TEXT("regional_suitability"), 0.0f);
		Entry.LocalSuitability = ReadNumber(Object, TEXT("local_suitability"), 0.0f);
		if (!Entry.RelativePath.IsEmpty()) OutEntries.Add(MoveTemp(Entry));
	}
	return !OutEntries.IsEmpty();
}

bool FGenerator::Generate(const FSettings& Settings, CubusLandscapeEvolution::FGlobalDem& OutDem,
	CubusLandscapeEvolution::FGenerationStats* OutStats, FString* OutError)
{
	if (Settings.Resolution < 33 || Settings.WorldSizeMeters <= 10000.0 || Settings.OceanFloorM >= Settings.OceanLevelM)
	{
		if (OutError) *OutError = TEXT("500 km DEM world requires valid production-scale settings.");
		return false;
	}

	TArray<FIndexedPatch> IndexEntries;
	if (!LoadIndex(Settings, IndexEntries, OutError)) return false;

	TArray<FMacroSource> Sources;
	const FString SourceRoot = ResolveSourceDirectory(Settings);
	for (const FIndexedPatch& Entry : IndexEntries)
	{
		if (!Entry.RelativePath.StartsWith(TEXT("Macro/")) && Entry.SourceKey != TEXT("macro_nz_8m")) continue;
		FMacroSource Source;
		Source.Metadata = Entry;
		if (!LoadPatch(FPaths::Combine(SourceRoot, Entry.RelativePath), Source.Patch, OutError)) return false;
		Source.ExtentM = FMath::Min((Source.Patch.Width - 1) * Source.Patch.CellSizeM, (Source.Patch.Height - 1) * Source.Patch.CellSizeM);
		if (Source.ExtentM > 10000.0f) Sources.Add(MoveTemp(Source));
	}
	if (Sources.IsEmpty())
	{
		if (OutError) *OutError = TEXT("No macro DEM patches found. Run Tools/DemLibrary/prepare_macro_dem_library.py first.");
		return false;
	}

	float MeanExtentM = 0.0f;
	float MinimumSourceReliefM = TNumericLimits<float>::Max();
	float MaximumSourceReliefM = 0.0f;
	float MeanSourceReliefM = 0.0f;
	for (const FMacroSource& Source : Sources)
	{
		MeanExtentM += Source.ExtentM;
		MinimumSourceReliefM = FMath::Min(MinimumSourceReliefM, Source.Metadata.ReliefP90M);
		MaximumSourceReliefM = FMath::Max(MaximumSourceReliefM, Source.Metadata.ReliefP90M);
		MeanSourceReliefM += Source.Metadata.ReliefP90M;
	}
	MeanExtentM /= Sources.Num();
	MeanSourceReliefM /= Sources.Num();
	const float GridSpacingM = FMath::Clamp(MeanExtentM * 0.72f, 18000.0f, 42000.0f);
	const int32 GridR = FMath::CeilToInt(Settings.WorldSizeMeters / GridSpacingM) + 3;
	const double HalfWorld = Settings.WorldSizeMeters * 0.5;
	const double GridOrigin = -HalfWorld - GridSpacingM;

	UE_LOG(LogTemp, Display,
		TEXT("Cubus macro DEM library: %d sources, mean extent %.1f km, P90 relief min/mean/max %.0f / %.0f / %.0f m, quilt spacing %.1f km"),
		Sources.Num(), MeanExtentM / 1000.0f, MinimumSourceReliefM, MeanSourceReliefM, MaximumSourceReliefM, GridSpacingM / 1000.0f);
	if (MaximumSourceReliefM < 800.0f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Cubus macro DEM library contains no source patch above 800 m P90 relief; the source library itself is low-relief."));
	}

	TArray<FPlacement> Placements;
	Placements.SetNum(GridR * GridR);
	FRandomStream Random(Settings.Seed);
	for (int32 Gy = 0; Gy < GridR; ++Gy)
	{
		for (int32 Gx = 0; Gx < GridR; ++Gx)
		{
			const FVector2D Center(GridOrigin + Gx * GridSpacingM, GridOrigin + Gy * GridSpacingM);
			const float Radial01 = FMath::Clamp(static_cast<float>(Center.Size() / HalfWorld), 0.0f, 1.0f);
			const float DesiredRelief01 = FMath::Clamp(1.15f - Radial01 + Random.FRandRange(-0.20f, 0.20f), 0.0f, 1.0f);
			int32 BestSource = 0;
			float BestScore = -TNumericLimits<float>::Max();
			for (int32 Candidate = 0; Candidate < Sources.Num(); ++Candidate)
			{
				const FMacroSource& Source = Sources[Candidate];
				const float Relief01 = FMath::Clamp(Source.Metadata.ReliefP90M / 1800.0f, 0.0f, 1.0f);
				const float ReliefMatch = 1.0f - FMath::Abs(Relief01 - DesiredRelief01);
				const float Score = ReliefMatch * 0.55f + Source.Metadata.MacroSuitability * 0.35f + Random.FRandRange(0.0f, 0.10f);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestSource = Candidate;
				}
			}
			FPlacement& Placement = Placements[Gy * GridR + Gx];
			Placement.SourceIndex = BestSource;
			Placement.Variant = Random.RandRange(0, 15);
			Placement.CenterM = Center;
		}
	}

	const FCoastShape CoastShape = BuildCoastShape(Settings);
	const float CoastTransitionM = FMath::Clamp(Settings.CoastBandM, 3500.0f, static_cast<float>(Settings.WorldSizeMeters * 0.03));
	UE_LOG(LogTemp, Display,
		TEXT("Cubus coastline: %.0f x %.0f km parent axes, rotation %.0f deg, %d peninsulas, %d bays, %d deep inlets, %.1f km shore transition"),
		CoastShape.AxisXM * 2.0f / 1000.0f, CoastShape.AxisYM * 2.0f / 1000.0f,
		FMath::RadiansToDegrees(CoastShape.RotationRad), CoastShape.PeninsulaCount, CoastShape.BayCount,
		CoastShape.InletCount, CoastTransitionM / 1000.0f);

	OutDem.Reset();
	OutDem.Resolution = Settings.Resolution;
	OutDem.WorldSizeMeters = Settings.WorldSizeMeters;
	OutDem.CellSizeMeters = Settings.WorldSizeMeters / static_cast<double>(Settings.Resolution - 1);
	OutDem.OceanLevelM = Settings.OceanLevelM;
	OutDem.ElevationM.SetNumUninitialized(OutDem.NumCells());
	OutDem.UpliftM.Init(0.0f, OutDem.NumCells());
	OutDem.PlateId.Init(0, OutDem.NumCells());
	OutDem.ProvinceId.Init(static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::StablePlain), OutDem.NumCells());
	OutDem.BoundaryType.Init(static_cast<uint8>(CubusLandscapeEvolution::EBoundaryType::Stable), OutDem.NumCells());

	const int32 R = OutDem.Resolution;
	ParallelFor(OutDem.NumCells(), [&OutDem, &Settings, &Sources, &Placements, &CoastShape, R, GridR, GridSpacingM, GridOrigin, CoastTransitionM](const int32 Cell)
	{
		const int32 X = Cell % R;
		const int32 Y = Cell / R;
		const double UWorld = static_cast<double>(X) / static_cast<double>(R - 1);
		const double VWorld = static_cast<double>(Y) / static_cast<double>(R - 1);
		const double Half = Settings.WorldSizeMeters * 0.5;
		const FVector2D WorldM(-Half + UWorld * Settings.WorldSizeMeters, -Half + VWorld * Settings.WorldSizeMeters);

		const double Gx = (WorldM.X - GridOrigin) / GridSpacingM;
		const double Gy = (WorldM.Y - GridOrigin) / GridSpacingM;
		const int32 X0 = FMath::Clamp(FMath::FloorToInt(Gx), 0, GridR - 2);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Gy), 0, GridR - 2);
		const float Fx = Smooth01(static_cast<float>(Gx - X0));
		const float Fy = Smooth01(static_cast<float>(Gy - Y0));
		const int32 PlacementIndices[4] = {Y0 * GridR + X0, Y0 * GridR + X0 + 1, (Y0 + 1) * GridR + X0, (Y0 + 1) * GridR + X0 + 1};
		const float BaseWeights[4] = {(1.0f - Fx) * (1.0f - Fy), Fx * (1.0f - Fy), (1.0f - Fx) * Fy, Fx * Fy};

		float WeightedRelief = 0.0f;
		float WeightSquaredSum = 0.0f;
		float BestWeight = -1.0f;
		int32 BestPlacement = PlacementIndices[0];
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const FPlacement& Placement = Placements[PlacementIndices[Corner]];
			const FMacroSource& Source = Sources[Placement.SourceIndex];
			const float SampleExtentM = FMath::Max(Source.ExtentM, GridSpacingM * 1.35f);
			const FVector2D Local = WorldM - Placement.CenterM;
			FVector2D SourceUv(Local.X / SampleExtentM + 0.5f, Local.Y / SampleExtentM + 0.5f);
			SourceUv = TransformUv(SourceUv, Placement.Variant);
			if (SourceUv.X < 0.0f || SourceUv.X > 1.0f || SourceUv.Y < 0.0f || SourceUv.Y > 1.0f) continue;

			const float W = FMath::Pow(FMath::Max(0.0f, BaseWeights[Corner]), QuiltWeightPower);
			if (W <= KINDA_SMALL_NUMBER) continue;
			const float Sample = (Source.Patch.SampleBilinear(SourceUv.X, SourceUv.Y) - Source.Patch.MeanElevationM) * Settings.ReliefScale;
			WeightedRelief += Sample * W;
			WeightSquaredSum += W * W;
			if (W > BestWeight)
			{
				BestWeight = W;
				BestPlacement = PlacementIndices[Corner];
			}
		}
		const float Relief = WeightSquaredSum > KINDA_SMALL_NUMBER
			? WeightedRelief / FMath::Sqrt(WeightSquaredSum)
			: 0.0f;

		const float CoastDistanceM = CoastSignedDistanceM(WorldM, Settings, CoastShape);
		float Elevation = Settings.OceanFloorM;
		if (CoastDistanceM >= 0.0f)
		{
			// The SDF zero contour is now the actual shore. Land rises from just above
			// sea level into the real-DEM interior instead of inheriting its coastline
			// from a huge ocean-floor interpolation band.
			const float InteriorHeight = FMath::Max(Settings.OceanLevelM + 2.0f, Settings.BaseLandElevationM + Relief);
			const float InlandT = Smooth01(CoastDistanceM / CoastTransitionM);
			Elevation = FMath::Lerp(Settings.OceanLevelM + 0.5f, InteriorHeight, InlandT);
		}
		else
		{
			// Keep a broad submerged shelf without allowing that shelf to round off the
			// coastline itself. The shoreline stays at the SDF zero contour.
			const float ShelfT = Smooth01((-CoastDistanceM) / (CoastTransitionM * 3.0f));
			Elevation = FMath::Lerp(Settings.OceanLevelM - 1.0f, Settings.OceanFloorM, ShelfT);
		}
		OutDem.ElevationM[Cell] = Elevation;

		if (CoastDistanceM < 0.0f)
		{
			OutDem.ProvinceId[Cell] = static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::OceanicBasin);
		}
		else if (CoastDistanceM < CoastTransitionM * 1.5f)
		{
			OutDem.ProvinceId[Cell] = static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::CoastalShelf);
		}
		else
		{
			const FPlacement& Placement = Placements[BestPlacement];
			OutDem.PlateId[Cell] = static_cast<uint8>(BestPlacement % 255);
			OutDem.ProvinceId[Cell] = static_cast<uint8>(MapProvinceType(Sources[Placement.SourceIndex].Metadata));
		}
	});

	Measure(OutDem, OutStats);
	if (OutStats)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Cubus raw 500 km DEM: elevation %.0f to %.0f m, mean land slope %.1f deg, steep land %.1f%%"),
			OutStats->MinimumElevationM, OutStats->MaximumElevationM, OutStats->MeanLandSlopeDegrees,
			OutStats->SteepLandFraction * 100.0f);
	}
	UE_LOG(LogTemp, Display, TEXT("Cubus 500 km DEM quilt: seed %d, %d macro sources, %d x %d placements, %.2f m/cell"),
		Settings.Seed, Sources.Num(), GridR, GridR, OutDem.CellSizeMeters);
	return true;
}
} // namespace CubusDemIsland
