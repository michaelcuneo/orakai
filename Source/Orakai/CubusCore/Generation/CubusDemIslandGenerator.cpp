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

struct FProvinceSource
{
	FIndexedPatch Metadata;
	FPatch Patch;
	FVector2D CenterM = FVector2D::ZeroVector;
	float RadiusM = 50000.0f;
	float RotationRad = 0.0f;
	float HeightScale = 1.0f;
	float Weight = 1.0f;
	uint8 ProvinceId = 0;
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

float IslandEnvelope(const double WorldX, const double WorldY, const FSettings& Settings)
{
	const double Half = Settings.WorldSizeMeters * 0.5;
	const double Margin = FMath::Clamp<double>(Settings.CoastBandM, 5000.0, Half * 0.35);
	const double BroadScale = FMath::Max(50000.0, Settings.WorldSizeMeters * 0.20);
	const float WarpX = Fbm(WorldX / BroadScale, WorldY / BroadScale, Settings.Seed ^ 0x51d7348d);
	const float WarpY = Fbm(WorldX / (BroadScale * 1.17), WorldY / (BroadScale * 1.17), Settings.Seed ^ 0x94d049bb);
	const double X = WorldX + WarpX * Settings.WarpMeters;
	const double Y = WorldY + WarpY * Settings.WarpMeters;
	const double Ax = Half - Margin * 0.25;
	const double Ay = Half - Margin * 0.38;
	const double Nx = FMath::Abs(X) / FMath::Max(1.0, Ax);
	const double Ny = FMath::Abs(Y) / FMath::Max(1.0, Ay);
	const double Super = FMath::Pow(FMath::Pow(Nx, 2.6) + FMath::Pow(Ny, 2.6), 1.0 / 2.6);
	const double DistanceM = (1.0 - Super) * FMath::Min(Ax, Ay);
	return Smooth01(static_cast<float>(DistanceM / Margin));
}

FVector2D Rotate(const FVector2D V, const float Radians)
{
	const float C = FMath::Cos(Radians);
	const float S = FMath::Sin(Radians);
	return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);
}

float SampleProvinceRelief(const FProvinceSource& Province, const FVector2D WorldM)
{
	const FVector2D Local = Rotate(WorldM - Province.CenterM, -Province.RotationRad);
	const float Diameter = FMath::Max(1.0f, Province.RadiusM * 2.0f);
	const float U = Local.X / Diameter + 0.5f;
	const float V = Local.Y / Diameter + 0.5f;
	if (U < 0.0f || U > 1.0f || V < 0.0f || V > 1.0f)
	{
		return 0.0f;
	}
	return (Province.Patch.SampleBilinear(U, V) - Province.Patch.MeanElevationM) * Province.HeightScale;
}

float ProvinceInfluence(const FProvinceSource& Province, const FVector2D WorldM, const float BlendM)
{
	const float Distance = FVector2D::Distance(WorldM, Province.CenterM);
	const float Inner = FMath::Max(1000.0f, Province.RadiusM - BlendM);
	if (Distance <= Inner)
	{
		return 1.0f;
	}
	if (Distance >= Province.RadiusM)
	{
		return 0.0f;
	}
	return 1.0f - Smooth01((Distance - Inner) / FMath::Max(1.0f, Province.RadiusM - Inner));
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
		const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
		if (Object->TryGetArrayField(TEXT("tags"), Tags) && Tags)
		{
			for (const TSharedPtr<FJsonValue>& Tag : *Tags) Entry.Tags.Add(Tag->AsString());
		}
		if (!Entry.RelativePath.IsEmpty()) OutEntries.Add(MoveTemp(Entry));
	}
	OutEntries.Sort([](const FIndexedPatch& A, const FIndexedPatch& B) { return A.MacroSuitability > B.MacroSuitability; });
	if (OutEntries.IsEmpty())
	{
		if (OutError) *OutError = TEXT("DEM terrain index contained zero usable patches.");
		return false;
	}
	return true;
}

