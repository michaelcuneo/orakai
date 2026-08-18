#include "CubusCore/Generation/CubusDemIslandGenerator.h"
#include "CubusCore/Generation/CubusDemPyramid.h"

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
constexpr int32 CoastKnotCount = 256;

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

/**
 * Closed polar coastline spline. The radii are not arbitrary bay/peninsula
 * primitives: they are extracted from the already-composed real DEM around the
 * outer part of the island, then smoothed into a continuous curve. Small
 * kilometre-scale roughness is added only after the real terrain has decided
 * the broad and medium coastline direction.
 */
struct FCoastShape
{
	TArray<float> RadiusM;
	float MeanRadiusM = 225000.0f;
	float MinimumRadiusM = 180000.0f;
	float MaximumRadiusM = 245000.0f;
	float FineRoughnessM = 4000.0f;
	float RotationRad = 0.0f;
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

float SamplePyramidReliefM(
	const CubusDemPyramid::FResult& Pyramid,
	const FSettings& Settings,
	const FVector2D& WorldM)
{
	const int32 R = Settings.Resolution;
	if (R < 2 || Pyramid.ReliefM.Num() != R * R)
	{
		return 0.0f;
	}

	const double Half = Settings.WorldSizeMeters * 0.5;
	const double U = FMath::Clamp((WorldM.X + Half) / Settings.WorldSizeMeters, 0.0, 1.0) * static_cast<double>(R - 1);
	const double V = FMath::Clamp((WorldM.Y + Half) / Settings.WorldSizeMeters, 0.0, 1.0) * static_cast<double>(R - 1);
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(U), 0, R - 1);
	const int32 Y0 = FMath::Clamp(FMath::FloorToInt(V), 0, R - 1);
	const int32 X1 = FMath::Min(X0 + 1, R - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, R - 1);
	const float Fx = static_cast<float>(U - X0);
	const float Fy = static_cast<float>(V - Y0);
	const float A = FMath::Lerp(Pyramid.ReliefM[Y0 * R + X0], Pyramid.ReliefM[Y0 * R + X1], Fx);
	const float B = FMath::Lerp(Pyramid.ReliefM[Y1 * R + X0], Pyramid.ReliefM[Y1 * R + X1], Fx);
	return FMath::Lerp(A, B, Fy);
}

void NormalizeSignal(TArray<float>& Signal)
{
	if (Signal.IsEmpty()) return;

	double Mean = 0.0;
	for (const float Value : Signal) Mean += Value;
	Mean /= Signal.Num();

	double Variance = 0.0;
	for (const float Value : Signal)
	{
		const double D = static_cast<double>(Value) - Mean;
		Variance += D * D;
	}
	const double StdDev = FMath::Sqrt(Variance / FMath::Max(1, Signal.Num()));
	const double Scale = FMath::Max(1.0, StdDev);
	for (float& Value : Signal)
	{
		Value = FMath::Clamp(static_cast<float>((Value - Mean) / Scale), -2.5f, 2.5f);
	}
}

TArray<float> SmoothCircularSignal(const TArray<float>& Input, const int32 Passes)
{
	TArray<float> Current = Input;
	TArray<float> Next;
	Next.SetNumUninitialized(Input.Num());
	const int32 Count = Input.Num();
	if (Count < 3) return Current;

	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		for (int32 I = 0; I < Count; ++I)
		{
			const int32 Prev = (I - 1 + Count) % Count;
			const int32 NextIndex = (I + 1) % Count;
			Next[I] = Current[Prev] * 0.22f + Current[I] * 0.56f + Current[NextIndex] * 0.22f;
		}
		Swap(Current, Next);
	}
	return Current;
}

float CatmullRom(const float P0, const float P1, const float P2, const float P3, const float T)
{
	const float T2 = T * T;
	const float T3 = T2 * T;
	return 0.5f * (
		2.0f * P1 +
		(-P0 + P2) * T +
		(2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * T2 +
		(-P0 + 3.0f * P1 - 3.0f * P2 + P3) * T3);
}

float CoastRadiusAtAngleM(const FCoastShape& Shape, float AngleRad)
{
	const int32 Count = Shape.RadiusM.Num();
	if (Count < 4) return Shape.MeanRadiusM;

	AngleRad -= Shape.RotationRad;
	while (AngleRad < 0.0f) AngleRad += 2.0f * PI;
	while (AngleRad >= 2.0f * PI) AngleRad -= 2.0f * PI;

	const float KnotPosition = AngleRad / (2.0f * PI) * static_cast<float>(Count);
	const int32 I1 = FMath::FloorToInt(KnotPosition) % Count;
	const float T = KnotPosition - static_cast<float>(FMath::FloorToInt(KnotPosition));
	const int32 I0 = (I1 - 1 + Count) % Count;
	const int32 I2 = (I1 + 1) % Count;
	const int32 I3 = (I1 + 2) % Count;
	return FMath::Clamp(
		CatmullRom(Shape.RadiusM[I0], Shape.RadiusM[I1], Shape.RadiusM[I2], Shape.RadiusM[I3], T),
		Shape.MinimumRadiusM,
		Shape.MaximumRadiusM);
}

FCoastShape BuildCoastShape(
	const FSettings& Settings,
	const CubusDemPyramid::FResult& Pyramid)
{
	FCoastShape Shape;
	FRandomStream Random(Settings.Seed ^ 0x4c8a91d3);
	const float Half = static_cast<float>(Settings.WorldSizeMeters * 0.5);

	// Keep the island large. Most seeds now occupy roughly 450-480 km of the
	// 500 km domain, while the DEM-driven curve is still free to pull real bays
	// and sounds substantially inward.
	Shape.MeanRadiusM = Half * Random.FRandRange(0.90f, 0.94f);
	Shape.MinimumRadiusM = Half * 0.70f;
	Shape.MaximumRadiusM = Half * 0.960f;
	Shape.FineRoughnessM = FMath::Clamp(Settings.WarpMeters * 0.36f, 2800.0f, 6000.0f);
	Shape.RotationRad = Random.FRandRange(-PI, PI);

	TArray<float> TerrainSignal;
	TArray<float> RadialTrendSignal;
	TerrainSignal.SetNumUninitialized(CoastKnotCount);
	RadialTrendSignal.SetNumUninitialized(CoastKnotCount);

	for (int32 I = 0; I < CoastKnotCount; ++I)
	{
		const float Angle = Shape.RotationRad + 2.0f * PI * static_cast<float>(I) / static_cast<float>(CoastKnotCount);
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));

		// Read a real terrain transect through the outer 30% of the island. The
		// weighted mean says whether this side is broadly high/low terrain; the
		// outward trend says whether the sampled landform is still climbing toward
		// the edge (headland/ridge) or falling away (bay/valley).
		const float R0 = Half * 0.70f;
		const float R1 = Half * 0.78f;
		const float R2 = Half * 0.86f;
		const float R3 = Half * 0.94f;
		const float Z0 = SamplePyramidReliefM(Pyramid, Settings, Direction * R0);
		const float Z1 = SamplePyramidReliefM(Pyramid, Settings, Direction * R1);
		const float Z2 = SamplePyramidReliefM(Pyramid, Settings, Direction * R2);
		const float Z3 = SamplePyramidReliefM(Pyramid, Settings, Direction * R3);

		TerrainSignal[I] = Z0 * 0.10f + Z1 * 0.20f + Z2 * 0.35f + Z3 * 0.35f;
		RadialTrendSignal[I] = (Z3 - Z2) * 0.55f + (Z2 - Z1) * 0.30f + (Z1 - Z0) * 0.15f;
	}

	NormalizeSignal(TerrainSignal);
	NormalizeSignal(RadialTrendSignal);

	// Three scales all come from the same real DEM signal. The broad component
	// carries long persistent curves; medium carries bays/headlands; the residual
	// keeps shorter bends without ever introducing a new straight primitive.
	TArray<float> Broad = SmoothCircularSignal(TerrainSignal, 22);
	TArray<float> Medium = SmoothCircularSignal(TerrainSignal, 7);
	TArray<float> Trend = SmoothCircularSignal(RadialTrendSignal, 5);
	NormalizeSignal(Broad);
	NormalizeSignal(Medium);
	NormalizeSignal(Trend);

	Shape.RadiusM.SetNumUninitialized(CoastKnotCount);
	float SumRadius = 0.0f;
	float MinRadius = TNumericLimits<float>::Max();
	float MaxRadius = TNumericLimits<float>::Lowest();
	for (int32 I = 0; I < CoastKnotCount; ++I)
	{
		const float BroadOffsetM = Broad[I] * Half * 0.070f;
		const float MediumOffsetM = (Medium[I] - Broad[I]) * Half * 0.048f;
		const float TrendOffsetM = Trend[I] * Half * 0.030f;
		const float ResidualOffsetM = (TerrainSignal[I] - Medium[I]) * Half * 0.012f;
		const float Radius = FMath::Clamp(
			Shape.MeanRadiusM + BroadOffsetM + MediumOffsetM + TrendOffsetM + ResidualOffsetM,
			Shape.MinimumRadiusM,
			Shape.MaximumRadiusM);
		Shape.RadiusM[I] = Radius;
		SumRadius += Radius;
		MinRadius = FMath::Min(MinRadius, Radius);
		MaxRadius = FMath::Max(MaxRadius, Radius);
	}
	Shape.MeanRadiusM = SumRadius / static_cast<float>(CoastKnotCount);
	Shape.MinimumRadiusM = MinRadius;
	Shape.MaximumRadiusM = MaxRadius;
	return Shape;
}