bool FGenerator::Generate(const FSettings& Settings, CubusLandscapeEvolution::FGlobalDem& OutDem,
	CubusLandscapeEvolution::FGenerationStats* OutStats, FString* OutError)
{
	if (Settings.Resolution < 33 || Settings.WorldSizeMeters <= 100.0 || Settings.OceanFloorM >= Settings.OceanLevelM)
	{
		if (OutError) *OutError = TEXT("Invalid DEM world settings.");
		return false;
	}

	TArray<FIndexedPatch> IndexEntries;
	if (!LoadIndex(Settings, IndexEntries, OutError)) return false;

	FRandomStream Random(Settings.Seed);
	const int32 CandidateCount = FMath::Clamp(IndexEntries.Num(), 1, 64);
	const int32 WantedProvinceCount = FMath::Clamp(Settings.ProvinceCount, 4, 64);
	TArray<FProvinceSource> Provinces;
	Provinces.Reserve(WantedProvinceCount);
	TSet<FString> RecentlyUsedSources;
	const FString SourceRoot = ResolveSourceDirectory(Settings);
	const double Half = Settings.WorldSizeMeters * 0.5;

	for (int32 ProvinceIndex = 0; ProvinceIndex < WantedProvinceCount; ++ProvinceIndex)
	{
		int32 Pick = Random.RandRange(0, CandidateCount - 1);
		for (int32 Attempt = 0; Attempt < 12; ++Attempt)
		{
			const int32 Candidate = Random.RandRange(0, CandidateCount - 1);
			if (!RecentlyUsedSources.Contains(IndexEntries[Candidate].SourceKey) || Attempt == 11)
			{
				Pick = Candidate;
				break;
			}
		}
		const FIndexedPatch& Entry = IndexEntries[Pick];
		FProvinceSource Province;
		Province.Metadata = Entry;
		const FString PatchPath = FPaths::Combine(SourceRoot, Entry.RelativePath);
		if (!LoadPatch(PatchPath, Province.Patch, OutError)) return false;
		const float RadiusKm = Random.FRandRange(Settings.ProvinceMinRadiusKm, Settings.ProvinceMaxRadiusKm);
		Province.RadiusM = RadiusKm * 1000.0f;
		const float PlacementRadius = static_cast<float>(Half * 0.72);
		const float Angle = Random.FRandRange(0.0f, 2.0f * PI);
		const float Radial = FMath::Sqrt(Random.FRand()) * PlacementRadius;
		Province.CenterM = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radial;
		Province.RotationRad = FMath::DegreesToRadians(Entry.DominantStructureAngleDeg) + Random.FRandRange(-0.55f, 0.55f);
		const float ReliefTarget = FMath::Lerp(500.0f, 2600.0f, FMath::Clamp(Entry.MacroSuitability, 0.0f, 1.0f));
		Province.HeightScale = Settings.ReliefScale * ReliefTarget / FMath::Max(30.0f, Entry.ReliefP90M);
		Province.Weight = FMath::Lerp(0.65f, 1.0f, Entry.MacroSuitability);
		Province.ProvinceId = static_cast<uint8>(ProvinceIndex % 255);
		Provinces.Add(MoveTemp(Province));
		RecentlyUsedSources.Add(Entry.SourceKey);
		if (RecentlyUsedSources.Num() > 4) RecentlyUsedSources.Reset();
	}

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
	const float BlendM = Settings.ProvinceBlendKm * 1000.0f;
	ParallelFor(OutDem.NumCells(), [&OutDem, &Settings, &Provinces, R, Half, BlendM](const int32 Cell)
	{
		const int32 X = Cell % R;
		const int32 Y = Cell / R;
		const double U = static_cast<double>(X) / static_cast<double>(R - 1);
		const double V = static_cast<double>(Y) / static_cast<double>(R - 1);
		const FVector2D WorldM(-Half + U * Settings.WorldSizeMeters, -Half + V * Settings.WorldSizeMeters);
		const float Envelope = IslandEnvelope(WorldM.X, WorldM.Y, Settings);

		float WeightedRelief = 0.0f;
		float WeightSum = 0.0f;
		float BestWeight = 0.0f;
		int32 BestProvince = INDEX_NONE;
		for (int32 ProvinceIndex = 0; ProvinceIndex < Provinces.Num(); ++ProvinceIndex)
		{
			const FProvinceSource& Province = Provinces[ProvinceIndex];
			const float W = ProvinceInfluence(Province, WorldM, BlendM) * Province.Weight;
			if (W <= 0.0001f) continue;
			WeightedRelief += SampleProvinceRelief(Province, WorldM) * W;
			WeightSum += W;
			if (W > BestWeight)
			{
				BestWeight = W;
				BestProvince = ProvinceIndex;
			}
		}

		float Relief = WeightSum > 0.001f ? WeightedRelief / WeightSum : 0.0f;
		const float InteriorUndulation = Fbm(WorldM.X / 70000.0, WorldM.Y / 70000.0, Settings.Seed ^ 0x68bc21eb) * 120.0f;
		const float LandHeight = Settings.BaseLandElevationM + Relief + InteriorUndulation;
		const float Elevation = FMath::Lerp(Settings.OceanFloorM, LandHeight, Envelope);
		OutDem.ElevationM[Cell] = Elevation;

		if (Elevation <= Settings.OceanLevelM)
		{
			OutDem.ProvinceId[Cell] = static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::OceanicBasin);
		}
		else if (Envelope < 0.35f)
		{
			OutDem.ProvinceId[Cell] = static_cast<uint8>(CubusLandscapeEvolution::EProvinceType::CoastalShelf);
		}
		else if (BestProvince != INDEX_NONE)
		{
			OutDem.PlateId[Cell] = Provinces[BestProvince].ProvinceId;
			OutDem.ProvinceId[Cell] = static_cast<uint8>(MapProvinceType(Provinces[BestProvince].Metadata));
		}
	});

	Measure(OutDem, OutStats);
	UE_LOG(LogTemp, Display, TEXT("Cubus DEM province world: seed %d, %d provinces, %.0f km world, %.2f m/cell, %d indexed sources"),
		Settings.Seed, Provinces.Num(), Settings.WorldSizeMeters / 1000.0, OutDem.CellSizeMeters, IndexEntries.Num());
	return true;
}
} // namespace CubusDemIsland