float CoastSignedDistanceM(const FVector2D& WorldM, const FSettings& Settings, const FCoastShape& Shape)
{
	const float RadiusM = static_cast<float>(WorldM.Size());
	const float AngleRad = FMath::Atan2(static_cast<float>(WorldM.Y), static_cast<float>(WorldM.X));
	const float CoastRadiusM = CoastRadiusAtAngleM(Shape, AngleRad);

	// The spline carries the geography. These are only small shoreline-scale
	// perturbations, so they roughen an existing curve rather than inventing
	// separate bays or chopping straight cuts through it.
	const float RoughnessScaleM = FMath::Max(1600.0f, static_cast<float>(Settings.WorldSizeMeters * 0.014));
	const float Fine = Fbm(
		WorldM.X / RoughnessScaleM,
		WorldM.Y / RoughnessScaleM,
		Settings.Seed ^ 0x51d7348d) * Shape.FineRoughnessM;
	const float Micro = Fbm(
		WorldM.X / (RoughnessScaleM * 0.38),
		WorldM.Y / (RoughnessScaleM * 0.38),
		Settings.Seed ^ 0x94d049bb) * Shape.FineRoughnessM * 0.34f;

	return CoastRadiusM + Fine + Micro - RadiusM;
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
} // namespace

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
		Entry.MinimumElevationM = ReadNumber(Object, TEXT("minimum_elevation_m"), 0.0f);
		Entry.MaximumElevationM = ReadNumber(Object, TEXT("maximum_elevation_m"), 0.0f);
		Entry.MeanElevationM = ReadNumber(Object, TEXT("mean_elevation_m"), 0.0f);
		Entry.MedianElevationM = ReadNumber(Object, TEXT("median_elevation_m"), Entry.MeanElevationM);
		Entry.ElevationP05M = ReadNumber(Object, TEXT("elevation_p05_m"), Entry.MinimumElevationM);
		Entry.ElevationP95M = ReadNumber(Object, TEXT("elevation_p95_m"), Entry.MaximumElevationM);
		Entry.ReliefP90M = ReadNumber(Object, TEXT("relief_p90_m"), 0.0f);
		Entry.MeanSlopeDeg = ReadNumber(Object, TEXT("mean_slope_deg"), 0.0f);
		Entry.DominantStructureAngleDeg = ReadNumber(Object, TEXT("dominant_structure_angle_deg"), 0.0f);
		Entry.DirectionalAnisotropy = ReadNumber(Object, TEXT("directional_anisotropy"), 0.0f);
		Entry.MacroSuitability = ReadNumber(Object, TEXT("macro_suitability"), 0.0f);
		Entry.RegionalSuitability = ReadNumber(Object, TEXT("regional_suitability"), 0.0f);
		Entry.LocalSuitability = ReadNumber(Object, TEXT("local_suitability"), 0.0f);
		if (!Entry.RelativePath.IsEmpty()) OutEntries.Add(MoveTemp(Entry));
	}
	if (OutEntries.IsEmpty())
	{
		if (OutError) *OutError = TEXT("DEM terrain index contains no usable patches.");
		return false;
	}
	return true;
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

	CubusDemPyramid::FResult Pyramid;
	if (!CubusDemPyramid::Compose(Settings, IndexEntries, Pyramid, OutError)) return false;

	OutDem.Reset();
	OutDem.Resolution = Settings.Resolution;
	OutDem.WorldSizeMeters = Settings.WorldSizeMeters;
	OutDem.CellSizeMeters = Settings.WorldSizeMeters / static_cast<double>(Settings.Resolution - 1);
	OutDem.OceanLevelM = Settings.OceanLevelM;
	const int32 CellCount = OutDem.NumCells();
	if (!Pyramid.IsValid(CellCount))
	{
		if (OutError) *OutError = TEXT("Multiscale DEM pyramid returned the wrong number of global cells.");
		return false;
	}
	OutDem.ElevationM.SetNumUninitialized(CellCount);
	OutDem.UpliftM.Init(0.0f, CellCount);
	OutDem.PlateId.Init(0, CellCount);
	OutDem.ProvinceId.Init(static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::StablePlain), CellCount);
	OutDem.BoundaryType.Init(static_cast<uint8>(CubusLandscapeEvolution::EBoundaryType::Stable), CellCount);

	const FCoastShape CoastShape = BuildCoastShape(Settings, Pyramid);
	const float CoastTransitionM = FMath::Clamp(Settings.CoastBandM * 0.18f, 2500.0f, static_cast<float>(Settings.WorldSizeMeters * 0.016));
	UE_LOG(LogTemp, Display,
		TEXT("Cubus terrain-following coastline: %d spline knots, radius min/mean/max %.0f / %.0f / %.0f km, fine roughness %.1f km, %.1f km shore transition"),
		CoastShape.RadiusM.Num(), CoastShape.MinimumRadiusM / 1000.0f, CoastShape.MeanRadiusM / 1000.0f,
		CoastShape.MaximumRadiusM / 1000.0f, CoastShape.FineRoughnessM / 1000.0f, CoastTransitionM / 1000.0f);

	const int32 R = OutDem.Resolution;
	const double HalfWorld = Settings.WorldSizeMeters * 0.5;
	ParallelFor(CellCount, [&OutDem, &Settings, &Pyramid, &CoastShape, R, HalfWorld, CoastTransitionM](const int32 Cell)
	{
		const int32 X = Cell % R;
		const int32 Y = Cell / R;
		const double U = static_cast<double>(X) / static_cast<double>(R - 1);
		const double V = static_cast<double>(Y) / static_cast<double>(R - 1);
		const FVector2D WorldM(-HalfWorld + U * Settings.WorldSizeMeters, -HalfWorld + V * Settings.WorldSizeMeters);
		const float CoastDistanceM = CoastSignedDistanceM(WorldM, Settings, CoastShape);

		float Elevation = Settings.OceanFloorM;
		if (CoastDistanceM >= 0.0f)
		{
			// Pyramid.ReliefM is a real broad land surface above a robust lowland
			// reference. The coastline follows that same composed real terrain instead
			// of intersecting it with separate geometric bay/capsule cuts.
			const float InteriorHeight = Settings.BaseLandElevationM + Pyramid.ReliefM[Cell];
			const float InlandT = Smooth01(CoastDistanceM / CoastTransitionM);
			Elevation = FMath::Lerp(Settings.OceanLevelM + 0.5f, InteriorHeight, InlandT);
		}
		else
		{
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
			OutDem.ProvinceId[Cell] = Pyramid.ProvinceId[Cell];
		}
	});

	Measure(OutDem, OutStats);
	if (OutStats)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Cubus raw 500 km multiscale DEM: elevation %.0f to %.0f m, mean land slope %.1f deg, steep land %.1f%%"),
			OutStats->MinimumElevationM, OutStats->MaximumElevationM, OutStats->MeanLandSlopeDegrees,
			OutStats->SteepLandFraction * 100.0f);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Cubus 500 km real-DEM world: seed %d, 128/64/32 km sources %d/%d/%d, %.2f m global cell"),
		Settings.Seed, Pyramid.Macro128SourceCount, Pyramid.Macro64SourceCount, Pyramid.Macro32SourceCount,
		OutDem.CellSizeMeters);
	return true;
}
} // namespace CubusDemIsland
